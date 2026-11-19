// SPDX-License-Identifier: GPL-3.0-or-later
#include "tvcnet/modem.hpp"

#include <algorithm>
#include <cstdio>
#include <string_view>

#include "tvcnet/codes.hpp"

namespace tvcnet {

namespace {

void put_le(std::vector<std::uint8_t>& out, std::uint64_t value, unsigned bytes) {
    for (unsigned i = 0; i < bytes; ++i) {
        out.push_back(static_cast<std::uint8_t>(value >> (8 * i)));
    }
}

}  // namespace

Modem::Modem(LineOut& out, Network* net, const ModemConfig& config, Log* log)
    : out_(out), net_(net), config_(config), log_(log) {}

void Modem::reset() {
    receiver_.reset();
    last_request_.clear();
    last_reply_.clear();
    pending_.clear();
    waiting_ = false;
}

ModemStats Modem::stats() const {
    ModemStats stats = stats_;
    stats.bad_frames = receiver_.bad_frames();
    return stats;
}

void Modem::on_byte(std::uint8_t byte, std::uint64_t now_us) {
    if (receiver_.feed(byte, now_us)) {
        handle(now_us);
    }
}

void Modem::handle(std::uint64_t now_us) {
    std::vector<std::uint8_t> request;
    build_frame(kSyncFromTvc, receiver_.ctrl(), receiver_.data().data(), receiver_.data().size(), request);
    log_frame("TVC>", request, false);
    if (request == last_request_ && !last_reply_.empty()) {
        ++stats_.repeats;
    } else {
        ++stats_.requests;
        std::uint8_t status = status::kErrUnknown;
        std::vector<std::uint8_t> data;
        execute(receiver_.ctrl() & kCodeMask, receiver_.data(), status, data);
        last_request_ = std::move(request);
        last_reply_.clear();
        build_frame(kSyncToTvc, static_cast<std::uint8_t>((receiver_.ctrl() & kToggleBit) | status), data.data(),
                    data.size(), last_reply_);
    }
    log_frame("<TVC", last_reply_, true);
    pending_.clear();
    encode_line(config_.line, last_reply_.data(), last_reply_.size(), pending_);
    due_us_ = now_us + config_.turnaround_us;
    waiting_ = true;
}

void Modem::poll(std::uint64_t) {
    if (waiting_ && !out_.sending()) {
        out_.send(pending_, due_us_);
        waiting_ = false;
    }
}

void Modem::execute(std::uint8_t code, const std::vector<std::uint8_t>& data, std::uint8_t& status,
                    std::vector<std::uint8_t>& reply) {
    status = status::kOk;
    switch (code) {
    case cmd::kPing:
        reply.push_back(kProtocolVersion);
        return;
    case cmd::kEcho:
        reply = data;
        return;
    case cmd::kStatus: {
        // wifi up, IPv4, job state, whole length (FFFFFFFFh: unknown)
        reply.push_back(net_ && net_->wifi_connected() ? 1 : 0);
        const std::array<std::uint8_t, 4> ip = net_ ? net_->ip() : std::array<std::uint8_t, 4>{};
        reply.insert(reply.end(), ip.begin(), ip.end());
        reply.push_back(net_ ? net_->job_state() : status::kNone);
        const std::int64_t length = net_ ? net_->content_length() : -1;
        put_le(reply, length < 0 || length > 0xFFFFFFFE ? 0xFFFFFFFFu : static_cast<std::uint64_t>(length), 4);
        return;
    }
    case cmd::kWifi: {
        const std::string_view text(reinterpret_cast<const char*>(data.data()), data.size());
        const std::size_t comma = text.find(',');
        if (comma == std::string_view::npos || comma == 0) {
            status = status::kErrParam;
        } else if (!net_) {
            status = status::kErrNoWifi;
        } else if (!net_->wifi_join(text.substr(0, comma), text.substr(comma + 1))) {
            status = status::kErrParam;
        }
        return;
    }
    case cmd::kGet:
        if (data.empty()) {
            status = status::kErrParam;
        } else if (!net_) {
            status = status::kErrNoWifi;
        } else {
            status = net_->http_get(std::string_view(reinterpret_cast<const char*>(data.data()), data.size()));
        }
        return;
    case cmd::kNext: {
        if (!net_) {
            status = status::kNone;
            return;
        }
        const std::size_t max = !data.empty() && data[0] != 0 ? data[0] : kMaxData;
        std::uint16_t detail = 0;
        status = net_->next(reply, max, detail);
        if (reply.size() > max) {
            reply.resize(max);
        }
        if (status == status::kErrHttp) {
            reply.clear();
            put_le(reply, detail, 2);
        }
        return;
    }
    case cmd::kClose:
        if (net_) {
            net_->close();
        }
        return;
    default:
        status = status::kErrUnknown;
        return;
    }
}

void Modem::log_frame(const char* what, const std::vector<std::uint8_t>& frame, bool to_tvc) {
    if (!log_ || frame.size() < 4) {
        return;
    }
    const std::uint8_t ctrl = frame[2];
    const std::uint8_t code = ctrl & kCodeMask;
    char head[64];
    std::snprintf(head, sizeof head, "%s %s bit=%d len=%u:", what, to_tvc ? status_name(code) : command_name(code),
                  (ctrl & kToggleBit) ? 1 : 0, static_cast<unsigned>(frame[1]));
    std::string text(head);
    const std::size_t shown = std::min<std::size_t>(frame.size() - 4, 32);
    for (std::size_t i = 0; i < shown; ++i) {
        char byte[4];
        std::snprintf(byte, sizeof byte, " %02X", frame[3 + i]);
        text += byte;
    }
    if (shown < frame.size() - 4) {
        text += " ...";
    }
    log_->line(text);
}

}  // namespace tvcnet
