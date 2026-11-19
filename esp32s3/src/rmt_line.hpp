// SPDX-License-Identifier: GPL-3.0-or-later
// The tape input line towards the TVC (DIN pin 3) on an RMT channel: half-periods become RMT symbols.
#pragma once

#include <Arduino.h>

#include <vector>

#include "tvcnet/hal.hpp"

class RmtLine final : public tvcnet::LineOut {
public:
    explicit RmtLine(int pin) : pin_(pin) {}
    bool begin();
    void send(const std::vector<std::uint32_t>& halfperiods, std::uint64_t start_us) override;
    bool sending() const override;

private:
    int pin_;
    bool level_ = false;  // the line's level between transmissions
    std::vector<rmt_data_t> symbols_;
};
