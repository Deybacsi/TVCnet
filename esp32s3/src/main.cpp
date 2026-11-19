// SPDX-License-Identifier: GPL-3.0-or-later
// TVCnet, the TVC tape-port WiFi module (PROTOKOLL.md): the motor line is a UART into GPIO1, the replies go out on GPIO2.
#include <Arduino.h>
#include <esp_timer.h>

#include "esp_network.hpp"
#include "rmt_line.hpp"
#include "tvcnet/modem.hpp"

namespace {

constexpr int kMotorPin = 1;    // DIN 5, 4.7k pull-up to 3V3
constexpr int kTapeInPin = 2;   // DIN 3 through 1k
constexpr unsigned long kBaud = 2400;

class UsbLog final : public tvcnet::Log {
public:
    void line(std::string_view text) override {
        Serial.write(reinterpret_cast<const std::uint8_t*>(text.data()), text.size());
        Serial.write('\n');
    }
};

RmtLine line(kTapeInPin);
EspNetwork network;
UsbLog usb_log;
tvcnet::Modem modem(line, &network, {}, &usb_log);

}  // namespace

void setup() {
    Serial.begin(115200);
    Serial1.begin(kBaud, SERIAL_8N1, kMotorPin, -1);
    if (!line.begin()) {
        Serial.println("RMT init failed");
    }
    network.begin();
    Serial.println("TVCnet ready");
}

void loop() {
    while (Serial1.available() > 0) {
        modem.on_byte(static_cast<std::uint8_t>(Serial1.read()), esp_timer_get_time());
    }
    modem.poll(esp_timer_get_time());
    delay(1);
}
