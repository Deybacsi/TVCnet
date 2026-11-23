// SPDX-License-Identifier: GPL-3.0-or-later
#include "esp_network.hpp"

#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <Preferences.h>
#include <WiFi.h>

#include <algorithm>
#include <memory>

#include "tvcnet/codes.hpp"

namespace st = tvcnet::status;

namespace {

class Lock {
public:
    explicit Lock(SemaphoreHandle_t mutex) : mutex_(mutex) { xSemaphoreTake(mutex_, portMAX_DELAY); }
    ~Lock() { xSemaphoreGive(mutex_); }

private:
    SemaphoreHandle_t mutex_;
};

constexpr const char* kPrefs = "tvcnet";

}  // namespace

void EspNetwork::begin() {
    mutex_ = xSemaphoreCreateMutex();
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    Preferences prefs;
    if (prefs.begin(kPrefs, true)) {
        const String ssid = prefs.getString("ssid", "");
        const String pass = prefs.getString("pass", "");
        prefs.end();
        if (!ssid.isEmpty()) {
            WiFi.begin(ssid.c_str(), pass.c_str());
        }
    }
    xTaskCreatePinnedToCore(task_entry, "tvcnet-net", 8192, this, 1, &task_, 0);
}

bool EspNetwork::wifi_connected() const { return WiFi.status() == WL_CONNECTED; }

std::array<std::uint8_t, 4> EspNetwork::ip() const {
    if (!wifi_connected()) {
        return {};
    }
    const IPAddress ip = WiFi.localIP();
    return {ip[0], ip[1], ip[2], ip[3]};
}

bool EspNetwork::wifi_join(std::string_view ssid, std::string_view password) {
    if (ssid.empty() || ssid.size() > 32 || password.size() > 63) {
        return false;
    }
    const std::string s(ssid), p(password);
    Preferences prefs;
    if (prefs.begin(kPrefs, false)) {
        prefs.putString("ssid", s.c_str());
        prefs.putString("pass", p.c_str());
        prefs.end();
    }
    WiFi.disconnect();
    WiFi.begin(s.c_str(), p.c_str());
    return true;
}

std::uint8_t EspNetwork::http_get(std::string_view url) {
    if (url.rfind("http://", 0) != 0 && url.rfind("https://", 0) != 0) {
        return st::kErrParam;
    }
    if (!wifi_connected()) {
        return st::kErrNoWifi;
    }
    {
        Lock lock(mutex_);
        start_job(false);
        url_.assign(url);
    }
    xTaskNotifyGive(task_);
    return st::kOk;
}

std::uint8_t EspNetwork::tcp_open(std::string_view host, std::uint16_t port) {
    if (host.empty() || host.size() > 253) {
        return st::kErrParam;
    }
    if (!wifi_connected()) {
        return st::kErrNoWifi;
    }
    {
        Lock lock(mutex_);
        start_job(true);
        host_.assign(host);
        port_ = port;
    }
    xTaskNotifyGive(task_);
    return st::kOk;
}

std::uint8_t EspNetwork::tcp_send(const std::uint8_t* data, std::size_t size) {
    Lock lock(mutex_);
    if (!job_ || !tcp_) {
        return st::kNone;
    }
    if (error_) {
        return error_;
    }
    if (finished_) {
        return st::kErrLost;
    }
    if (send_.size() + size > kSendQueue) {
        return st::kBusy;
    }
    send_.insert(send_.end(), data, data + size);
    return st::kOk;
}

void EspNetwork::start_job(bool tcp) {
    ++job_id_;
    job_ = true;
    tcp_ = tcp;
    connected_ = false;
    started_ = false;
    finished_ = false;
    error_ = 0;
    http_status_ = 0;
    length_ = -1;
    buffer_.clear();
    send_.clear();
}

void EspNetwork::fail(std::uint32_t id, std::uint8_t code, std::uint16_t http_status) {
    Lock lock(mutex_);
    if (current(id)) {
        error_ = code;
        http_status_ = http_status;
    }
}

std::uint8_t EspNetwork::next(std::vector<std::uint8_t>& out, std::size_t max, std::uint16_t& detail) {
    out.clear();
    Lock lock(mutex_);
    if (!job_) {
        return st::kNone;
    }
    if (buffer_.empty()) {
        if (error_) {
            job_ = false;
            detail = http_status_;
            return error_;
        }
        if (finished_) {
            job_ = false;
            return st::kEnd;
        }
        return st::kBusy;
    }
    const std::size_t n = std::min(max, buffer_.size());
    out.assign(buffer_.begin(), buffer_.begin() + static_cast<long>(n));
    buffer_.erase(buffer_.begin(), buffer_.begin() + static_cast<long>(n));
    if (buffer_.empty() && finished_ && !error_) {
        job_ = false;
        return st::kEnd;
    }
    return st::kData;
}

std::uint8_t EspNetwork::job_state() const {
    Lock lock(mutex_);
    if (!job_) {
        return st::kNone;
    }
    if (!buffer_.empty()) {
        return st::kData;
    }
    if (error_) {
        return error_;
    }
    if (finished_) {
        return st::kEnd;
    }
    return tcp_ && connected_ ? st::kOk : st::kBusy;
}

std::int64_t EspNetwork::content_length() const {
    Lock lock(mutex_);
    return length_;
}

void EspNetwork::close() {
    Lock lock(mutex_);
    ++job_id_;
    job_ = false;
    buffer_.clear();
    send_.clear();
}

void EspNetwork::task_entry(void* self) { static_cast<EspNetwork*>(self)->task(); }

void EspNetwork::task() {
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        std::uint32_t id;
        bool tcp;
        std::string target;
        std::uint16_t port;
        {
            Lock lock(mutex_);
            if (!job_ || started_) {
                continue;
            }
            started_ = true;
            id = job_id_;
            tcp = tcp_;
            target = tcp ? host_ : url_;
            port = port_;
        }
        if (tcp) {
            run_tcp(id, target, port);
        } else {
            run_job(id, target);
        }
    }
}

void EspNetwork::run_job(std::uint32_t id, const std::string& url) {
    const bool tls = url.rfind("https://", 0) == 0;
    std::unique_ptr<NetworkClient> client;
    if (tls) {
        auto secure = std::make_unique<NetworkClientSecure>();
        secure->setInsecure();  // ⚑ no certificate check yet
        client = std::move(secure);
    } else {
        client = std::make_unique<NetworkClient>();
    }
    HTTPClient http;
    http.useHTTP10(true);  // no chunked transfer: the stream is the body
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setTimeout(15000);
    if (!http.begin(*client, url.c_str())) {
        fail(id, st::kErrParam);
        return;
    }
    const int code = http.GET();
    if (code < 0) {
        fail(id, code == HTTPC_ERROR_READ_TIMEOUT ? st::kErrLost : st::kErrConnect);
        http.end();
        return;
    }
    if (code >= 400) {
        fail(id, st::kErrHttp, static_cast<std::uint16_t>(code));
        http.end();
        return;
    }
    {
        Lock lock(mutex_);
        if (!current(id)) {
            http.end();
            return;
        }
        length_ = http.getSize();
    }
    NetworkClient* stream = http.getStreamPtr();
    std::int64_t left = http.getSize();
    std::uint8_t chunk[512];
    std::uint32_t idle_ms = 0;
    while (left != 0) {
        std::size_t room;
        {
            Lock lock(mutex_);
            if (!current(id)) {
                http.end();
                return;
            }
            room = kBuffer - buffer_.size();
        }
        const int available = stream->available();
        if (available <= 0 || room == 0) {
            if (!stream->connected() && available <= 0) {
                break;
            }
            if (available <= 0 && (idle_ms += 10) > 15000) {
                fail(id, st::kErrLost);
                http.end();
                return;
            }
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        idle_ms = 0;
        std::size_t want = std::min<std::size_t>({sizeof chunk, room, static_cast<std::size_t>(available)});
        if (left > 0) {
            want = std::min<std::size_t>(want, static_cast<std::size_t>(left));
        }
        const int got = stream->read(chunk, want);
        if (got <= 0) {
            continue;
        }
        if (left > 0) {
            left -= got;
        }
        Lock lock(mutex_);
        if (current(id)) {
            buffer_.insert(buffer_.end(), chunk, chunk + got);
        }
    }
    http.end();
    if (left > 0) {
        fail(id, st::kErrLost);
        return;
    }
    Lock lock(mutex_);
    if (current(id)) {
        finished_ = true;
    }
}

void EspNetwork::run_tcp(std::uint32_t id, const std::string& host, std::uint16_t port) {
    IPAddress address;
    if (!WiFi.hostByName(host.c_str(), address)) {
        fail(id, st::kErrDns);
        return;
    }
    NetworkClient client;
    if (!client.connect(address, port, 10000)) {
        fail(id, st::kErrConnect);
        return;
    }
    client.setNoDelay(true);
    std::uint8_t chunk[512];
    for (bool first = true;; first = false) {
        std::size_t out = 0;
        std::size_t room;
        {
            Lock lock(mutex_);
            if (!current(id)) {
                client.stop();
                return;
            }
            if (first) {
                connected_ = true;
            }
            room = std::min(kBuffer - buffer_.size(), sizeof chunk);
            out = std::min(send_.size(), sizeof chunk);
            std::copy(send_.begin(), send_.begin() + static_cast<long>(out), chunk);
            send_.erase(send_.begin(), send_.begin() + static_cast<long>(out));
        }
        bool idle = true;
        if (out > 0) {
            if (client.write(chunk, out) != out) {
                fail(id, st::kErrLost);
                client.stop();
                return;
            }
            idle = false;
        }
        const int available = client.available();
        if (available > 0 && room > 0) {
            const int got = client.read(chunk, std::min<std::size_t>(room, static_cast<std::size_t>(available)));
            if (got > 0) {
                Lock lock(mutex_);
                if (current(id)) {
                    buffer_.insert(buffer_.end(), chunk, chunk + got);
                }
                idle = false;
            }
        } else if (available <= 0 && !client.connected()) {
            client.stop();
            Lock lock(mutex_);
            if (current(id)) {
                finished_ = true;
            }
            return;
        }
        if (idle) {
            vTaskDelay(pdMS_TO_TICKS(5));
        }
    }
}
