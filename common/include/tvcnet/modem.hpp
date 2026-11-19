// SPDX-License-Identifier: GPL-3.0-or-later
// TVCnet's logic above the physical layer: frames from the TVC, the alternating bit, the commands, the replies
// as line-coded half-periods (PROTOKOLL.md). Event driven: the host feeds bytes and polls with its clock.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "tvcnet/codes.hpp"
#include "tvcnet/frame.hpp"
#include "tvcnet/hal.hpp"
#include "tvcnet/line_code.hpp"

namespace tvcnet {

struct ModemConfig {
    LineCode line;
    // From the end of a request's last byte to the reply's first edge: the TVC turns to listening meanwhile
    std::uint32_t turnaround_us = 2000;
};

struct ModemStats {
    std::uint32_t requests = 0;
    std::uint32_t repeats = 0;     // requests answered again from the stored reply
    std::uint32_t bad_frames = 0;  // dropped for a checksum or a gap
};

class Modem {
public:
    // `net` may be null: every network command then answers status::kErrNoWifi.
    Modem(LineOut& out, Network* net, const ModemConfig& config = {}, Log* log = nullptr);

    // A byte from the TVC (the UART on the motor line) that ended at `now_us`.
    void on_byte(std::uint8_t byte, std::uint64_t now_us);
    // Sends a reply whose time has come.
    void poll(std::uint64_t now_us);
    // Forgets the half-received frame, the waiting reply and the stored one (power-on; a host clock restart).
    void reset();

    void set_network(Network* net) { net_ = net; }
    const ModemConfig& config() const { return config_; }
    void set_config(const ModemConfig& config) { config_ = config; }
    ModemStats stats() const;
    // The last request's and reply's frame bytes, for logs and the emulator's monitor
    const std::vector<std::uint8_t>& last_request() const { return last_request_; }
    const std::vector<std::uint8_t>& last_reply() const { return last_reply_; }

private:
    void handle(std::uint64_t now_us);
    void execute(std::uint8_t code, const std::vector<std::uint8_t>& data, std::uint8_t& status,
                 std::vector<std::uint8_t>& reply);
    void log_frame(const char* what, const std::vector<std::uint8_t>& frame, bool to_tvc);

    LineOut& out_;
    Network* net_;
    ModemConfig config_;
    Log* log_;
    FrameReceiver receiver_{kSyncFromTvc};
    ModemStats stats_;
    std::vector<std::uint8_t> last_request_;  // SYNC to CHK
    std::vector<std::uint8_t> last_reply_;
    std::vector<std::uint32_t> pending_;      // line-coded reply waiting for its time
    std::uint64_t due_us_ = 0;
    bool waiting_ = false;
};

}  // namespace tvcnet
