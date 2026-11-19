// SPDX-License-Identifier: GPL-3.0-or-later
// What the shared TVCnet code needs from its host: the ESP32-S3 firmware or the freeTVC emulator.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace tvcnet {

// The tape input line towards the TVC (DIN pin 3).
class LineOut {
public:
    virtual ~LineOut() = default;
    // Plays half-periods in microseconds: an edge at `start_us` (or now, if that has passed), then one at the end of
    // every entry. A call while still sending replaces nothing; the caller waits for !sending().
    virtual void send(const std::vector<std::uint32_t>& halfperiods, std::uint64_t start_us) = 0;
    virtual bool sending() const = 0;
};

// WiFi and the one network job (an HTTP GET; later a TCP connection). Calls come from the modem and must not block.
class Network {
public:
    virtual ~Network() = default;
    virtual bool wifi_connected() const = 0;
    // 0.0.0.0 without a connection
    virtual std::array<std::uint8_t, 4> ip() const = 0;
    // Stores the credentials and starts joining; false if they are unusable.
    virtual bool wifi_join(std::string_view ssid, std::string_view password) = 0;
    // Starts a GET of `url` (http:// or https://), closing any earlier job; status::kOk or an error code.
    virtual std::uint8_t http_get(std::string_view url) = 0;
    // Moves up to `max` bytes of the job's data into `out` (cleared first). status::kNone (no job), kBusy (no data
    // yet), kData (more may follow), kEnd (these are the last) or an error code; `detail` is the HTTP status for
    // kErrHttp.
    virtual std::uint8_t next(std::vector<std::uint8_t>& out, std::size_t max, std::uint16_t& detail) = 0;
    // status::kNone, kBusy, kData (data is waiting), kEnd (finished, nothing left) or an error code
    virtual std::uint8_t job_state() const = 0;
    // The job's whole length when the server told it, else -1
    virtual std::int64_t content_length() const = 0;
    virtual void close() = 0;
};

// Optional log lines (USB serial on the ESP, the monitor in freeTVC).
class Log {
public:
    virtual ~Log() = default;
    virtual void line(std::string_view text) = 0;
};

}  // namespace tvcnet
