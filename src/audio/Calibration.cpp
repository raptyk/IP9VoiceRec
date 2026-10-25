// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#include "Calibration.h"

#include <math.h>

#include <algorithm>

#include "audio/AudioConfig.h"
#include "audio/VoiceDetector.h"

namespace voicerec::calibration {

namespace {
constexpr float kVoicedAboveNoiseDb = 8.0f;  // a speech-phase frame this far above noise counts as voice
constexpr size_t kMinVoicedFrames = 14;      // ~0.5 s of actual voice
constexpr float kTargetVoiceDbfs = -20.0f;   // loud speech frames land here: peaks around -6 dBFS
constexpr float kQuietRoomBelow = -60.0f;
constexpr float kNormalRoomBelow = -45.0f;

float percentile(const float* sorted, size_t count, int percent) {
    if (count == 0) {
        return kSilenceDbfs;
    }
    const size_t index = std::min(count - 1, count * percent / 100);
    return sorted[index];
}
}  // namespace

NoiseResult analyzeNoise(float* levels, size_t count) {
    std::sort(levels, levels + count);
    NoiseResult result;
    result.floorDbfs = percentile(levels, count, 50);
    result.spreadDb = percentile(levels, count, 90) - result.floorDbfs;
    if (result.floorDbfs < kQuietRoomBelow) {
        result.room = RoomKind::Quiet;
    } else if (result.floorDbfs < kNormalRoomBelow) {
        result.room = RoomKind::Normal;
    } else {
        result.room = RoomKind::Noisy;
    }
    return result;
}

SpeechResult analyzeSpeech(float* levels, size_t count, const NoiseResult& noise, int currentGainDb) {
    std::sort(levels, levels + count);
    const float voicedFrom = noise.floorDbfs + kVoicedAboveNoiseDb;
    const float* voiced = std::lower_bound(levels, levels + count, voicedFrom);
    const size_t voicedCount = static_cast<size_t>(levels + count - voiced);

    SpeechResult result{false, kSilenceDbfs, 0.0f, currentGainDb};
    if (voicedCount < kMinVoicedFrames) {
        return result;
    }
    result.heard = true;
    result.voiceDbfs = percentile(voiced, voicedCount, 80);
    result.snrDb = result.voiceDbfs - noise.floorDbfs;

    // Round the correction to whole gain steps and stay within the supported range.
    const float correction = kTargetVoiceDbfs - result.voiceDbfs;
    int gain = currentGainDb + static_cast<int>(lroundf(correction / kMicGainStepDb)) * kMicGainStepDb;
    result.recommendedGainDb = std::max(kMicGainMinDb, std::min(kMicGainMaxDb, gain));
    return result;
}

uint8_t sensitivityFor(const NoiseResult& noise, const SpeechResult& speech) {
    // The most sensitive step whose margin still clears the background's own fluctuation.
    uint8_t sensitivity = kSensitivityMin;
    for (uint8_t s = kSensitivityMax; s >= kSensitivityMin; --s) {
        if (VoiceDetector::marginForSensitivity(s) >= noise.spreadDb + 3.0f) {
            sensitivity = s;
            break;
        }
    }
    // A quiet voice in a noisy room: rather catch the voice (with some noise) than miss it.
    while (speech.heard && sensitivity < kSensitivityMax &&
           VoiceDetector::marginForSensitivity(sensitivity) > speech.snrDb - 6.0f) {
        ++sensitivity;
    }
    return sensitivity;
}

}  // namespace voicerec::calibration
