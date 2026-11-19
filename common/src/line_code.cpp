// SPDX-License-Identifier: GPL-3.0-or-later
#include "tvcnet/line_code.hpp"

namespace tvcnet {

void encode_line(const LineCode& code, const std::uint8_t* data, std::size_t size, std::vector<std::uint32_t>& out) {
    const std::uint32_t one = code.t_us;
    const std::uint32_t zero = 2 * code.t_us;
    out.reserve(out.size() + code.preamble + code.trailer + size * 8);
    out.insert(out.end(), code.preamble, one);
    for (std::size_t i = 0; i < size; ++i) {
        for (unsigned bit = 0; bit < 8; ++bit) {
            out.push_back((data[i] >> bit) & 1 ? one : zero);
        }
    }
    out.insert(out.end(), code.trailer, one);
}

std::vector<std::uint8_t> decode_line(const LineCode& code, const std::vector<std::uint32_t>& halves, std::uint8_t sync) {
    std::vector<std::uint8_t> bytes;
    const std::uint32_t threshold = code.t_us + code.t_us / 2;
    unsigned shift = 0;
    unsigned seen = 0;
    bool synced = false;
    for (const std::uint32_t half : halves) {
        const unsigned bit = half < threshold ? 1 : 0;
        shift = (shift >> 1) | (bit << 7);
        ++seen;
        if (!synced) {
            if (seen >= 8 && shift == sync) {
                synced = true;
                seen = 0;
            }
        } else if (seen == 8) {
            bytes.push_back(static_cast<std::uint8_t>(shift));
            seen = 0;
        }
    }
    return bytes;
}

}  // namespace tvcnet
