// SPDX-License-Identifier: GPL-3.0-or-later
// WiFi and the one HTTP(S) job on the ESP32-S3. The job runs in its own FreeRTOS task, so the modem never blocks;
// it reads only as much as the buffer holds, the TVC's NEXT requests empty it.
#pragma once

#include <Arduino.h>

#include <deque>
#include <string>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "tvcnet/hal.hpp"

class EspNetwork final : public tvcnet::Network {
public:
    static constexpr std::size_t kBuffer = 32 * 1024;

    // Joins the stored network, if any, and starts the job task.
    void begin();

    bool wifi_connected() const override;
    std::array<std::uint8_t, 4> ip() const override;
    bool wifi_join(std::string_view ssid, std::string_view password) override;
    std::uint8_t http_get(std::string_view url) override;
    std::uint8_t next(std::vector<std::uint8_t>& out, std::size_t max, std::uint16_t& detail) override;
    std::uint8_t job_state() const override;
    std::int64_t content_length() const override;
    void close() override;

private:
    static void task_entry(void* self);
    void task();
    void run_job(std::uint32_t id, const std::string& url);
    bool current(std::uint32_t id) const { return id == job_id_; }

    mutable SemaphoreHandle_t mutex_ = nullptr;
    TaskHandle_t task_ = nullptr;
    // Guarded by mutex_
    std::uint32_t job_id_ = 0;
    bool job_ = false;
    bool started_ = false;   // the task took the job
    bool finished_ = false;
    std::uint8_t error_ = 0;
    std::uint16_t http_status_ = 0;
    std::int64_t length_ = -1;
    std::string url_;
    std::deque<std::uint8_t> buffer_;
};
