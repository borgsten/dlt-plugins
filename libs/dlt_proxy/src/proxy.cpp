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

enum class FrameKind : std::uint8_t { kIncomplete, kComplete, kInvalid };

struct Frame {
    FrameKind kind = FrameKind::kIncomplete;
    std::size_t size = 0;
    /* Only messages with an extended header have one */
    std::optional<Id> app_id = std::nullopt;
};

Frame inspectFrame(std::span<const std::uint8_t> data) {
    std::array<std::uint8_t, kStandardHeaderSize> header{};
    if (data.size() < header.size()) {
        return {.kind = FrameKind::kIncomplete};
    }
    std::ranges::copy(data.first(header.size()), header.begin());
    const std::uint8_t htyp = header.at(0);
    if ((htyp & kVersionMask) != kVersion1) {
        return {.kind = FrameKind::kInvalid};
    }

    const std::size_t length = (std::size_t{header.at(2)} << 8U) | header.at(3);
    std::size_t extended_header = kStandardHeaderSize;
    for (const std::uint8_t flag : {kWithEcuId, kWithSessionId, kWithTimestamp}) {
        if ((htyp & flag) != 0) {
            extended_header += kOptionalFieldSize;
        }
    }
    if (length < extended_header) {
        return {.kind = FrameKind::kInvalid};
    }
    if (data.size() < length) {
        return {.kind = FrameKind::kIncomplete};
    }
    if ((htyp & kUseExtendedHeader) == 0 || length < extended_header + kExtendedHeaderSize) {
        return {.kind = FrameKind::kComplete, .size = length};
    }

    Id app_id{};
    std::ranges::copy(data.subspan(extended_header + kApidOffset, app_id.size()), app_id.begin());
    return {.kind = FrameKind::kComplete, .size = length, .app_id = app_id};
}

class FileDescriptor {
 public:
    FileDescriptor() = default;
    explicit FileDescriptor(int descriptor) : fd_(descriptor) {}
    FileDescriptor(const FileDescriptor&) = delete;
    FileDescriptor& operator=(const FileDescriptor&) = delete;
    FileDescriptor(FileDescriptor&& other) noexcept : fd_(std::exchange(other.fd_, -1)) {}
    FileDescriptor& operator=(FileDescriptor&& other) noexcept {
        std::swap(fd_, other.fd_);
        return *this;
    }
    ~FileDescriptor() { reset(); }

    [[nodiscard]] int fd() const { return fd_; }
    [[nodiscard]] bool isOpen() const { return fd_ >= 0; }
    void reset() {
        if (fd_ >= 0) {
            ::close(fd_);
            fd_ = -1;
        }
    }

 private:
    int fd_ = -1;
};

struct Client {
    FileDescriptor socket;
    std::vector<std::uint8_t> output;
    std::size_t output_start = 0;
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
    explicit Impl(Options options) : options_(std::move(options)) {}

    void run() {
        std::vector<pollfd> fds;
        while (running_) {
            if (!source_.isOpen() && Clock::now() >= next_connect_) {
                startConnect();
                if (!running_) {
                    break;
                }
            }

            // poll() ignores negative descriptors, which keeps the first
            // three entries in place while the listener or source is closed.
            fds.clear();
            fds.push_back({.fd = stop_.fd(), .events = POLLIN, .revents = 0});
            fds.push_back({.fd = listener_.fd(), .events = POLLIN, .revents = 0});
            fds.push_back({
                .fd = source_.fd(),
                .events = static_cast<short>(connecting_ ? POLLOUT : POLLIN),
                .revents = 0,
            });
            for (const auto& client : clients_) {
                const bool pending = client->output_start < client->output.size();
                fds.push_back({
                    .fd = client->socket.fd(),
                    .events = pending ? pollFlags({POLLIN, POLLOUT}) : pollFlags({POLLIN}),
                    .revents = 0,
                });
            }

            int timeout = -1;
            if (!source_.isOpen()) {
                timeout = static_cast<int>(std::chrono::ceil<std::chrono::milliseconds>(
                                               std::max(next_connect_ - Clock::now(), Clock::duration{}))
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
        // EAGAIN: the counter is full, so it is signalled already.
        if (::eventfd_write(stop_.fd(), 1) != 0 && errno != EAGAIN) {
            log("waking the proxy thread failed: {}", lastError());
        }
    }

 private:
    void handleEvents(std::span<const pollfd> fds) {
        auto pollfd_it = fds.begin();
        if ((pollfd_it++)->revents != 0) {
            running_ = false;
            return;
        }
        const short listener_events = (pollfd_it++)->revents;
        const short source_events = (pollfd_it++)->revents;
        if (connecting_ && source_events != 0) {
            finishConnect();
        } else if (hasAny(source_events, {POLLIN, POLLHUP, POLLERR})) {
            readSource();
        }
        for (const auto& client : clients_) {
            if (pollfd_it == fds.end()) {
                break;
            }
            const short events = (pollfd_it++)->revents;
            if (hasAny(events, {POLLIN})) {
                discardInput(*client);
            }
            if (hasAny(events, {POLLOUT, POLLHUP, POLLERR})) {
                flush(*client);
            }
        }
        std::erase_if(clients_, [this](const auto& client) {
            if (client->closed) {
                log("Client disconnected, {} left", clients_.size() - 1);
            }
            return client->closed;
        });
        if (hasAny(listener_events, {POLLIN})) {
            acceptClient();
        }
    }

    bool listen() {
        addrinfo hints{};
        hints.ai_family = AF_INET6;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_PASSIVE;
        addrinfo* result = nullptr;
        const std::string port = std::to_string(options_.listen_port);
        if (const int error = ::getaddrinfo(nullptr, port.c_str(), &hints, &result); error != 0) {
            log("Invalid listen port: {}", ::gai_strerror(error));
            return false;
        }
        const std::unique_ptr<addrinfo, decltype(&::freeaddrinfo)> address(result, &::freeaddrinfo);

        listener_ = FileDescriptor(::socket(address->ai_family, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0));
        const int disable = 0;
        const int enable = 1;
        // Accept IPv4 clients too.
        ::setsockopt(listener_.fd(), IPPROTO_IPV6, IPV6_V6ONLY, &disable, sizeof(disable));
        ::setsockopt(listener_.fd(), SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));
        if (!listener_.isOpen() || ::bind(listener_.fd(), address->ai_addr, address->ai_addrlen) < 0 ||
            ::listen(listener_.fd(), SOMAXCONN) < 0) {
            log("Cannot listen on port {}: {}", options_.listen_port, lastError());
            listener_.reset();
            return false;
        }
        log("Listening on port {}", options_.listen_port);
        return true;
    }

    void startConnect() {
        addrinfo hints{};
        hints.ai_socktype = SOCK_STREAM;
        addrinfo* result = nullptr;
        const std::string port = std::to_string(options_.source_port);
        if (::getaddrinfo(options_.source_host.c_str(), port.c_str(), &hints, &result) != 0) {
            retryLater();
            return;
        }
        addresses_.reset(result);
        next_address_ = result;
        connectToNextAddress();
    }

    // A host name can resolve to several addresses (e.g. localhost to ::1 and
    // 127.0.0.1); try each until one accepts.
    void connectToNextAddress() {
        while (next_address_ != nullptr) {
            const addrinfo* address = next_address_;
            next_address_ = address->ai_next;
            source_ = FileDescriptor(::socket(address->ai_family, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0));
            if (::connect(source_.fd(), address->ai_addr, address->ai_addrlen) == 0) {
                onConnected();
                return;
            }
            if (errno == EINPROGRESS) {
                connecting_ = true;
                return;
            }
        }
        retryLater();
    }

    void finishConnect() {
        int error = 0;
        socklen_t size = sizeof(error);
        ::getsockopt(source_.fd(), SOL_SOCKET, SO_ERROR, &error, &size);
        connecting_ = false;
        if (error == 0) {
            onConnected();
        } else {
            connectToNextAddress();
        }
    }

    void onConnected() {
        addresses_.reset();
        next_address_ = nullptr;
        log("Connected to {}:{}", options_.source_host, options_.source_port);
        waiting_reported_ = false;
        if (listener_.isOpen()) {
            return;
        }
        if (!listen()) {
            running_ = false;
            return;
        }
        keep_for_first_client_ = true;
    }

    void retryLater() {
        if (!waiting_reported_) {
            log("Waiting for {}:{}", options_.source_host, options_.source_port);
            waiting_reported_ = true;
        }
        source_.reset();
        connecting_ = false;
        input_.clear();
        next_connect_ = Clock::now() + kReconnectDelay;
    }

    void readSource() {
        for (std::size_t total = 0; total < kMaxReadPerPoll;) {
            const std::size_t used = input_.size();
            input_.resize(used + kReadChunk);
            const ssize_t received = ::recv(source_.fd(), &input_.at(used), kReadChunk, 0);
            input_.resize(used + static_cast<std::size_t>(std::max<ssize_t>(received, 0)));
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
            const auto pending = std::span(input_).subspan(consumed);
            const Frame frame = inspectFrame(pending);
            if (frame.kind == FrameKind::kIncomplete) {
                break;
            }
            if (frame.kind == FrameKind::kInvalid) {
                log("Not a DLT v1 stream, reconnecting");
                retryLater();
                return;
            }
            if (matches(frame.app_id)) {
                forward(pending.first(frame.size));
            }
            consumed += frame.size;
        }
        input_.erase(input_.begin(), input_.begin() + static_cast<std::ptrdiff_t>(consumed));

        for (const auto& client : clients_) {
            flush(*client);
        }
    }

    [[nodiscard]] bool matches(const std::optional<Id>& app_id) const {
        const std::vector<Id>& app_ids = options_.app_ids;
        return app_ids.empty() || (app_id && std::ranges::find(app_ids, *app_id) != app_ids.end());
    }

    void forward(std::span<const std::uint8_t> message) {
        // Nothing is lost while the first client retries its connection.
        if (keep_for_first_client_) {
            if (backlog_.size() + message.size() > kMaxClientBacklog) {
                return;
            }
            backlog_.insert(backlog_.end(), message.begin(), message.end());
            return;
        }
        for (const auto& client : clients_) {
            if (client->output.size() - client->output_start + message.size() > kMaxClientBacklog) {
                log("Client too slow, disconnecting");
                client->closed = true;
                continue;
            }
            client->output.insert(client->output.end(), message.begin(), message.end());
        }
    }

    static void flush(Client& client) {
        while (!client.closed && client.output_start < client.output.size()) {
            const ssize_t sent = ::send(client.socket.fd(), &client.output.at(client.output_start),
                                        client.output.size() - client.output_start, MSG_NOSIGNAL);
            if (sent < 0) {
                client.closed = !wouldBlock();
                return;
            }
            client.output_start += static_cast<std::size_t>(sent);
        }
        client.output.clear();
        client.output_start = 0;
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
            FileDescriptor socket(::accept4(listener_.fd(), nullptr, nullptr, SOCK_NONBLOCK | SOCK_CLOEXEC));
            if (!socket.isOpen()) {
                return;
            }
            auto client = std::make_unique<Client>();
            client->socket = std::move(socket);
            if (keep_for_first_client_) {
                client->output = std::exchange(backlog_, {});
                keep_for_first_client_ = false;
            }
            clients_.push_back(std::move(client));
            log("Client connected, {} total", clients_.size());
        }
    }

    template <typename... Args>
    void log(std::format_string<Args...> format, Args&&... args) const {
        if (options_.log) {
            options_.log(std::format(format, std::forward<Args>(args)...));
        }
    }

    Options options_;
    FileDescriptor stop_{::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC)};
    bool running_ = true;
    FileDescriptor listener_;
    FileDescriptor source_;
    std::unique_ptr<addrinfo, decltype(&::freeaddrinfo)> addresses_{nullptr, &::freeaddrinfo};
    const addrinfo* next_address_ = nullptr;
    bool connecting_ = false;
    bool waiting_reported_ = false;
    Clock::time_point next_connect_;
    std::vector<std::uint8_t> input_;
    bool keep_for_first_client_ = false;
    std::vector<std::uint8_t> backlog_;
    std::vector<std::unique_ptr<Client>> clients_;
};

Proxy::Proxy(Options options)
    : impl_(std::make_unique<Impl>(std::move(options))), thread_([impl = impl_.get()] { impl->run(); }) {}

// thread_ is joined before impl_ is destroyed.
Proxy::~Proxy() { impl_->stop(); }

void Proxy::wait() { thread_.join(); }

}  // namespace dlt_proxy
