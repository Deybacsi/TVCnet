// SPDX-License-Identifier: GPL-3.0-or-later
#include "rmt_line.hpp"

#include <esp_timer.h>

namespace {

constexpr std::uint32_t kTickHz = 1000000;      // 1 RMT tick = 1 us
constexpr std::uint32_t kMaxDuration = 32767;   // 15-bit symbol halves

}  // namespace

bool RmtLine::begin() {
    if (!rmtInit(pin_, RMT_TX_MODE, RMT_MEM_NUM_BLOCKS_1, kTickHz)) {
        return false;
    }
    return rmtSetEOT(pin_, level_ ? HIGH : LOW);
}

void RmtLine::send(const std::vector<std::uint32_t>& halfperiods, std::uint64_t start_us) {
    if (halfperiods.empty()) {
        return;
    }
    // Every entry is one level; the line flips at the start and after every entry
    symbols_.clear();
    symbols_.reserve((halfperiods.size() + 1) / 2);
    bool level = !level_;
    for (std::size_t i = 0; i < halfperiods.size(); i += 2) {
        rmt_data_t symbol{};
        symbol.duration0 = std::min(halfperiods[i], kMaxDuration);
        symbol.level0 = level;
        if (i + 1 < halfperiods.size()) {
            symbol.duration1 = std::min(halfperiods[i + 1], kMaxDuration);
            symbol.level1 = !level;
        } else {
            symbol.duration1 = 0;
            symbol.level1 = level;
        }
        symbols_.push_back(symbol);
    }
    // After the last entry the line flips once more and stays there
    level_ = (halfperiods.size() % 2 == 1) ? level_ : !level_;
    rmtSetEOT(pin_, level_ ? HIGH : LOW);
    while (static_cast<std::uint64_t>(esp_timer_get_time()) < start_us) {
    }
    rmtWriteAsync(pin_, symbols_.data(), symbols_.size());
}

bool RmtLine::sending() const { return !rmtTransmitCompleted(pin_); }
