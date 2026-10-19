#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace dlt_proxy {

using Id = std::array<char, 4>;

/* Pads IDs shorter than four characters with zeros, as DLT does */
std::optional<Id> toId(std::string_view text);

struct Options {
    std::string source_host;
    std::uint16_t source_port = 0;
    std::uint16_t listen_port = 0;
    /* Empty forwards every message */
    std::vector<Id> app_ids;
    std::function<void(const std::string&)> log;
};

/*
 * Forwards the DLT messages with selected app IDs from one DLT server (e.g.
 * dlt-daemon) to any number of clients (e.g. dlt-viewer). Only the headers
 * are read; matching messages are forwarded unchanged. Listens once the
 * source has been connected. Runs on its own thread while the object exists.
 */
class Proxy {
 public:
    explicit Proxy(Options options);
    /* Stops and joins the thread */
    ~Proxy();
    Proxy(const Proxy&) = delete;
    Proxy& operator=(const Proxy&) = delete;
    Proxy(Proxy&&) = delete;
    Proxy& operator=(Proxy&&) = delete;

    /* Returns once the proxy has stopped after a fatal error */
    void wait();

 private:
    class Impl;
    std::unique_ptr<Impl> impl_;
    std::jthread thread_;
};

}  // namespace dlt_proxy
