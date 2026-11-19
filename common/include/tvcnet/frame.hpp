// SPDX-License-Identifier: GPL-3.0-or-later
// Frames (PROTOKOLL.md): SYNC, LEN, CTRL, LEN data bytes, CHK = 8-bit sum of LEN, CTRL and the data.
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace tvcnet {

std::uint8_t checksum(std::uint8_t len, std::uint8_t ctrl, const std::uint8_t* data, std::size_t size);

// Appends a whole frame; `size` is cut to kMaxData.
void build_frame(std::uint8_t sync, std::uint8_t ctrl, const std::uint8_t* data, std::size_t size,
                 std::vector<std::uint8_t>& out);

// Collects one frame from received bytes. Bytes are taken by their place, never by their value; a gap longer
// than kGapUs between two bytes drops a half-received frame.
class FrameReceiver {
public:
    static constexpr std::uint64_t kGapUs = 20000;

    explicit FrameReceiver(std::uint8_t sync) : sync_(sync) {}

    // true when `byte` completed a frame with a good checksum; it stays readable until the next feed().
    bool feed(std::uint8_t byte, std::uint64_t now_us);
    void reset();

    std::uint8_t ctrl() const { return ctrl_; }
    const std::vector<std::uint8_t>& data() const { return data_; }
    // Frames dropped for a bad checksum or a gap since construction
    std::uint32_t bad_frames() const { return bad_frames_; }

private:
    enum class State { Sync, Len, Ctrl, Data, Chk };

    std::uint8_t sync_;
    State state_ = State::Sync;
    std::uint8_t len_ = 0;
    std::uint8_t ctrl_ = 0;
    std::vector<std::uint8_t> data_;
    std::uint64_t last_us_ = 0;
    std::uint32_t bad_frames_ = 0;
};

}  // namespace tvcnet
