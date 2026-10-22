// Minimal DLT test source. Listens on TCP (default port 3490, like
// dlt-daemon) and continuously sends verbose log messages from several app
// and context IDs to every connected client, e.g. dlt-viewer or the proxy.
// Can mix in mocked test runs (suite and test start/end tokens, as the Test
// Overview plugin expects by default), and write to a .dlt file instead.

#include <algorithm>
#include <array>
#include <cerrno>
#include <charconv>
#include <chrono>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <format>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <netdb.h>
#include <netinet/in.h>
#include <optional>
#include <ostream>
#include <random>
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

enum class LogLevel : std::uint8_t { kFatal = 1, kError, kWarn, kInfo, kDebug, kVerbose };

struct Source {
    std::string_view app_id;
    std::string_view context_id;
    LogLevel level;
    std::string_view text;
};

// Mocked test runs, as Robot Framework's listener logs them: a suite per
// directory and test file, nested, with dotted names.
struct TestFile {
    std::string_view name;
    std::span<const std::string_view> tests;
};

struct TestDirectory {
    std::string_view name;
    std::span<const TestFile> files;
};

constexpr std::array kSources{
    Source{.app_id = "NAV", .context_id = "ROUT", .level = LogLevel::kInfo, .text = "Route recalculated"},
    Source{.app_id = "NAV", .context_id = "GPS", .level = LogLevel::kDebug, .text = "GPS fix received"},
    Source{.app_id = "MED", .context_id = "PLAY", .level = LogLevel::kInfo, .text = "Playing track"},
    Source{.app_id = "MED", .context_id = "BT", .level = LogLevel::kWarn, .text = "Bluetooth link quality low"},
    Source{.app_id = "HMI", .context_id = "UI", .level = LogLevel::kVerbose, .text = "Frame rendered"},
    Source{.app_id = "HMI", .context_id = "TOUC", .level = LogLevel::kDebug, .text = "Touch event"},
    Source{.app_id = "DIAG", .context_id = "DTC", .level = LogLevel::kError, .text = "DTC stored"},
    Source{.app_id = "SYS", .context_id = "PWR", .level = LogLevel::kFatal, .text = "Voltage out of range"},
};

constexpr std::array kHttpProxyTests = std::to_array<std::string_view>(
    {"Proxy host responds", "State", "Proxying", "Crash recovery", "Caching", "Connect timeout", "Purge"});
constexpr std::array kDnsTests = std::to_array<std::string_view>({"Resolve host", "Cache entries", "Fallback server"});
constexpr std::array kPlaybackTests =
    std::to_array<std::string_view>({"Play", "Pause", "Seek", "Next track", "Gapless", "Resume after reboot"});
constexpr std::array kBluetoothTests = std::to_array<std::string_view>({"Pair phone", "Audio stream", "Reconnect"});
constexpr std::array kPowerTests =
    std::to_array<std::string_view>({"Suspend resume", "Voltage drop", "Shutdown on low battery"});
constexpr std::array kDiagnosticsTests = std::to_array<std::string_view>({"Read DTC", "Clear DTC", "Self test"});

constexpr std::array kConnectivityFiles{
    TestFile{.name = "Http Proxy", .tests = kHttpProxyTests},
    TestFile{.name = "Dns", .tests = kDnsTests},
};
constexpr std::array kMediaFiles{
    TestFile{.name = "Playback", .tests = kPlaybackTests},
    TestFile{.name = "Bluetooth", .tests = kBluetoothTests},
};
constexpr std::array kSystemFiles{
    TestFile{.name = "Power", .tests = kPowerTests},
    TestFile{.name = "Diagnostics", .tests = kDiagnosticsTests},
};

constexpr std::string_view kRootSuite = "baseline";
constexpr std::array kDirectories{
    TestDirectory{.name = "Connectivity", .files = kConnectivityFiles},
    TestDirectory{.name = "Media", .files = kMediaFiles},
    TestDirectory{.name = "System", .files = kSystemFiles},
};

// The test framework's tokens.
constexpr std::string_view kTestAppId = "HOST";
constexpr std::string_view kTestContextId = "ROBO";
// Percentages: tests that pass or fail; the rest never end, as if crashed.
constexpr int kPassPercent = 80;
constexpr int kFailPercent = 15;
// Test file suites that never end.
constexpr int kUnfinishedSuitePercent = 5;
constexpr int kMinTestsPerFile = 2;
// Other messages between two tokens.
constexpr int kMinNoise = 1;
constexpr int kMaxNoise = 6;
constexpr std::uint32_t kDefaultSeed = 1;
constexpr int kMaxPercent = 99;

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
// Verbose argument type info.
constexpr std::uint32_t kTypeString = 0x00000200;
constexpr std::uint32_t kTypeUnsigned32 = 0x00000043;

// Storage header of messages in .dlt files.
constexpr std::string_view kStoragePattern{"DLT\x01", 4};
constexpr std::int64_t kMicrosPerSecond = 1'000'000;

constexpr unsigned kByteBits = 8;
constexpr std::uint8_t kLowByte = 0xff;

template <typename... Args>
void log(std::format_string<Args...> format, Args&&... args) {
    std::cerr << std::format(format, std::forward<Args>(args)...) << '\n';
}

// DLT timestamps count 0.1 ms.
using DltTicks = std::chrono::duration<std::int64_t, std::ratio_multiply<std::hecto, std::micro>>;

struct MessageInfo {
    std::uint8_t message_counter = 0;
    std::uint32_t timestamp = 0;
};

struct Message {
    std::string_view app_id;
    std::string_view context_id;
    LogLevel level;
    std::string text;
    // A second argument, if set.
    std::optional<std::uint32_t> sequence;
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

// A verbose log message: the text, and the sequence number if there is one.
std::vector<std::uint8_t> buildMessage(const Message& source, const MessageInfo& info) {
    std::vector<std::uint8_t> message;
    message.push_back(kHeaderType);
    message.push_back(info.message_counter);
    putBigEndian(message, std::uint16_t{0});  // length, set below
    putId(message, kEcuId);
    putBigEndian(message, static_cast<std::uint32_t>(::getpid()));
    putBigEndian(message, info.timestamp);

    message.push_back(static_cast<std::uint8_t>(kVerbose | (static_cast<unsigned>(source.level) << kLogLevelShift)));
    message.push_back(source.sequence ? 2 : 1);
    putId(message, source.app_id);
    putId(message, source.context_id);

    putLittleEndian(message, kTypeString);
    putLittleEndian(message, static_cast<std::uint16_t>(source.text.size() + 1));
    message.insert(message.end(), source.text.begin(), source.text.end());
    message.push_back(0);

    if (source.sequence) {
        putLittleEndian(message, kTypeUnsigned32);
        putLittleEndian(message, *source.sequence);
    }

    std::vector<std::uint8_t> length;
    putBigEndian(length, static_cast<std::uint16_t>(message.size()));
    std::ranges::copy(length, message.begin() + kLengthOffset);
    return message;
}

// What a .dlt file stores before each message.
std::vector<std::uint8_t> buildStorageHeader(std::chrono::system_clock::time_point time) {
    const std::int64_t micros = std::chrono::duration_cast<std::chrono::microseconds>(time.time_since_epoch()).count();
    std::vector<std::uint8_t> header(kStoragePattern.begin(), kStoragePattern.end());
    putLittleEndian(header, static_cast<std::uint32_t>(micros / kMicrosPerSecond));
    putLittleEndian(header, static_cast<std::uint32_t>(micros % kMicrosPerSecond));
    putId(header, kEcuId);
    return header;
}

/*
 * The messages to send: the sources in turn, and with tests, mocked test runs
 * in between, suite after suite, each with some of its tests and results.
 */
class MessageGenerator {
 public:
    MessageGenerator(bool tests, std::uint32_t seed) : tests_(tests), random_(seed) {}

    Message next() {
        if (tests_ && noise_left_ == 0) {
            if (tokens_.empty()) {
                queueRun();
            }
            Message token{
                .app_id = kTestAppId,
                .context_id = kTestContextId,
                .level = LogLevel::kInfo,
                .text = std::move(tokens_.front()),
                .sequence = std::nullopt,
            };
            tokens_.pop_front();
            noise_left_ = std::uniform_int_distribution(kMinNoise, kMaxNoise)(random_);
            return token;
        }
        if (noise_left_ > 0) {
            --noise_left_;
        }
        const Source& source = kSources.at(sequence_ % kSources.size());
        return {
            .app_id = source.app_id,
            .context_id = source.context_id,
            .level = source.level,
            .text = std::string(source.text),
            .sequence = sequence_++,
        };
    }

 private:
    // All of the suites, each test file with some of its tests.
    void queueRun() {
        bool run_passed = true;
        startSuite(kRootSuite);
        for (const TestDirectory& directory : kDirectories) {
            const std::string directory_name = std::format("{}.{}", kRootSuite, directory.name);
            bool directory_passed = true;
            startSuite(directory_name);
            for (const TestFile& file : directory.files) {
                const bool passed = queueFile(std::format("{}.{}", directory_name, file.name), file.tests);
                directory_passed = directory_passed && passed;
            }
            endSuite(directory_name, directory_passed);
            run_passed = run_passed && directory_passed;
        }
        endSuite(kRootSuite, run_passed);
    }

    // Whether all of its tests passed.
    bool queueFile(const std::string& name, std::span<const std::string_view> all_tests) {
        startSuite(name);
        const auto count = std::uniform_int_distribution<std::size_t>(kMinTestsPerFile, all_tests.size())(random_);
        std::vector<std::string_view> tests;
        std::ranges::sample(all_tests, std::back_inserter(tests), static_cast<std::ptrdiff_t>(count), random_);
        bool passed = true;
        for (const std::string_view test : tests) {
            tokens_.push_back(std::format("START TESTCASE: {}.{}", name, test));
            const int outcome = percent();
            if (outcome < kPassPercent + kFailPercent) {
                const bool test_passed = outcome < kPassPercent;
                tokens_.push_back(std::format("END TESTCASE: {}.{} status: {}", name, test, result(test_passed)));
                passed = passed && test_passed;
            } else {
                passed = false;
            }
        }
        if (percent() >= kUnfinishedSuitePercent) {
            endSuite(name, passed);
        }
        return passed;
    }

    void startSuite(std::string_view name) { tokens_.push_back(std::format("START SUITE: {}", name)); }
    void endSuite(std::string_view name, bool passed) {
        tokens_.push_back(std::format("END SUITE: {} status: {}", name, result(passed)));
    }

    int percent() { return std::uniform_int_distribution(0, kMaxPercent)(random_); }
    static std::string_view result(bool passed) { return passed ? "PASS" : "FAIL"; }

    bool tests_;
    std::mt19937 random_;
    std::deque<std::string> tokens_;
    std::uint32_t sequence_ = 0;
    int noise_left_ = 0;
};

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

// Parses text into target; false if it is not a positive number.
template <typename T>
bool assignPositive(std::string_view text, T& target) {
    const std::optional<T> value = parsePositive<T>(text);
    if (value) {
        target = *value;
    }
    return value.has_value();
}

struct Output {
    std::string file;
    std::uint64_t count;
};

struct Options {
    std::uint16_t port = kDefaultPort;
    int interval_ms = kDefaultIntervalMs;
    bool tests = false;
    // The same seed gives the same test runs.
    std::uint32_t seed = kDefaultSeed;
    // Written instead of served.
    std::optional<Output> output;
};

std::optional<Options> parseOptions(const std::vector<std::string_view>& args) {
    Options options;
    std::string file;
    std::optional<std::uint64_t> count;
    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string_view arg = args.at(i);
        const std::optional<std::string_view> value =
            i + 1 < args.size() ? std::optional(args.at(i + 1)) : std::nullopt;
        if (arg == "--tests") {
            options.tests = true;
            continue;
        }
        if (!value) {
            return std::nullopt;
        }
        ++i;
        bool valid = true;
        if (arg == "--port") {
            valid = assignPositive(*value, options.port);
        } else if (arg == "--interval") {
            valid = assignPositive(*value, options.interval_ms);
        } else if (arg == "--seed") {
            valid = assignPositive(*value, options.seed);
        } else if (arg == "--write") {
            file = std::string(*value);
        } else if (arg == "--count") {
            count.emplace();
            valid = assignPositive(*value, *count);
        } else {
            valid = false;
        }
        if (!valid) {
            return std::nullopt;
        }
    }
    if (file.empty() != !count) {
        return std::nullopt;
    }
    if (count) {
        options.output = Output{.file = std::move(file), .count = *count};
    }
    return options;
}

void writeBytes(std::ostream& out, std::span<const std::uint8_t> bytes) {
    std::ranges::copy(bytes, std::ostreambuf_iterator<char>(out));
}

int writeFile(const Options& options, const Output& output) {
    std::ofstream out(output.file, std::ios::binary);
    if (!out) {
        log("Cannot write {}", output.file);
        return 1;
    }
    MessageGenerator generator(options.tests, options.seed);
    const auto start = std::chrono::system_clock::now();
    for (std::uint64_t number = 0; number < output.count; ++number) {
        // Spaced as if sent live.
        const auto elapsed = std::chrono::milliseconds(options.interval_ms) * number;
        const std::vector<std::uint8_t> message = buildMessage(
            generator.next(),
            {
                .message_counter = static_cast<std::uint8_t>(number),
                .timestamp = static_cast<std::uint32_t>(std::chrono::duration_cast<DltTicks>(elapsed).count()),
            });
        writeBytes(out, buildStorageHeader(start + elapsed));
        writeBytes(out, message);
    }
    if (!out) {
        log("Writing {} failed", output.file);
        return 1;
    }
    log("Wrote {} messages to {}", output.count, output.file);
    return 0;
}

int serve(const Options& options) {
    const int listener = listenOn(options.port);
    if (listener < 0) {
        log("Cannot listen on port {}: {}", options.port, std::system_category().message(errno));
        return 1;
    }
    log("DLT test source on port {}, ECU {}, one message every {} ms{}", options.port, kEcuId, options.interval_ms,
        options.tests ? ", with test runs" : "");

    std::vector<int> clients;
    MessageGenerator generator(options.tests, options.seed);
    const auto start = std::chrono::steady_clock::now();
    for (std::uint32_t number = 0;; ++number) {
        acceptClients(listener, clients);
        const auto elapsed = std::chrono::duration_cast<DltTicks>(std::chrono::steady_clock::now() - start);
        const MessageInfo info{
            .message_counter = static_cast<std::uint8_t>(number),
            .timestamp = static_cast<std::uint32_t>(elapsed.count()),
        };
        broadcast(clients, buildMessage(generator.next(), info));
        std::this_thread::sleep_for(std::chrono::milliseconds(options.interval_ms));
    }
}

}  // namespace

int main(int argc, char** argv) {
    const std::span arg_span(argv, static_cast<std::size_t>(argc));
    const std::vector<std::string_view> args(arg_span.begin(), arg_span.end());
    const std::optional<Options> options = parseOptions({std::next(args.begin()), args.end()});
    if (!options) {
        log("Usage: {} [--port N] [--interval MS] [--tests [--seed N]] [--write FILE --count N]\n"
            "Sends DLT log messages to all clients (default port {}, every {} ms).\n"
            "  --tests   mix in mocked test runs: suite and test start/end tokens from {} {}\n"
            "  --seed    vary the test runs (default {})\n"
            "  --write   write COUNT messages to FILE, a .dlt file, instead",
            args.front(), kDefaultPort, kDefaultIntervalMs, kTestAppId, kTestContextId, kDefaultSeed);
        return 2;
    }
    if (const std::optional<Output>& output = options->output) {
        return writeFile(*options, *output);
    }
    return serve(*options);
}
