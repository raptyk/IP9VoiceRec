// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#include "Settings.h"

#include <Preferences.h>

#include <algorithm>

namespace voicerec {

namespace {
constexpr const char* kNamespace = "voicerec";
constexpr uint8_t kVolumeCurve[11] = {0, 8, 16, 28, 44, 64, 90, 120, 155, 200, 255};
// Starting gains, chosen so normal speech at arm's length lands around -20 dBFS.
constexpr int8_t kDefaultGainPdm = 12;  // Cardputer: PDM MEMS microphone
constexpr int8_t kDefaultGainAdv = 18;  // Cardputer ADV: analog MEMS via the ES8311 (+21 dB PGA, x1)
constexpr float kTypicalNoiseAtZeroGain = -66.0f;  // only a start value, the floor adapts

template <typename T>
T clampValue(T value, T low, T high) {
    return std::max(low, std::min(high, value));
}
}  // namespace

void Settings::load(bool cardputerAdv) {
    Preferences prefs;
    prefs.begin(kNamespace, true);
    const int8_t defaultGain = cardputerAdv ? kDefaultGainAdv : kDefaultGainPdm;
    voiceActivation = prefs.getBool("vas", voiceActivation);
    sensitivity = clampValue<uint8_t>(prefs.getUChar("sens", sensitivity), kSensitivityMin, kSensitivityMax);
    holdMs = clampValue<uint16_t>(prefs.getUShort("hold", holdMs), kHangoverMinMs, kHangoverMaxMs);
    micGainDb = clampValue<int8_t>(prefs.getChar("gain", defaultGain), kMicGainMinDb, kMicGainMaxDb);
    autoLevel = prefs.getBool("agc", autoLevel);
    bitrateKbps = prefs.getUChar("kbps", bitrateKbps);
    if (std::find(std::begin(kBitrates), std::end(kBitrates), bitrateKbps) == std::end(kBitrates)) {
        bitrateKbps = kDefaultBitrateKbps;
    }
    calibrated = prefs.getBool("cal", false);
    noiseFloorDbfs = prefs.getFloat("noise", noiseFloorDbfs);
    calibratedGainDb = prefs.getChar("calGain", micGainDb);
    volume = clampValue<uint8_t>(prefs.getUChar("vol", volume), 0, 10);
    brightness = prefs.getUChar("bright", brightness);
    screenOffSec = prefs.getUShort("offSec", screenOffSec);
    statusLed = prefs.getBool("led", statusLed);
    prefs.end();
}

void Settings::save() const {
    Preferences prefs;
    prefs.begin(kNamespace, false);
    prefs.putBool("vas", voiceActivation);
    prefs.putUChar("sens", sensitivity);
    prefs.putUShort("hold", holdMs);
    prefs.putChar("gain", micGainDb);
    prefs.putBool("agc", autoLevel);
    prefs.putUChar("kbps", bitrateKbps);
    prefs.putBool("cal", calibrated);
    prefs.putFloat("noise", noiseFloorDbfs);
    prefs.putChar("calGain", calibratedGainDb);
    prefs.putUChar("vol", volume);
    prefs.putUChar("bright", brightness);
    prefs.putUShort("offSec", screenOffSec);
    prefs.putBool("led", statusLed);
    prefs.end();
}

float Settings::expectedNoiseFloor() const {
    if (calibrated) {
        return noiseFloorDbfs + (micGainDb - calibratedGainDb);
    }
    return kTypicalNoiseAtZeroGain + micGainDb;
}

RecorderConfig Settings::recorderConfig() const {
    RecorderConfig config;
    config.gainDb = micGainDb;
    config.voiceActivation = voiceActivation;
    config.sensitivity = sensitivity;
    config.holdMs = holdMs;
    config.noiseFloorDbfs = expectedNoiseFloor();
    config.autoLevel = autoLevel;
    return config;
}

uint8_t Settings::speakerVolume() const {
    return kVolumeCurve[std::min<uint8_t>(volume, 10)];
}

}  // namespace voicerec
