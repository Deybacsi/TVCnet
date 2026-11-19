// SPDX-License-Identifier: GPL-3.0-or-later
#include "esp_network.hpp"

#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <Preferences.h>
#include <WiFi.h>

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
        ++job_id_;
        job_ = true;
        started_ = false;
        finished_ = false;
        error_ = 0;
        http_status_ = 0;
        length_ = -1;
        url_.assign(url);
        buffer_.clear();
    }
    xTaskNotifyGive(task_);
    return st::kOk;
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
    return finished_ ? st::kEnd : st::kBusy;
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
}

void EspNetwork::task_entry(void* self) { static_cast<EspNetwork*>(self)->task(); }

void EspNetwork::task() {
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        std::uint32_t id;
        std::string url;
        {
            Lock lock(mutex_);
            if (!job_ || started_) {
                continue;
            }
            started_ = true;
            id = job_id_;
            url = url_;
        }
        run_job(id, url);
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
    auto fail = [&](std::uint8_t code, std::uint16_t status) {
        Lock lock(mutex_);
        if (current(id)) {
            error_ = code;
            http_status_ = status;
        }
    };
    if (!http.begin(*client, url.c_str())) {
        fail(st::kErrParam, 0);
        return;
    }
    const int code = http.GET();
    if (code < 0) {
        fail(code == HTTPC_ERROR_READ_TIMEOUT ? st::kErrLost : st::kErrConnect, 0);
        http.end();
        return;
    }
    if (code >= 400) {
        fail(st::kErrHttp, static_cast<std::uint16_t>(code));
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
                fail(st::kErrLost, 0);
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
        fail(st::kErrLost, 0);
        return;
    }
    Lock lock(mutex_);
    if (current(id)) {
        finished_ = true;
    }
}
