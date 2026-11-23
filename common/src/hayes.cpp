// SPDX-License-Identifier: GPL-3.0-or-later
#include "tvcnet/hayes.hpp"

#include <algorithm>
#include <cctype>
#include <string>

#include "tvcnet/codes.hpp"

namespace tvcnet {

namespace {

char upper(char c) { return static_cast<char>(std::toupper(static_cast<unsigned char>(c))); }

bool is_error(std::uint8_t status) { return status >= status::kFirstError; }

// A number after a command letter; none is 0
int number(std::string_view line, std::size_t& at) {
    int value = 0;
    while (at < line.size() && std::isdigit(static_cast<unsigned char>(line[at])) && value < 1000) {
        value = value * 10 + (line[at++] - '0');
    }
    return value;
}

// The rest of the line, or the text between quotes if it starts with one
std::string_view argument(std::string_view line, std::size_t& at) {
    while (at < line.size() && line[at] == ' ') {
        ++at;
    }
    std::string_view rest = line.substr(at);
    at = line.size();
    if (!rest.empty() && rest.front() == '"') {
        rest.remove_prefix(1);
        const std::size_t end = rest.find('"');
        if (end != std::string_view::npos) {
            rest = rest.substr(0, end);
        }
    }
    while (!rest.empty() && rest.back() == ' ') {
        rest.remove_suffix(1);
    }
    return rest;
}

std::string dotted(const std::array<std::uint8_t, 4>& ip) {
    return std::to_string(ip[0]) + "." + std::to_string(ip[1]) + "." + std::to_string(ip[2]) + "." + std::to_string(ip[3]);
}

}  // namespace

bool parse_host_port(std::string_view text, std::string_view& host, std::uint16_t& port) {
    const std::size_t colon = text.rfind(':');
    if (colon == std::string_view::npos || colon == 0 || colon + 1 == text.size() || colon + 6 < text.size()) {
        return false;
    }
    unsigned value = 0;
    for (const char c : text.substr(colon + 1)) {
        if (!std::isdigit(static_cast<unsigned char>(c))) {
            return false;
        }
        value = value * 10 + static_cast<unsigned>(c - '0');
    }
    if (value == 0 || value > 65535) {
        return false;
    }
    host = text.substr(0, colon);
    port = static_cast<std::uint16_t>(value);
    return true;
}

void Hayes::reset() {
    mode_ = Mode::Command;
    echo_ = true;
    verbose_ = true;
    quiet_ = false;
    line_.clear();
    last_line_.clear();
    pending_.clear();
    tx_.clear();
    plus_ = 0;
    connection_ = false;
}

void Hayes::drop() {
    mode_ = Mode::Command;
    line_.clear();
    tx_.clear();
    plus_ = 0;
    connection_ = false;
}

std::uint8_t Hayes::exchange(const std::uint8_t* in, std::size_t size, std::uint64_t now_us,
                             std::vector<std::uint8_t>& out, std::size_t max) {
    out.clear();
    if (mode_ == Mode::Online && plus_ > 0 && now_us - plus_us_ >= kGuardUs) {
        if (plus_ == 3) {
            plus_ = 0;
            mode_ = Mode::Command;
            say(kResultOk);
        } else {
            flush_plus();
        }
    }
    for (std::size_t i = 0; i < size; ++i) {
        switch (mode_) {
        case Mode::Command:
            command_byte(in[i], now_us);
            break;
        case Mode::Online:
            online_byte(in[i], now_us);
            break;
        case Mode::Dialing:
        case Mode::Fetching:
            // any key hangs up, the rest of the frame is dropped
            if (net_) {
                net_->close();
            }
            mode_ = Mode::Command;
            say(kNoCarrier);
            i = size;
            break;
        }
    }
    if (!tx_.empty()) {
        if (net_) {
            net_->tcp_send(reinterpret_cast<const std::uint8_t*>(tx_.data()), tx_.size());
        }
        tx_.clear();
    }
    check_dial(now_us);

    auto drain = [&] {
        while (!pending_.empty() && out.size() < max) {
            out.push_back(pending_.front());
            pending_.pop_front();
        }
    };
    drain();
    if ((mode_ == Mode::Online || mode_ == Mode::Fetching) && out.size() < max) {
        std::vector<std::uint8_t> data;
        std::uint16_t detail = 0;
        const std::uint8_t state = net_ ? net_->next(data, max - out.size(), detail) : status::kNone;
        out.insert(out.end(), data.begin(), data.end());
        if (state != status::kData && state != status::kBusy) {
            const bool fetching = mode_ == Mode::Fetching;
            mode_ = Mode::Command;
            connection_ = false;
            plus_ = 0;
            say(!fetching ? kNoCarrier : state == status::kEnd ? kResultOk : kError);
            drain();
        }
    }
    switch (mode_) {
    case Mode::Command: return status::kOk;
    case Mode::Dialing: return status::kBusy;
    default: return status::kData;
    }
}

void Hayes::command_byte(std::uint8_t byte, std::uint64_t now_us) {
    const char c = static_cast<char>(byte);
    if (c == '\r') {
        if (echo_) {
            put("\r");
        }
        const std::string line = line_;
        line_.clear();
        run_line(line, now_us);
        return;
    }
    if (c == '\b' || byte == 0x7F) {
        if (!line_.empty()) {
            line_.pop_back();
            if (echo_) {
                put(std::string(1, c));
            }
        }
        return;
    }
    if (byte < 0x20 || line_.size() >= kMaxLine) {
        return;
    }
    if (echo_) {
        put(std::string(1, c));
    }
    if (c == '/' && line_.size() == 1 && upper(line_[0]) == 'A') {
        line_.clear();
        if (echo_) {
            put("\r");
        }
        run_line(last_line_, now_us);
        return;
    }
    line_.push_back(c);
}

void Hayes::online_byte(std::uint8_t byte, std::uint64_t now_us) {
    if (byte == '+' && plus_ < 3 && (plus_ > 0 || now_us - last_data_us_ >= kGuardUs)) {
        ++plus_;
        plus_us_ = now_us;
        return;
    }
    if (plus_ > 0) {
        flush_plus();
    }
    tx_.push_back(static_cast<char>(byte));
    last_data_us_ = now_us;
}

void Hayes::flush_plus() {
    tx_.append(plus_, '+');
    last_data_us_ = plus_us_;
    plus_ = 0;
}

void Hayes::run_line(std::string_view line, std::uint64_t now_us) {
    while (!line.empty() && line.front() == ' ') {
        line.remove_prefix(1);
    }
    if (line.size() < 2 || upper(line[0]) != 'A' || upper(line[1]) != 'T') {
        return;
    }
    last_line_.assign(line);
    int result = kResultOk;
    std::size_t at = 2;
    while (at < line.size()) {
        if (line[at] == ' ') {
            ++at;
            continue;
        }
        if (!run_command(line, at, result, now_us)) {
            say(kError);
            return;
        }
    }
    if (result >= 0) {
        say(static_cast<Result>(result));
    }
}

bool Hayes::run_command(std::string_view line, std::size_t& at, int& result, std::uint64_t now_us) {
    const char c = upper(line[at++]);
    switch (c) {
    case 'E':
    case 'V':
    case 'Q': {
        const int n = number(line, at);
        if (n > 1) {
            return false;
        }
        (c == 'E' ? echo_ : c == 'V' ? verbose_ : quiet_) = n == 1;
        return true;
    }
    case 'Z':
        number(line, at);
        if (connection_ && net_) {
            net_->close();
        }
        connection_ = false;
        echo_ = true;
        verbose_ = true;
        quiet_ = false;
        return true;
    case 'H':
        number(line, at);
        if (connection_ && net_) {
            net_->close();
        }
        connection_ = false;
        return true;
    case 'O': {
        number(line, at);
        at = line.size();
        const std::uint8_t state = connection_ && net_ ? net_->job_state() : status::kNone;
        if (state == status::kOk || state == status::kData || state == status::kBusy) {
            mode_ = Mode::Online;
            last_data_us_ = now_us;
            result = kConnect;
        } else {
            connection_ = false;
            result = kNoCarrier;
        }
        return true;
    }
    case 'I': {
        const int n = number(line, at);
        const bool wifi = net_ && net_->wifi_connected();
        const std::string ip = dotted(net_ ? net_->ip() : std::array<std::uint8_t, 4>{});
        if (n == 0) {
            text("TVCNET PROTOCOL " + std::to_string(kProtocolVersion));
            text(wifi ? "WIFI CONNECTED " + ip : std::string("WIFI DISCONNECTED"));
        } else if (n == 2) {
            text(ip);
        } else if (n == 4) {
            text(std::to_string(kProtocolVersion));
        } else {
            return false;
        }
        return true;
    }
    case 'W': {
        const std::string_view arg = argument(line, at);
        const std::size_t comma = arg.find(',');
        return comma != std::string_view::npos && comma != 0 && net_ &&
               net_->wifi_join(arg.substr(0, comma), arg.substr(comma + 1));
    }
    case 'D': {
        while (at < line.size() && (upper(line[at]) == 'T' || upper(line[at]) == 'P')) {
            ++at;
        }
        std::string_view host;
        std::uint16_t port = 0;
        if (!parse_host_port(argument(line, at), host, port)) {
            return false;
        }
        connection_ = false;
        if (!net_ || is_error(net_->tcp_open(host, port))) {
            result = kNoCarrier;
            return true;
        }
        mode_ = Mode::Dialing;
        dial_start_us_ = now_us;
        result = -1;
        return true;
    }
    case '&': {
        if (at >= line.size()) {
            return false;
        }
        const char sub = upper(line[at++]);
        if (sub == 'F') {
            number(line, at);
            echo_ = true;
            verbose_ = true;
            quiet_ = false;
            return true;
        }
        if (sub == 'G') {
            const std::string_view url = argument(line, at);
            if (url.empty() || !net_) {
                return false;
            }
            connection_ = false;
            if (is_error(net_->http_get(url))) {
                return false;
            }
            mode_ = Mode::Fetching;
            result = -1;
            return true;
        }
        return false;
    }
    default:
        return false;
    }
}

void Hayes::check_dial(std::uint64_t now_us) {
    if (mode_ != Mode::Dialing) {
        return;
    }
    const std::uint8_t state = net_ ? net_->job_state() : status::kNone;
    if (state == status::kOk || state == status::kData) {
        mode_ = Mode::Online;
        connection_ = true;
        last_data_us_ = now_us;
        say(kConnect);
    } else if (state != status::kBusy || now_us - dial_start_us_ >= kDialUs) {
        if (net_) {
            net_->close();
        }
        mode_ = Mode::Command;
        say(kNoCarrier);
    }
}

void Hayes::say(Result result) {
    if (quiet_) {
        return;
    }
    if (!verbose_) {
        put(std::to_string(static_cast<int>(result)) + "\r");
        return;
    }
    static constexpr const char* kWords[] = {"OK", "CONNECT", "RING", "NO CARRIER", "ERROR"};
    put(std::string("\r\n") + kWords[result] + "\r\n");
}

void Hayes::text(std::string_view line) {
    put("\r\n");
    put(line);
    put("\r\n");
}

void Hayes::put(std::string_view bytes) { pending_.insert(pending_.end(), bytes.begin(), bytes.end()); }

}  // namespace tvcnet
