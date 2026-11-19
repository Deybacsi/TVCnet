// SPDX-License-Identifier: GPL-3.0-or-later
// The TVCnet -> TVC line code (PROTOKOLL.md): every half-period between two edges is one bit,
// T long = 1, 2T long = 0, LSB first; a preamble of ones before the bytes and a few ones after them.
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace tvcnet {

struct LineCode {
    std::uint32_t t_us = 250;
    std::uint16_t preamble = 32;
    std::uint16_t trailer = 4;
};

// Appends the half-periods in microseconds. The line toggles when the list starts and at the end of every entry,
// so the last data bit is closed by an edge.
void encode_line(const LineCode& code, const std::uint8_t* data, std::size_t size, std::vector<std::uint32_t>& out);

// Decodes half-periods back into bytes after the preamble and the first `sync` byte (tests and logs).
std::vector<std::uint8_t> decode_line(const LineCode& code, const std::vector<std::uint32_t>& halves, std::uint8_t sync);

}  // namespace tvcnet
