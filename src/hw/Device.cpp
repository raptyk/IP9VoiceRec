// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#include "Device.h"
#include <M5Cardputer.h>
#include <cmath>

namespace voicerec {

namespace {
constexpr uint32_t kBatterySampleMs = 2000;
constexpr float kBatteryAlpha = 0.15f;
constexpr int kBatteryReverseMargin = 3;
}  // namespace

void Device::begin() {
    switch (M5.getBoard()) {
        case m5::board_t::board_M5Cardputer:
            model_ = DeviceModel::Cardputer;
            break;
        case m5::board_t::board_M5CardputerADV:
            model_ = DeviceModel::CardputerAdv;
            break;
        default:
            model_ = DeviceModel::Unknown;
            break;
    }
    setBrightness(brightness_);
}

const char* Device::modelName() const {
    switch (model_) {
        case DeviceModel::Cardputer: return "Cardputer";
        case DeviceModel::CardputerAdv: return "Cardputer ADV";
        default: return "ESP32-S3";
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
        batteryFiltered_ = raw;
        batteryShown_ = raw;
        charging_ = charging;
        return;
    }

    batteryFiltered_ += kBatteryAlpha * (raw - batteryFiltered_);
    const int target = static_cast<int>(std::lround(batteryFiltered_));
    const bool expected = charging_ ? target > batteryShown_ : target < batteryShown_;
    const bool clearReverse = std::abs(target - batteryShown_) >= kBatteryReverseMargin;
    if (expected || clearReverse) {
        batteryShown_ = std::max(0, std::min(100, target));
    }
}

void Device::setBrightness(uint8_t level) {
    brightness_ = level;
    M5.Display.setBrightness(brightness_);
}

}  // namespace voicerec
