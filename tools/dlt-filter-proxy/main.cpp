#include <algorithm>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#include "dlt_proxy/proxy.h"

namespace {

void printUsage(std::string_view program) {
    std::cerr << "Usage: " << program << " --source HOST:PORT --listen PORT [--appid ID ...]\n"
              << "Forwards the DLT messages with the given app IDs (all without --appid) from the source to all\n"
              << "clients. Listens once the source has been connected.\n";
}

std::optional<std::uint16_t> toPort(std::string_view text) {
    std::uint16_t port = 0;
    const auto [end, error] = std::from_chars(text.begin(), text.end(), port);
    if (error != std::errc{} || end != text.end() || port == 0) {
        return std::nullopt;
    }
    return port;
}

// Returns why the options are invalid, or an empty string.
std::string parseOptions(std::span<char*> args, dlt_proxy::Options& options) {
    for (auto arg = std::next(args.begin()); arg != args.end(); ++arg) {
        const std::string name = *arg;
        if (name != "--source" && name != "--listen" && name != "--appid") {
            return "Unknown option: " + name;
        }
        if (std::next(arg) == args.end()) {
            return "Missing value for " + name;
        }
        const std::string_view value = *++arg;
        if (name == "--source") {
            const std::size_t colon = value.rfind(':');
            const auto port = colon == std::string_view::npos ? std::nullopt : toPort(value.substr(colon + 1));
            if (colon == 0 || !port) {
                return "--source needs HOST:PORT, got " + std::string(value);
            }
            options.source_host = value.substr(0, colon);
            options.source_port = *port;
        } else if (name == "--listen") {
            const auto port = toPort(value);
            if (!port) {
                return "--listen needs a port, got " + std::string(value);
            }
            options.listen_port = *port;
        } else {
            const auto app_id = dlt_proxy::toId(value);
            if (!app_id) {
                return "An app ID has 1 to 4 characters, got " + std::string(value);
            }
            options.app_ids.push_back(*app_id);
        }
    }
    if (options.source_host.empty()) {
        return "Missing --source";
    }
    if (options.listen_port == 0) {
        return "Missing --listen";
    }
    return {};
}

}  // namespace

int main(int argc, char** argv) {
    const std::span args(argv, static_cast<std::size_t>(argc));
    if (std::ranges::find(args, std::string_view("--help")) != args.end()) {
        printUsage(args.front());
        return 0;
    }
    dlt_proxy::Options options;
    if (const std::string error = parseOptions(args, options); !error.empty()) {
        std::cerr << error << "\n\n";
        printUsage(args.front());
        return 2;
    }
    options.log = [](const std::string& message) { std::cerr << message << '\n'; };
    dlt_proxy::Proxy proxy(std::move(options));
    proxy.wait();
    return 1;
}
