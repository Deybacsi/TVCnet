// SPDX-License-Identifier: GPL-3.0-or-later
// Host tests of the shared code: frames, line code, modem commands and the Hayes modem with a fake network.
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "tvcnet/codes.hpp"
#include "tvcnet/frame.hpp"
#include "tvcnet/hayes.hpp"
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
    enum class Kind { None, Http, Tcp };
    Kind kind = Kind::None;
    std::string body;
    std::size_t pos = 0;
    bool done = false;       // HTTP: the whole body is in; TCP: connected
    bool peer_closed = false;
    std::uint8_t error = 0;
    std::string url;
    std::string host;
    std::uint16_t port = 0;
    std::string sent;
    bool wifi_connected() const override { return true; }
    std::array<std::uint8_t, 4> ip() const override { return {192, 168, 0, 9}; }
    bool wifi_join(std::string_view, std::string_view) override { return true; }
    void start(Kind k) {
        kind = k;
        body.clear();
        pos = 0;
        done = false;
        peer_closed = false;
        error = 0;
        sent.clear();
    }
    std::uint8_t http_get(std::string_view u) override {
        start(Kind::Http);
        url = std::string(u);
        return status::kOk;
    }
    std::uint8_t tcp_open(std::string_view h, std::uint16_t p) override {
        start(Kind::Tcp);
        host = std::string(h);
        port = p;
        return status::kOk;
    }
    std::uint8_t tcp_send(const std::uint8_t* data, std::size_t size) override {
        if (kind != Kind::Tcp) {
            return status::kNone;
        }
        sent.append(reinterpret_cast<const char*>(data), size);
        return status::kOk;
    }
    std::uint8_t next(std::vector<std::uint8_t>& out, std::size_t max, std::uint16_t&) override {
        out.clear();
        if (kind == Kind::None) {
            return status::kNone;
        }
        if (error) {
            kind = Kind::None;
            return error;
        }
        if (!done) {
            return status::kBusy;
        }
        const std::size_t n = std::min(max, body.size() - pos);
        out.assign(body.begin() + static_cast<long>(pos), body.begin() + static_cast<long>(pos + n));
        pos += n;
        if (pos < body.size()) {
            return status::kData;
        }
        if (kind == Kind::Http || peer_closed) {
            kind = Kind::None;
            return status::kEnd;
        }
        return n ? status::kData : status::kBusy;
    }
    std::uint8_t job_state() const override {
        if (kind == Kind::None) {
            return status::kNone;
        }
        if (error) {
            return error;
        }
        if (!done) {
            return status::kBusy;
        }
        if (pos < body.size()) {
            return status::kData;
        }
        return kind == Kind::Http || peer_closed ? status::kEnd : status::kOk;
    }
    std::int64_t content_length() const override { return -1; }
    void close() override { kind = Kind::None; }
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

// One TERM exchange; returns what the TVC gets as text
std::string term(Hayes& hayes, std::uint64_t now, const std::string& in, std::uint8_t* status = nullptr) {
    std::vector<std::uint8_t> out;
    const std::uint8_t st = hayes.exchange(reinterpret_cast<const std::uint8_t*>(in.data()), in.size(), now, out, kMaxData);
    if (status) {
        *status = st;
    }
    return std::string(out.begin(), out.end());
}

constexpr std::uint64_t kSecond = 1000000;

void test_host_port() {
    std::string_view host;
    std::uint16_t port = 0;
    CHECK(parse_host_port("bbs.example.org:6502", host, port) && host == "bbs.example.org" && port == 6502);
    CHECK(!parse_host_port("nocolon", host, port));
    CHECK(!parse_host_port(":23", host, port));
    CHECK(!parse_host_port("h:0", host, port));
    CHECK(!parse_host_port("h:65536", host, port));
    CHECK(!parse_host_port("h:2x", host, port));
}

void test_hayes_commands() {
    FakeNet net;
    Hayes hayes(&net);
    std::uint8_t st = 0xFF;
    CHECK(term(hayes, 0, "AT\r", &st) == "AT\r\r\nOK\r\n" && st == status::kOk);
    CHECK(term(hayes, 0, "at e0\r") == "at e0\r\r\nOK\r\n");
    CHECK(term(hayes, 0, "AT\r") == "\r\nOK\r\n");
    CHECK(term(hayes, 0, "ATV0\r") == "0\r");
    CHECK(term(hayes, 0, "ATX\r") == "4\r");
    CHECK(term(hayes, 0, "ATV1Q1\r").empty());
    CHECK(term(hayes, 0, "ATQ0\r") == "\r\nOK\r\n");
    CHECK(term(hayes, 0, "hello\r").empty());
    CHECK(term(hayes, 0, "ATI\r") == "\r\nTVCNET PROTOCOL 2\r\n\r\nWIFI CONNECTED 192.168.0.9\r\n\r\nOK\r\n");
    CHECK(term(hayes, 0, "ATI2\r") == "\r\n192.168.0.9\r\n\r\nOK\r\n");
    CHECK(term(hayes, 0, "ATE1\r") == "\r\nOK\r\n");
    // Backspace takes a character back, A/ repeats the last line
    CHECK(term(hayes, 0, "ATX\bI4\r") == "ATX\bI4\r\r\n2\r\n\r\nOK\r\n");
    CHECK(term(hayes, 0, "A/") == "A/\r\r\n2\r\n\r\nOK\r\n");
    CHECK(term(hayes, 0, "ATW\"home,secret\"\r") == "ATW\"home,secret\"\r\r\nOK\r\n");
    CHECK(term(hayes, 0, "ATWnocomma\r") == "ATWnocomma\r\r\nERROR\r\n");
    CHECK(term(hayes, 0, "ATDnoport\r") == "ATDnoport\r\r\nERROR\r\n");
    CHECK(term(hayes, 0, "ATZ\r") == "ATZ\r\r\nOK\r\n");
    // A line arriving in pieces
    CHECK(term(hayes, 0, "A") == "A" && term(hayes, 0, "T\r") == "T\r\r\nOK\r\n");
}

void test_hayes_online() {
    FakeNet net;
    Hayes hayes(&net);
    std::uint8_t st = 0;
    term(hayes, 0, "ATE0\r");
    std::uint64_t t = kSecond;
    CHECK(term(hayes, t, "ATDT\"bbs.example.org:6502\"\r", &st).empty() && st == status::kBusy);
    CHECK(net.kind == FakeNet::Kind::Tcp && net.host == "bbs.example.org" && net.port == 6502);
    CHECK(term(hayes, t += 100000, "", &st).empty() && st == status::kBusy);
    net.done = true;
    CHECK(term(hayes, t += 100000, "", &st) == "\r\nCONNECT\r\n" && st == status::kData);
    CHECK(term(hayes, t += 100000, "hello", &st).empty() && st == status::kData && net.sent == "hello");
    net.body = "world";
    CHECK(term(hayes, t += 100000, "") == "world");
    // +++ right after data is data
    CHECK(term(hayes, t += 100000, "a+++").empty() && net.sent == "helloa+++");
    // +++ between two silent seconds is the escape; the connection stays open
    t += 2 * kSecond;
    CHECK(term(hayes, t, "+").empty() && term(hayes, t += 100000, "++").empty() && net.sent == "helloa+++");
    CHECK(term(hayes, t += 500000, "", &st).empty() && st == status::kData);
    CHECK(term(hayes, t += 600000, "", &st) == "\r\nOK\r\n" && st == status::kOk);
    CHECK(net.kind == FakeNet::Kind::Tcp);
    CHECK(term(hayes, t, "ATO\r", &st) == "\r\nCONNECT\r\n" && st == status::kData);
    // Two pluses and then silence are sent as data
    t += 2 * kSecond;
    term(hayes, t, "++");
    CHECK(term(hayes, t += 1100000, "x").empty() && net.sent == "helloa+++++x");
    // The peer closes: its last bytes, then NO CARRIER
    net.body += "bye";
    net.peer_closed = true;
    CHECK(term(hayes, t += 100000, "", &st) == "bye\r\nNO CARRIER\r\n" && st == status::kOk);
    CHECK(term(hayes, t, "ATO\r") == "\r\nNO CARRIER\r\n");

    // Escape and hang up
    term(hayes, t += kSecond, "ATD\"h:23\"\r");
    net.done = true;
    CHECK(term(hayes, t += 100000, "") == "\r\nCONNECT\r\n");
    t += 2 * kSecond;
    term(hayes, t, "+++");
    CHECK(term(hayes, t += 1100000, "ATH\r") == "\r\nOK\r\n\r\nOK\r\n" && net.kind == FakeNet::Kind::None);

    // A refused connection, a dial that times out, a key that aborts the dial
    term(hayes, t, "ATD\"h:23\"\r");
    net.error = status::kErrConnect;
    CHECK(term(hayes, t += 100000, "", &st) == "\r\nNO CARRIER\r\n" && st == status::kOk);
    term(hayes, t, "ATD\"h:23\"\r");
    CHECK(term(hayes, t += Hayes::kDialUs, "") == "\r\nNO CARRIER\r\n" && net.kind == FakeNet::Kind::None);
    term(hayes, t, "ATD\"h:23\"\r");
    CHECK(term(hayes, t += 100000, "x") == "\r\nNO CARRIER\r\n" && net.kind == FakeNet::Kind::None);
}

void test_hayes_fetch() {
    FakeNet net;
    Hayes hayes(&net);
    std::uint8_t st = 0;
    term(hayes, 0, "ATE0\r");
    CHECK(term(hayes, 0, "AT&G\"https://example.org/a.txt\"\r", &st).empty() && st == status::kData);
    CHECK(net.url == "https://example.org/a.txt");
    CHECK(term(hayes, 1, "", &st).empty() && st == status::kData);
    net.done = true;
    net.body.assign(300, 'z');
    CHECK(term(hayes, 2, "").size() == 255);
    CHECK(term(hayes, 3, "", &st) == std::string(45, 'z') + "\r\nOK\r\n" && st == status::kOk);
    term(hayes, 4, "AT&Ghttp://example.org/missing\r");
    net.error = status::kErrHttp;
    CHECK(term(hayes, 5, "") == "\r\nERROR\r\n" && net.url == "http://example.org/missing");
    CHECK(term(hayes, 6, "AT&G\r") == "\r\nERROR\r\n");
}

void test_modem_tcp() {
    FakeOut out;
    FakeNet net;
    Modem modem(out, &net);
    std::uint64_t now = 0;
    auto reply = ask(modem, out, now, request(cmd::kOpen, "nocolon"));
    CHECK(reply[1] == status::kErrParam);
    reply = ask(modem, out, now, request(kToggleBit | cmd::kOpen, "127.0.0.1:7000"));
    CHECK(reply[1] == (kToggleBit | status::kOk) && net.host == "127.0.0.1" && net.port == 7000);
    reply = ask(modem, out, now, request(cmd::kSend, "abc"));
    CHECK(reply[1] == status::kOk && net.sent == "abc");
    // A repeated SEND is not sent twice
    reply = ask(modem, out, now, request(cmd::kSend, "abc"));
    CHECK(reply[1] == status::kOk && net.sent == "abc");
    reply = ask(modem, out, now, request(kToggleBit | cmd::kStatus));
    CHECK(reply[1] == (kToggleBit | status::kOk) && reply[7] == status::kBusy);
    net.done = true;
    net.body = "xyz";
    reply = ask(modem, out, now, request(cmd::kNext));
    CHECK(reply[0] == 3 && reply[1] == status::kData && reply[2] == 'x');
    reply = ask(modem, out, now, request(kToggleBit | cmd::kNext));
    CHECK(reply[0] == 0 && reply[1] == (kToggleBit | status::kBusy));
    reply = ask(modem, out, now, request(cmd::kTerm, "AT\r"));
    CHECK(reply[1] == status::kOk && std::string(reply.begin() + 2, reply.end() - 1) == "AT\r\r\nOK\r\n");
    reply = ask(modem, out, now, request(kToggleBit | cmd::kClose));
    CHECK(net.kind == FakeNet::Kind::None);
    reply = ask(modem, out, now, request(cmd::kSend, "late"));
    CHECK(reply[1] == status::kNone);
}

}  // namespace

int main() {
    test_line_code();
    test_frames();
    test_modem();
    test_host_port();
    test_hayes_commands();
    test_hayes_online();
    test_hayes_fetch();
    test_modem_tcp();
    if (failures) {
        std::fprintf(stderr, "%d check(s) failed\n", failures);
        return EXIT_FAILURE;
    }
    std::puts("all checks passed");
    return EXIT_SUCCESS;
}
