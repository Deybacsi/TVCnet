// SPDX-License-Identifier: GPL-3.0-or-later
#include "tvcnet/frame.hpp"

#include <algorithm>

#include "tvcnet/codes.hpp"

namespace tvcnet {

std::uint8_t checksum(std::uint8_t len, std::uint8_t ctrl, const std::uint8_t* data, std::size_t size) {
    unsigned sum = len + ctrl;
    for (std::size_t i = 0; i < size; ++i) {
        sum += data[i];
    }
    return static_cast<std::uint8_t>(sum);
}

void build_frame(std::uint8_t sync, std::uint8_t ctrl, const std::uint8_t* data, std::size_t size,
                 std::vector<std::uint8_t>& out) {
    size = std::min(size, kMaxData);
    const auto len = static_cast<std::uint8_t>(size);
    out.push_back(sync);
    out.push_back(len);
    out.push_back(ctrl);
    out.insert(out.end(), data, data + size);
    out.push_back(checksum(len, ctrl, data, size));
}

void FrameReceiver::reset() {
    state_ = State::Sync;
    data_.clear();
}

bool FrameReceiver::feed(std::uint8_t byte, std::uint64_t now_us) {
    if (state_ != State::Sync && now_us - last_us_ > kGapUs) {
        ++bad_frames_;
        reset();
    }
    last_us_ = now_us;
    switch (state_) {
    case State::Sync:
        if (byte == sync_) {
            data_.clear();
            state_ = State::Len;
        }
        return false;
    case State::Len:
        len_ = byte;
        state_ = State::Ctrl;
        return false;
    case State::Ctrl:
        ctrl_ = byte;
        state_ = len_ == 0 ? State::Chk : State::Data;
        return false;
    case State::Data:
        data_.push_back(byte);
        if (data_.size() == len_) {
            state_ = State::Chk;
        }
        return false;
    case State::Chk:
        state_ = State::Sync;
        if (byte != checksum(len_, ctrl_, data_.data(), data_.size())) {
            ++bad_frames_;
            return false;
        }
        return true;
    }
    return false;
}

}  // namespace tvcnet
