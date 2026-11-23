// SPDX-License-Identifier: GPL-3.0-or-later
// The TERM command's Hayes modem (PROTOKOLL.md): AT commands in command mode, a byte pipe to the network job
// while online, +++ with a guard time back to command mode. It runs only when the TVC asks, so it never speaks first.
#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>
#include <string_view>
#include <vector>

#include "tvcnet/hal.hpp"

namespace tvcnet {

// "host:port" with a port of 1-65535; false otherwise.
bool parse_host_port(std::string_view text, std::string_view& host, std::uint16_t& port);

class Hayes {
public:
    enum class Mode { Command, Dialing, Online, Fetching };

    static constexpr std::uint64_t kGuardUs = 1000000;   // S12 = 50
    static constexpr std::uint64_t kDialUs = 30000000;   // S7 = 30
    static constexpr std::size_t kMaxLine = 80;

    explicit Hayes(Network* net) : net_(net) {}

    // One TERM exchange: `in` arrived from the TVC at `now_us`; `out` gets up to `max` bytes for the TVC. Returns
    // the reply's status: status::kOk in command mode, kBusy while dialing, kData online or fetching.
    std::uint8_t exchange(const std::uint8_t* in, std::size_t size, std::uint64_t now_us, std::vector<std::uint8_t>& out,
                          std::size_t max);
    // Factory settings and command mode; the network job is left alone.
    void reset();
    // The network job was replaced or closed by another command: back to command mode without a result.
    void drop();

    void set_network(Network* net) { net_ = net; }
    Mode mode() const { return mode_; }

private:
    enum Result { kResultOk = 0, kConnect = 1, kNoCarrier = 3, kError = 4 };

    void command_byte(std::uint8_t byte, std::uint64_t now_us);
    void online_byte(std::uint8_t byte, std::uint64_t now_us);
    void run_line(std::string_view line, std::uint64_t now_us);
    // false: ERROR; `result` -1: the result comes later (a dial, a fetch)
    bool run_command(std::string_view line, std::size_t& at, int& result, std::uint64_t now_us);
    void check_dial(std::uint64_t now_us);
    void flush_plus();
    void say(Result result);
    void text(std::string_view line);
    void put(std::string_view bytes);

    Network* net_;
    Mode mode_ = Mode::Command;
    bool echo_ = true;
    bool verbose_ = true;
    bool quiet_ = false;
    std::string line_;
    std::string last_line_;
    std::deque<std::uint8_t> pending_;  // echo and results, sent before network data
    std::string tx_;                     // this exchange's bytes for the connection
    std::uint64_t dial_start_us_ = 0;
    std::uint64_t last_data_us_ = 0;     // the last byte sent to the connection
    unsigned plus_ = 0;                  // '+' held back as a possible escape
    std::uint64_t plus_us_ = 0;
    bool connection_ = false;            // a TCP connection ATO could go back to
};

}  // namespace tvcnet
