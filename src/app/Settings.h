// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#pragma once

#include <stdint.h>

#include "audio/AudioConfig.h"
#include "audio/Recorder.h"

namespace voicerec {

// User settings, stored in NVS (they survive without an SD card).
struct Settings {
    // Recording.
    bool voiceActivation = true;
    uint8_t sensitivity = 3;  // 1..5
    uint16_t holdMs = 1500;
    int8_t micGainDb = 12;    // software gain, 0..30 dB
    bool autoLevel = true;    // AutoGain: quiet speech raised to a clear level in the file
    uint8_t bitrateKbps = kDefaultBitrateKbps;

    // Calibration (Mic check). The noise floor was measured with `calibratedGainDb`.
    bool calibrated = false;
    float noiseFloorDbfs = -60.0f;
    int8_t calibratedGainDb = 12;

    // Playback and device.
    uint8_t volume = 6;  // 0..10
    uint8_t brightness = 160;
    uint16_t screenOffSec = 60;  // 0 = never
    bool statusLed = true;

    // Loads stored values; the first start picks the mic gain for the board.
    void load(bool cardputerAdv);
    void save() const;

    // Expected noise floor at the current gain (calibrated value, or a typical one).
    float expectedNoiseFloor() const;
    RecorderConfig recorderConfig() const;
    // 0..255 for the speaker, from the 0..10 volume steps (perceptually even).
    uint8_t speakerVolume() const;
};

}  // namespace voicerec
