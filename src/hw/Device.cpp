// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#include "Device.h"

#include <M5Cardputer.h>

namespace voicerec {

namespace {
// M5GFX maps 255 to a 100 % PWM duty (offset 16, 9-bit LEDC: duty 512/512), i.e. GPIO38
// stays high and the LED supply is never interrupted.
constexpr uint8_t kFullBacklight = 255;

// Battery gauge smoothing. The voltage-based level sags under load (SD writes, speaker, backlight)
// and recovers at rest, so the raw value jumps by a few percent. Like phones and laptops do,
// the level is sampled slowly, filtered with an exponential moving average and only allowed
// to move in the expected direction (down while discharging, up while charging); a move the
// other way needs a clear margin (e.g. after a long rest or a brief unplug).
constexpr uint32_t kBatterySampleMs = 2000;
constexpr float kBatteryAlpha = 0.15f;  // ~13 s time constant at one sample per 2 s
constexpr int kBatteryReverseMargin = 3;
}  // namespace

void Device::begin() {
    switch (M5.getBoard()) {
        case m5::board_t::board_M5Cardputer: model_ = DeviceModel::Cardputer; break;
        case m5::board_t::board_M5CardputerADV: model_ = DeviceModel::CardputerAdv; break;
        default: model_ = DeviceModel::Unknown; break;
    }
}

const char* Device::modelName() const {
    switch (model_) {
        case DeviceModel::Cardputer: return "Cardputer";
        case DeviceModel::CardputerAdv: return "Cardputer ADV";
        default: return "Unknown";
    }
}

void Device::update() {
    const uint32_t now = millis();
    if (batterySampledAt_ != 0 && now - batterySampledAt_ < kBatterySampleMs) {
        return;
    }
    batterySampledAt_ = now == 0 ? 1 : now;

    const int raw = M5.Power.getBatteryLevel();
    const bool charging = M5.Power.isCharging() == m5::Power_Class::is_charging_t::is_charging;
    if (raw < 0) {
        batteryShown_ = -1;
        return;
    }
    if (batteryFiltered_ < 0 || charging != charging_) {
        // First reading or charger (un)plugged: start over from the current value.
        batteryFiltered_ = raw;
        batteryShown_ = raw;
        charging_ = charging;
        return;
    }

    batteryFiltered_ += kBatteryAlpha * (raw - batteryFiltered_);
    const int target = static_cast<int>(lroundf(batteryFiltered_));
    const bool expected = charging_ ? target > batteryShown_ : target < batteryShown_;
    const bool clearReverse = abs(target - batteryShown_) >= kBatteryReverseMargin;
    if (expected || clearReverse) {
        batteryShown_ = constrain(target, 0, 100);
    }
}

void Device::setBrightness(uint8_t level) {
    brightness_ = level;
    applyBrightness();
}

void Device::setLedPowerHold(bool hold) {
    hold = hold && model_ == DeviceModel::CardputerAdv;
    if (hold == ledPowerHold_) {
        return;
    }
    ledPowerHold_ = hold;
    applyBrightness();
}

void Device::applyBrightness() {
    // A switched-off screen stays off (the LED is unpowered then, which is fine).
    const uint8_t level = brightness_ == 0 ? 0 : (ledPowerHold_ ? kFullBacklight : brightness_);
    M5.Display.setBrightness(level);
}

}  // namespace voicerec
