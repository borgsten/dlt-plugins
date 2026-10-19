// Minimal DLT test source. Listens on TCP (default port 3490, like
// dlt-daemon) and continuously sends verbose log messages from several app
// and context IDs to every connected client, e.g. dlt-viewer or the proxy.

#include <algorithm>
#include <array>
#include <cerrno>
#include <charconv>
#include <chrono>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <format>
#include <iostream>
#include <memory>
#include <netdb.h>
#include <netinet/in.h>
#include <optional>
#include <ratio>
#include <span>
#include <string>
#include <string_view>
#include <sys/socket.h>
#include <system_error>
#include <thread>
#include <unistd.h>
#include <utility>
#include <vector>

namespace {

enum class LogLevel : std::uint8_t { Fatal = 1, Error, Warn, Info, Debug, Verbose };

struct Source {
    std::string_view appId;
    std::string_view contextId;
    LogLevel level;
    std::string_view text;
};

constexpr std::array kSources{
    Source{.appId = "NAV", .contextId = "ROUT", .level = LogLevel::Info, .text = "Route recalculated"},
    Source{.appId = "NAV", .contextId = "GPS", .level = LogLevel::Debug, .text = "GPS fix received"},
    Source{.appId = "MED", .contextId = "PLAY", .level = LogLevel::Info, .text = "Playing track"},
    Source{.appId = "MED", .contextId = "BT", .level = LogLevel::Warn, .text = "Bluetooth link quality low"},
    Source{.appId = "HMI", .contextId = "UI", .level = LogLevel::Verbose, .text = "Frame rendered"},
    Source{.appId = "HMI", .contextId = "TOUC", .level = LogLevel::Debug, .text = "Touch event"},
    Source{.appId = "DIAG", .contextId = "DTC", .level = LogLevel::Error, .text = "DTC stored"},
    Source{.appId = "SYS", .contextId = "PWR", .level = LogLevel::Fatal, .text = "Voltage out of range"},
};

constexpr std::string_view kEcuId = "TEST";
constexpr std::uint16_t kDefaultPort = 3490;
constexpr int kDefaultIntervalMs = 100;

// Extended header, ECU ID, session ID, timestamp, version 1. The standard
// header is big endian, the extended header and payload little endian.
constexpr std::uint8_t kHeaderType = 0x3d;
constexpr std::size_t kLengthOffset = 2;
// Extended header: a verbose log message, the level in the upper bits.
constexpr std::uint8_t kVerbose = 0x01;
constexpr unsigned kLogLevelShift = 4;
constexpr std::uint8_t kArgumentCount = 2;
// Verbose argument type info.
constexpr std::uint32_t kTypeString = 0x00000200;
constexpr std::uint32_t kTypeUnsigned32 = 0x00000043;

constexpr unsigned kByteBits = 8;
constexpr std::uint8_t kLowByte = 0xff;

template <typename... Args>
void log(std::format_string<Args...> format, Args&&... args) {
    std::cerr << std::format(format, std::forward<Args>(args)...) << '\n';
}

// DLT timestamps count 0.1 ms.
using DltTicks = std::chrono::duration<std::int64_t, std::ratio_multiply<std::hecto, std::micro>>;

struct MessageInfo {
    std::uint8_t messageCounter = 0;
    std::uint32_t timestamp = 0;
    std::uint32_t sequence = 0;
};

template <std::unsigned_integral T>
void putLittleEndian(std::vector<std::uint8_t>& out, T value) {
    for (unsigned byte = 0; byte < sizeof(T); ++byte) {
        out.push_back(static_cast<std::uint8_t>((std::uint32_t{value} >> (kByteBits * byte)) & kLowByte));
    }
}

template <std::unsigned_integral T>
void putBigEndian(std::vector<std::uint8_t>& out, T value) {
    const std::size_t start = out.size();
    putLittleEndian(out, value);
    std::reverse(out.begin() + static_cast<std::ptrdiff_t>(start), out.end());
}

void putId(std::vector<std::uint8_t>& out, std::string_view value) {
    std::array<std::uint8_t, 4> padded{};
    std::ranges::copy(value.substr(0, padded.size()), padded.begin());
    out.insert(out.end(), padded.begin(), padded.end());
}

// A verbose log message with two arguments: the source's text and the sequence number.
std::vector<std::uint8_t> buildMessage(const Source& source, const MessageInfo& info) {
    std::vector<std::uint8_t> message;
    message.push_back(kHeaderType);
    message.push_back(info.messageCounter);
    putBigEndian(message, std::uint16_t{0});  // length, set below
    putId(message, kEcuId);
    putBigEndian(message, static_cast<std::uint32_t>(::getpid()));
    putBigEndian(message, info.timestamp);

    message.push_back(static_cast<std::uint8_t>(kVerbose | (static_cast<unsigned>(source.level) << kLogLevelShift)));
    message.push_back(kArgumentCount);
    putId(message, source.appId);
    putId(message, source.contextId);

    putLittleEndian(message, kTypeString);
    putLittleEndian(message, static_cast<std::uint16_t>(source.text.size() + 1));
    message.insert(message.end(), source.text.begin(), source.text.end());
    message.push_back(0);

    putLittleEndian(message, kTypeUnsigned32);
    putLittleEndian(message, info.sequence);

    std::vector<std::uint8_t> length;
    putBigEndian(length, static_cast<std::uint16_t>(message.size()));
    std::ranges::copy(length, message.begin() + kLengthOffset);
    return message;
}

// Returns the listening socket, or -1.
int listenOn(std::uint16_t port) {
    addrinfo hints{};
    hints.ai_family = AF_INET6;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    addrinfo* result = nullptr;
    if (::getaddrinfo(nullptr, std::to_string(port).c_str(), &hints, &result) != 0) {
        return -1;
    }
    const std::unique_ptr<addrinfo, decltype(&::freeaddrinfo)> address(result, &::freeaddrinfo);

    const int listener = ::socket(address->ai_family, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (listener < 0) {
        return -1;
    }
    const int disable = 0;
    const int enable = 1;
    // Accept IPv4 clients too.
    ::setsockopt(listener, IPPROTO_IPV6, IPV6_V6ONLY, &disable, sizeof(disable));
    ::setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));
    if (::bind(listener, address->ai_addr, address->ai_addrlen) < 0 || ::listen(listener, SOMAXCONN) < 0) {
        ::close(listener);
        return -1;
    }
    return listener;
}

void acceptClients(int listener, std::vector<int>& clients) {
    for (;;) {
        // Blocking, so a message is always sent whole.
        const int client = ::accept4(listener, nullptr, nullptr, SOCK_CLOEXEC);
        if (client < 0) {
            return;
        }
        clients.push_back(client);
        log("Client connected, {} total", clients.size());
    }
}

void broadcast(std::vector<int>& clients, std::span<const std::uint8_t> message) {
    std::erase_if(clients, [message, &clients](int client) {
        if (::send(client, message.data(), message.size(), MSG_NOSIGNAL) == static_cast<ssize_t>(message.size())) {
            return false;
        }
        ::close(client);
        log("Client disconnected, {} left", clients.size() - 1);
        return true;
    });
}

template <typename T>
std::optional<T> parsePositive(std::string_view text) {
    T value{};
    const auto [end, error] = std::from_chars(text.begin(), text.end(), value);
    if (error != std::errc{} || end != text.end() || value <= 0) {
        return std::nullopt;
    }
    return value;
}

}  // namespace

int main(int argc, char** argv) {
    const std::span argSpan(argv, static_cast<std::size_t>(argc));
    const std::vector<std::string_view> args(argSpan.begin(), argSpan.end());
    const auto port = args.size() > 1 ? parsePositive<std::uint16_t>(args.at(1)) : kDefaultPort;
    const auto intervalMs = args.size() > 2 ? parsePositive<int>(args.at(2)) : kDefaultIntervalMs;
    if (args.size() > 3 || !port || !intervalMs) {
        log("Usage: {} [port] [interval_ms]\n"
            "Sends DLT log messages to all clients (default port {}, every {} ms).",
            args.front(), kDefaultPort, kDefaultIntervalMs);
        return 2;
    }

    const int listener = listenOn(*port);
    if (listener < 0) {
        log("Cannot listen on port {}: {}", *port, std::system_category().message(errno));
        return 1;
    }
    log("DLT test source on port {}, ECU {}, one message every {} ms", *port, kEcuId, *intervalMs);

    std::vector<int> clients;
    const auto start = std::chrono::steady_clock::now();
    for (std::uint32_t sequence = 0;; ++sequence) {
        acceptClients(listener, clients);
        const auto elapsed = std::chrono::duration_cast<DltTicks>(std::chrono::steady_clock::now() - start);
        const MessageInfo info{
            .messageCounter = static_cast<std::uint8_t>(sequence),
            .timestamp = static_cast<std::uint32_t>(elapsed.count()),
            .sequence = sequence,
        };
        broadcast(clients, buildMessage(kSources.at(sequence % kSources.size()), info));
        std::this_thread::sleep_for(std::chrono::milliseconds(*intervalMs));
    }
}
