// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#include "Dsp.h"

#include <math.h>

#include "audio/AudioConfig.h"

namespace voicerec::dsp {

namespace {
constexpr float kFullScale = 32767.0f;
constexpr float kKnee = 24576.0f;  // -2.5 dBFS: above it the limiter compresses 4:1
constexpr float kCeiling = 32000.0f;
}  // namespace

float dbToGain(float db) {
    return powf(10.0f, db / 20.0f);
}

bool applyGain(int16_t* samples, size_t count, float gain) {
    bool limited = false;
    for (size_t i = 0; i < count; ++i) {
        float value = samples[i] * gain;
        const float magnitude = fabsf(value);
        if (magnitude > kKnee) {
            limited = true;
            float compressed = kKnee + (magnitude - kKnee) * 0.25f;
            if (compressed > kCeiling) {
                compressed = kCeiling;
            }
            value = value < 0 ? -compressed : compressed;
        }
        samples[i] = static_cast<int16_t>(value);
    }
    return limited;
}

FrameLevel measure(const int16_t* samples, size_t count) {
    FrameLevel level{kSilenceDbfs, kSilenceDbfs, false};
    if (count == 0) {
        return level;
    }
    float sumSquares = 0.0f;
    int peak = 0;
    for (size_t i = 0; i < count; ++i) {
        const float value = samples[i];
        sumSquares += value * value;
        const int magnitude = samples[i] < 0 ? -samples[i] : samples[i];
        if (magnitude > peak) {
            peak = magnitude;
        }
    }
    const float rms = sqrtf(sumSquares / count);
    if (rms >= 1.0f) {
        level.rmsDbfs = fmaxf(kSilenceDbfs, 20.0f * log10f(rms / kFullScale));
    }
    if (peak > 0) {
        level.peakDbfs = fmaxf(kSilenceDbfs, 20.0f * log10f(peak / kFullScale));
    }
    level.clipped = peak >= static_cast<int>(kKnee);
    return level;
}

}  // namespace voicerec::dsp
