// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#pragma once

#include <stdint.h>

namespace voicerec {

enum class DeviceModel : uint8_t { Cardputer, CardputerAdv, Unknown };

// Board-level helpers: model detection, battery, backlight.
class Device {
public:
    void begin();
    // Samples the battery (every 2 s); call from the main loop.
    void update();

    DeviceModel model() const { return model_; }
    const char* modelName() const;

    // Smoothed battery level (0..100, -1 = unknown) and charger state, see update().
    int batteryPercent() const { return batteryShown_; }
    bool isCharging() const { return charging_; }

    void setBrightness(uint8_t level);
    uint8_t brightness() const { return brightness_; }

    // Cardputer ADV: the RGB LED's VDD is the backlight rail BL_3V3, switched by GPIO38,
    // which M5GFX drives as PWM to dim the screen. While the status light is enabled the
    // backlight therefore runs at 255 (100 % duty = GPIO38 constantly high) unless the
    // screen is off. No effect on the original Cardputer.
    void setLedPowerHold(bool hold);
    // True when brightness settings cannot take effect (ADV with the status light on).
    bool brightnessLocked() const { return ledPowerHold_; }

private:
    void applyBrightness();

    DeviceModel model_ = DeviceModel::Unknown;
    uint8_t brightness_ = 0;
    bool ledPowerHold_ = false;

    uint32_t batterySampledAt_ = 0;
    float batteryFiltered_ = -1;
    int batteryShown_ = -1;
    bool charging_ = false;
};

}  // namespace voicerec
