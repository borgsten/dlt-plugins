#include "dlt_proxy/proxy.h"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <format>
#include <initializer_list>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <span>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <system_error>
#include <unistd.h>
#include <utility>

namespace dlt_proxy {

namespace {

using namespace std::chrono_literals;
using Clock = std::chrono::steady_clock;

// DLT v1: a standard header (htyp, mcnt, big endian len), optional ECU ID,
// session ID and timestamp, then an optional extended header (msin, noar,
// apid, ctid).
constexpr std::size_t kStandardHeaderSize = 4;
constexpr std::size_t kOptionalFieldSize = 4;
constexpr std::size_t kExtendedHeaderSize = 10;
constexpr std::size_t kApidOffset = 2;
constexpr std::uint8_t kUseExtendedHeader = 0x01;
constexpr std::uint8_t kWithEcuId = 0x04;
constexpr std::uint8_t kWithSessionId = 0x08;
constexpr std::uint8_t kWithTimestamp = 0x10;
constexpr std::uint8_t kVersionMask = 0xe0;
constexpr std::uint8_t kVersion1 = 0x20;

constexpr std::size_t kReadChunk = std::size_t{64} * 1024;
// Read at most this much per poll() so clients get written to in between.
constexpr std::size_t kMaxReadPerPoll = std::size_t{1024} * 1024;
constexpr std::size_t kMaxClientBacklog = std::size_t{16} * 1024 * 1024;
constexpr auto kReconnectDelay = 1s;

enum class FrameKind : std::uint8_t { Incomplete, Complete, Invalid };

struct Frame {
    FrameKind kind = FrameKind::Incomplete;
    std::size_t size = 0;
    /* Only messages with an extended header have one */
    std::optional<Id> appId = std::nullopt;
};

Frame inspectFrame(std::span<const std::uint8_t> data) {
    std::array<std::uint8_t, kStandardHeaderSize> header{};
    if (data.size() < header.size()) {
        return {.kind = FrameKind::Incomplete};
    }
    std::ranges::copy(data.first(header.size()), header.begin());
    const std::uint8_t htyp = header.at(0);
    if ((htyp & kVersionMask) != kVersion1) {
        return {.kind = FrameKind::Invalid};
    }

    const std::size_t length = (std::size_t{header.at(2)} << 8U) | header.at(3);
    std::size_t extendedHeader = kStandardHeaderSize;
    for (const std::uint8_t flag : {kWithEcuId, kWithSessionId, kWithTimestamp}) {
        if ((htyp & flag) != 0) {
            extendedHeader += kOptionalFieldSize;
        }
    }
    if (length < extendedHeader) {
        return {.kind = FrameKind::Invalid};
    }
    if (data.size() < length) {
        return {.kind = FrameKind::Incomplete};
    }
    if ((htyp & kUseExtendedHeader) == 0 || length < extendedHeader + kExtendedHeaderSize) {
        return {.kind = FrameKind::Complete, .size = length};
    }

    Id appId{};
    std::ranges::copy(data.subspan(extendedHeader + kApidOffset, appId.size()), appId.begin());
    return {.kind = FrameKind::Complete, .size = length, .appId = appId};
}

class FileDescriptor {
 public:
    FileDescriptor() = default;
    explicit FileDescriptor(int descriptor) : m_fd(descriptor) {}
    FileDescriptor(const FileDescriptor&) = delete;
    FileDescriptor& operator=(const FileDescriptor&) = delete;
    FileDescriptor(FileDescriptor&& other) noexcept : m_fd(std::exchange(other.m_fd, -1)) {}
    FileDescriptor& operator=(FileDescriptor&& other) noexcept {
        std::swap(m_fd, other.m_fd);
        return *this;
    }
    ~FileDescriptor() { reset(); }

    [[nodiscard]] int fd() const { return m_fd; }
    [[nodiscard]] bool isOpen() const { return m_fd >= 0; }
    void reset() {
        if (m_fd >= 0) {
            ::close(m_fd);
            m_fd = -1;
        }
    }

 private:
    int m_fd = -1;
};

struct Client {
    FileDescriptor socket;
    std::vector<std::uint8_t> output;
    std::size_t outputStart = 0;
    bool closed = false;
};

bool wouldBlock() { return errno == EAGAIN || errno == EWOULDBLOCK; }

std::string lastError() { return std::system_category().message(errno); }

// poll() flags are signed ints; combine and test them as unsigned.
short pollFlags(std::initializer_list<int> flags) {
    unsigned combined = 0;
    for (const int flag : flags) {
        combined |= static_cast<unsigned>(flag);
    }
    return static_cast<short>(combined);
}

bool hasAny(short revents, std::initializer_list<int> flags) {
    return (static_cast<unsigned>(revents) & static_cast<unsigned>(pollFlags(flags))) != 0;
}

}  // namespace

std::optional<Id> toId(std::string_view text) {
    Id result{};
    if (text.empty() || text.size() > result.size()) {
        return std::nullopt;
    }
    std::ranges::copy(text, result.begin());
    return result;
}

class Proxy::Impl {
 public:
    explicit Impl(Options options) : m_options(std::move(options)) {}

    void run() {
        std::vector<pollfd> fds;
        while (m_running) {
            if (!m_source.isOpen() && Clock::now() >= m_nextConnect) {
                startConnect();
                if (!m_running) {
                    break;
                }
            }

            // poll() ignores negative descriptors, which keeps the first
            // three entries in place while the listener or source is closed.
            fds.clear();
            fds.push_back({.fd = m_stop.fd(), .events = POLLIN, .revents = 0});
            fds.push_back({.fd = m_listener.fd(), .events = POLLIN, .revents = 0});
            fds.push_back({
                .fd = m_source.fd(),
                .events = static_cast<short>(m_connecting ? POLLOUT : POLLIN),
                .revents = 0,
            });
            for (const auto& client : m_clients) {
                const bool pending = client->outputStart < client->output.size();
                fds.push_back({
                    .fd = client->socket.fd(),
                    .events = pending ? pollFlags({POLLIN, POLLOUT}) : pollFlags({POLLIN}),
                    .revents = 0,
                });
            }

            int timeout = -1;
            if (!m_source.isOpen()) {
                timeout = static_cast<int>(std::chrono::ceil<std::chrono::milliseconds>(
                                               std::max(m_nextConnect - Clock::now(), Clock::duration{}))
                                               .count());
            }
            if (::poll(fds.data(), fds.size(), timeout) < 0 && errno != EINTR) {
                log("poll failed: {}", lastError());
                return;
            }
            handleEvents(fds);
        }
    }

    void stop() const {
        const std::uint64_t one = 1;
        static_cast<void>(::write(m_stop.fd(), &one, sizeof(one)));
    }

 private:
    void handleEvents(std::span<const pollfd> fds) {
        auto pollfdIt = fds.begin();
        if ((pollfdIt++)->revents != 0) {
            m_running = false;
            return;
        }
        const short listenerEvents = (pollfdIt++)->revents;
        const short sourceEvents = (pollfdIt++)->revents;
        if (m_connecting && sourceEvents != 0) {
            finishConnect();
        } else if (hasAny(sourceEvents, {POLLIN, POLLHUP, POLLERR})) {
            readSource();
        }
        for (const auto& client : m_clients) {
            if (pollfdIt == fds.end()) {
                break;
            }
            const short events = (pollfdIt++)->revents;
            if (hasAny(events, {POLLIN})) {
                discardInput(*client);
            }
            if (hasAny(events, {POLLOUT, POLLHUP, POLLERR})) {
                flush(*client);
            }
        }
        std::erase_if(m_clients, [this](const auto& client) {
            if (client->closed) {
                log("Client disconnected, {} left", m_clients.size() - 1);
            }
            return client->closed;
        });
        if (hasAny(listenerEvents, {POLLIN})) {
            acceptClient();
        }
    }

    bool listen() {
        addrinfo hints{};
        hints.ai_family = AF_INET6;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_PASSIVE;
        addrinfo* result = nullptr;
        const std::string port = std::to_string(m_options.listenPort);
        if (const int error = ::getaddrinfo(nullptr, port.c_str(), &hints, &result); error != 0) {
            log("Invalid listen port: {}", ::gai_strerror(error));
            return false;
        }
        const std::unique_ptr<addrinfo, decltype(&::freeaddrinfo)> address(result, &::freeaddrinfo);

        m_listener = FileDescriptor(::socket(address->ai_family, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0));
        const int disable = 0;
        const int enable = 1;
        // Accept IPv4 clients too.
        ::setsockopt(m_listener.fd(), IPPROTO_IPV6, IPV6_V6ONLY, &disable, sizeof(disable));
        ::setsockopt(m_listener.fd(), SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));
        if (!m_listener.isOpen() || ::bind(m_listener.fd(), address->ai_addr, address->ai_addrlen) < 0 ||
            ::listen(m_listener.fd(), SOMAXCONN) < 0) {
            log("Cannot listen on port {}: {}", m_options.listenPort, lastError());
            m_listener.reset();
            return false;
        }
        log("Listening on port {}", m_options.listenPort);
        return true;
    }

    void startConnect() {
        addrinfo hints{};
        hints.ai_socktype = SOCK_STREAM;
        addrinfo* result = nullptr;
        const std::string port = std::to_string(m_options.sourcePort);
        if (::getaddrinfo(m_options.sourceHost.c_str(), port.c_str(), &hints, &result) != 0) {
            retryLater();
            return;
        }
        m_addresses.reset(result);
        m_nextAddress = result;
        connectToNextAddress();
    }

    // A host name can resolve to several addresses (e.g. localhost to ::1 and
    // 127.0.0.1); try each until one accepts.
    void connectToNextAddress() {
        while (m_nextAddress != nullptr) {
            const addrinfo* address = m_nextAddress;
            m_nextAddress = address->ai_next;
            m_source = FileDescriptor(::socket(address->ai_family, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0));
            if (::connect(m_source.fd(), address->ai_addr, address->ai_addrlen) == 0) {
                onConnected();
                return;
            }
            if (errno == EINPROGRESS) {
                m_connecting = true;
                return;
            }
        }
        retryLater();
    }

    void finishConnect() {
        int error = 0;
        socklen_t size = sizeof(error);
        ::getsockopt(m_source.fd(), SOL_SOCKET, SO_ERROR, &error, &size);
        m_connecting = false;
        if (error == 0) {
            onConnected();
        } else {
            connectToNextAddress();
        }
    }

    void onConnected() {
        m_addresses.reset();
        m_nextAddress = nullptr;
        log("Connected to {}:{}", m_options.sourceHost, m_options.sourcePort);
        m_waitingReported = false;
        if (m_listener.isOpen()) {
            return;
        }
        if (!listen()) {
            m_running = false;
            return;
        }
        m_keepForFirstClient = true;
    }

    void retryLater() {
        if (!m_waitingReported) {
            log("Waiting for {}:{}", m_options.sourceHost, m_options.sourcePort);
            m_waitingReported = true;
        }
        m_source.reset();
        m_connecting = false;
        m_input.clear();
        m_nextConnect = Clock::now() + kReconnectDelay;
    }

    void readSource() {
        for (std::size_t total = 0; total < kMaxReadPerPoll;) {
            const std::size_t used = m_input.size();
            m_input.resize(used + kReadChunk);
            const ssize_t received = ::recv(m_source.fd(), &m_input.at(used), kReadChunk, 0);
            m_input.resize(used + static_cast<std::size_t>(std::max<ssize_t>(received, 0)));
            if (received > 0) {
                total += static_cast<std::size_t>(received);
                continue;
            }
            if (received < 0 && wouldBlock()) {
                break;
            }
            log("Source disconnected");
            retryLater();
            return;
        }

        std::size_t consumed = 0;
        for (;;) {
            const auto pending = std::span(m_input).subspan(consumed);
            const Frame frame = inspectFrame(pending);
            if (frame.kind == FrameKind::Incomplete) {
                break;
            }
            if (frame.kind == FrameKind::Invalid) {
                log("Not a DLT v1 stream, reconnecting");
                retryLater();
                return;
            }
            if (matches(frame.appId)) {
                forward(pending.first(frame.size));
            }
            consumed += frame.size;
        }
        m_input.erase(m_input.begin(), m_input.begin() + static_cast<std::ptrdiff_t>(consumed));

        for (const auto& client : m_clients) {
            flush(*client);
        }
    }

    [[nodiscard]] bool matches(const std::optional<Id>& appId) const {
        const std::vector<Id>& appIds = m_options.appIds;
        return appIds.empty() || (appId && std::ranges::find(appIds, *appId) != appIds.end());
    }

    void forward(std::span<const std::uint8_t> message) {
        // Nothing is lost while the first client retries its connection.
        if (m_keepForFirstClient) {
            if (m_backlog.size() + message.size() > kMaxClientBacklog) {
                return;
            }
            m_backlog.insert(m_backlog.end(), message.begin(), message.end());
            return;
        }
        for (const auto& client : m_clients) {
            if (client->output.size() - client->outputStart + message.size() > kMaxClientBacklog) {
                log("Client too slow, disconnecting");
                client->closed = true;
                continue;
            }
            client->output.insert(client->output.end(), message.begin(), message.end());
        }
    }

    static void flush(Client& client) {
        while (!client.closed && client.outputStart < client.output.size()) {
            const ssize_t sent = ::send(client.socket.fd(), &client.output.at(client.outputStart),
                                        client.output.size() - client.outputStart, MSG_NOSIGNAL);
            if (sent < 0) {
                client.closed = !wouldBlock();
                return;
            }
            client.outputStart += static_cast<std::size_t>(sent);
        }
        client.output.clear();
        client.outputStart = 0;
    }

    // Clients may send control requests; this proxy only forwards to them.
    static void discardInput(Client& client) {
        std::array<char, kReadChunk> scratch{};
        for (;;) {
            const ssize_t received = ::recv(client.socket.fd(), scratch.data(), scratch.size(), 0);
            if (received > 0) {
                continue;
            }
            client.closed = received == 0 || !wouldBlock();
            return;
        }
    }

    void acceptClient() {
        for (;;) {
            FileDescriptor socket(::accept4(m_listener.fd(), nullptr, nullptr, SOCK_NONBLOCK | SOCK_CLOEXEC));
            if (!socket.isOpen()) {
                return;
            }
            auto client = std::make_unique<Client>();
            client->socket = std::move(socket);
            if (m_keepForFirstClient) {
                client->output = std::exchange(m_backlog, {});
                m_keepForFirstClient = false;
            }
            m_clients.push_back(std::move(client));
            log("Client connected, {} total", m_clients.size());
        }
    }

    template <typename... Args>
    void log(std::format_string<Args...> format, Args&&... args) const {
        if (m_options.log) {
            m_options.log(std::format(format, std::forward<Args>(args)...));
        }
    }

    Options m_options;
    FileDescriptor m_stop{::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC)};
    bool m_running = true;
    FileDescriptor m_listener;
    FileDescriptor m_source;
    std::unique_ptr<addrinfo, decltype(&::freeaddrinfo)> m_addresses{nullptr, &::freeaddrinfo};
    const addrinfo* m_nextAddress = nullptr;
    bool m_connecting = false;
    bool m_waitingReported = false;
    Clock::time_point m_nextConnect;
    std::vector<std::uint8_t> m_input;
    bool m_keepForFirstClient = false;
    std::vector<std::uint8_t> m_backlog;
    std::vector<std::unique_ptr<Client>> m_clients;
};

Proxy::Proxy(Options options)
    : m_impl(std::make_unique<Impl>(std::move(options))), m_thread([impl = m_impl.get()] { impl->run(); }) {}

// m_thread is joined before m_impl is destroyed.
Proxy::~Proxy() { m_impl->stop(); }

void Proxy::wait() { m_thread.join(); }

}  // namespace dlt_proxy
