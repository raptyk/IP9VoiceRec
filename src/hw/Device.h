// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#pragma once

#include <stdint.h>

namespace voicerec {

enum class DeviceModel : uint8_t {
    Unknown,
    Cardputer,
    CardputerAdv
};

class Device {
public:
    static Device& instance() {
        static Device inst;
        return inst;
    }

    void begin();
    void update();

    DeviceModel model() const { return model_; }
    const char* modelName() const;

    int batteryLevel() const { return batteryShown_; }
    bool isCharging() const { return charging_; }

    void setBrightness(uint8_t level);
    uint8_t brightness() const { return brightness_; }

private:
    Device() = default;

    DeviceModel model_ = DeviceModel::Unknown;
    uint32_t batterySampledAt_ = 0;
    float batteryFiltered_ = -1.0f;
    int batteryShown_ = -1;
    bool charging_ = false;
    uint8_t brightness_ = 200;
};

}  // namespace voicerec
