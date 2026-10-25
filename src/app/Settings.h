// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#pragma once

#include <stdint.h>
#include <string>

namespace voicerec {

struct Settings {
    bool vasEnabled = true;
    float vasAttackThresholdDbfs = -36.0f;
    float vasHysteresisMarginDb = 5.0f;
    uint32_t vasHangoverMs = 1500;
    int bitrateKbps = 48;
    uint8_t screenBrightness = 200;
    bool statusLedEnabled = true;
    uint8_t speakerVolume = 180;

    static Settings& instance() {
        static Settings inst;
        return inst;
    }

    bool load();
    bool save();

    static constexpr const char* kConfigPath = "/config/voicerec.json";
};

}  // namespace voicerec
