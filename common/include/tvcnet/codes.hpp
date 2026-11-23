// SPDX-License-Identifier: GPL-3.0-or-later
// Frame markers, command and status codes of the TVC <-> TVCnet protocol (PROTOKOLL.md).
#pragma once

#include <cstddef>
#include <cstdint>

namespace tvcnet {

inline constexpr std::uint8_t kProtocolVersion = 2;

inline constexpr std::uint8_t kSyncFromTvc = 0xA5;
inline constexpr std::uint8_t kSyncToTvc = 0x6A;
// CTRL b7: the alternating bit; b6-b0: the command or status code
inline constexpr std::uint8_t kToggleBit = 0x80;
inline constexpr std::uint8_t kCodeMask = 0x7F;
inline constexpr std::size_t kMaxData = 255;

namespace cmd {
inline constexpr std::uint8_t kPing = 0x01;
inline constexpr std::uint8_t kEcho = 0x02;
inline constexpr std::uint8_t kStatus = 0x03;
inline constexpr std::uint8_t kWifi = 0x04;
inline constexpr std::uint8_t kGet = 0x10;
inline constexpr std::uint8_t kNext = 0x11;
inline constexpr std::uint8_t kClose = 0x12;
inline constexpr std::uint8_t kOpen = 0x20;
inline constexpr std::uint8_t kSend = 0x21;
inline constexpr std::uint8_t kTerm = 0x30;
}  // namespace cmd

namespace status {
inline constexpr std::uint8_t kOk = 0x00;
inline constexpr std::uint8_t kNone = 0x01;
inline constexpr std::uint8_t kBusy = 0x02;
inline constexpr std::uint8_t kData = 0x03;
inline constexpr std::uint8_t kEnd = 0x04;
inline constexpr std::uint8_t kErrUnknown = 0x40;
inline constexpr std::uint8_t kErrParam = 0x41;
inline constexpr std::uint8_t kErrNoWifi = 0x42;
inline constexpr std::uint8_t kErrDns = 0x43;
inline constexpr std::uint8_t kErrConnect = 0x44;
inline constexpr std::uint8_t kErrHttp = 0x45;
inline constexpr std::uint8_t kErrLost = 0x46;
inline constexpr std::uint8_t kFirstError = 0x40;
}  // namespace status

// A short English name for logs ("PING", "DATA", "ERR_HTTP", ...); "?" if unknown.
const char* command_name(std::uint8_t code);
const char* status_name(std::uint8_t code);

}  // namespace tvcnet
