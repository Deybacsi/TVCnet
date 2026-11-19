// SPDX-License-Identifier: GPL-3.0-or-later
// Host tests of the shared code: frames, line code, modem commands with a fake network.
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "tvcnet/codes.hpp"
#include "tvcnet/frame.hpp"
#include "tvcnet/line_code.hpp"
#include "tvcnet/modem.hpp"

using namespace tvcnet;

namespace {

int failures = 0;

void check(bool ok, const char* what, int line) {
    if (!ok) {
        std::fprintf(stderr, "FAIL line %d: %s\n", line, what);
        ++failures;
    }
}
#define CHECK(x) check((x), #x, __LINE__)

struct FakeOut final : LineOut {
    std::vector<std::uint32_t> sent;
    std::uint64_t start = 0;
    int sends = 0;
    void send(const std::vector<std::uint32_t>& halves, std::uint64_t start_us) override {
        sent = halves;
        start = start_us;
        ++sends;
    }
    bool sending() const override { return false; }
};

struct FakeNet final : Network {
    std::string body;
    std::size_t pos = 0;
    bool job = false;
    bool done = false;
    std::string url;
    bool wifi_connected() const override { return true; }
    std::array<std::uint8_t, 4> ip() const override { return {192, 168, 0, 9}; }
    bool wifi_join(std::string_view, std::string_view) override { return true; }
    std::uint8_t http_get(std::string_view u) override {
        url = std::string(u);
        job = true;
        pos = 0;
        return status::kOk;
    }
    std::uint8_t next(std::vector<std::uint8_t>& out, std::size_t max, std::uint16_t&) override {
        out.clear();
        if (!job) {
            return status::kNone;
        }
        if (!done) {
            return status::kBusy;
        }
        const std::size_t n = std::min(max, body.size() - pos);
        out.assign(body.begin() + static_cast<long>(pos), body.begin() + static_cast<long>(pos + n));
        pos += n;
        return pos == body.size() ? status::kEnd : status::kData;
    }
    std::uint8_t job_state() const override { return job ? status::kBusy : status::kNone; }
    std::int64_t content_length() const override { return -1; }
    void close() override { job = false; }
};

std::vector<std::uint8_t> request(std::uint8_t ctrl, const std::string& data = {}) {
    std::vector<std::uint8_t> frame;
    build_frame(kSyncFromTvc, ctrl, reinterpret_cast<const std::uint8_t*>(data.data()), data.size(), frame);
    return frame;
}

// Sends a request, returns the reply frame decoded from the line (without SYNC)
std::vector<std::uint8_t> ask(Modem& modem, FakeOut& out, std::uint64_t& now, const std::vector<std::uint8_t>& req) {
    for (const std::uint8_t byte : req) {
        now += 4167;  // one 2400-baud character
        modem.on_byte(byte, now);
    }
    modem.poll(now);
    return decode_line(modem.config().line, out.sent, kSyncToTvc);
}

void test_line_code() {
    const LineCode code;
    const std::vector<std::uint8_t> bytes{kSyncToTvc, 0x03, 0x03, 0x41, 0x04, 0x42, 0xCD};
    std::vector<std::uint32_t> halves;
    encode_line(code, bytes.data(), bytes.size(), halves);
    CHECK(halves.size() == code.preamble + code.trailer + bytes.size() * 8);
    const auto back = decode_line(code, halves, kSyncToTvc);
    CHECK(back == std::vector<std::uint8_t>(bytes.begin() + 1, bytes.end()));
}

void test_frames() {
    FrameReceiver rx(kSyncFromTvc);
    const auto frame = request(0x82, "AB\x04");
    bool done = false;
    std::uint64_t t = 0;
    for (const std::uint8_t b : frame) {
        done = rx.feed(b, t += 100);
    }
    CHECK(done && rx.ctrl() == 0x82 && rx.data().size() == 3 && rx.data()[2] == 0x04);
    auto bad = frame;
    bad.back() ^= 1;
    done = false;
    for (const std::uint8_t b : bad) {
        done = rx.feed(b, t += 100);
    }
    CHECK(!done && rx.bad_frames() == 1);
    // A gap drops the half-received frame
    rx.feed(kSyncFromTvc, t += 100);
    rx.feed(0x01, t += 100);
    rx.feed(0x01, t += FrameReceiver::kGapUs + 1);
    CHECK(rx.bad_frames() == 2);
}

void test_modem() {
    FakeOut out;
    FakeNet net;
    Modem modem(out, &net);
    std::uint64_t now = 0;

    auto reply = ask(modem, out, now, request(cmd::kPing));
    CHECK(reply.size() == 4 && reply[0] == 1 && reply[1] == status::kOk && reply[2] == kProtocolVersion);
    CHECK(out.start == now + modem.config().turnaround_us);

    reply = ask(modem, out, now, request(kToggleBit | cmd::kEcho, std::string("hi\x6A\x04", 4)));
    CHECK(reply.size() == 7 && reply[1] == (kToggleBit | status::kOk) && reply[4] == 0x6A && reply[5] == 0x04);

    reply = ask(modem, out, now, request(cmd::kGet, "http://x/"));
    CHECK(reply[1] == status::kOk && net.url == "http://x/");
    reply = ask(modem, out, now, request(kToggleBit | cmd::kNext));
    CHECK(reply[1] == (kToggleBit | status::kBusy));

    net.done = true;
    net.body.assign(300, 'z');
    reply = ask(modem, out, now, request(cmd::kNext));
    CHECK(reply[0] == 255 && reply[1] == status::kData);
    // The same request again (a lost reply) gets the same chunk, the job does not move on
    reply = ask(modem, out, now, request(cmd::kNext));
    CHECK(reply[0] == 255 && reply[1] == status::kData && net.pos == 255 && modem.stats().repeats == 1);
    reply = ask(modem, out, now, request(kToggleBit | cmd::kNext, std::string(1, '\x10')));
    CHECK(reply[0] == 16 && reply[1] == (kToggleBit | status::kData));
    reply = ask(modem, out, now, request(cmd::kNext));
    CHECK(reply[0] == 29 && reply[1] == status::kEnd);

    reply = ask(modem, out, now, request(kToggleBit | cmd::kStatus));
    CHECK(reply[0] == 10 && reply[2] == 1 && reply[3] == 192 && reply[6] == 9 && reply[8] == 0xFF);

    reply = ask(modem, out, now, request(0x7E));
    CHECK(reply[1] == status::kErrUnknown);
    reply = ask(modem, out, now, request(kToggleBit | cmd::kWifi, "nocomma"));
    CHECK(reply[1] == (kToggleBit | status::kErrParam));

    Modem offline(out, nullptr);
    reply = ask(offline, out, now, request(cmd::kGet, "http://x/"));
    CHECK(reply[1] == status::kErrNoWifi);
}

}  // namespace

int main() {
    test_line_code();
    test_frames();
    test_modem();
    if (failures) {
        std::fprintf(stderr, "%d check(s) failed\n", failures);
        return EXIT_FAILURE;
    }
    std::puts("all checks passed");
    return EXIT_SUCCESS;
}
