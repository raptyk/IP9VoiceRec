// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace voicerec::dsp {

struct FrameLevel {
    float rmsDbfs;
    float peakDbfs;
    bool clipped;  // the limiter had to work on this frame
};

// Linear factor of a gain in dB.
float dbToGain(float db);

// Multiplies the samples by `gain` with a soft limiter above -2.5 dBFS, so a loud voice
// close to the microphone is compressed instead of clipped hard. Returns true if limited.
bool applyGain(int16_t* samples, size_t count, float gain);

// Like applyGain, with the gain moving linearly from `startGain` to `endGain` across the
// samples, so a gain change between frames never clicks.
bool applyGainRamp(int16_t* samples, size_t count, float startGain, float endGain);

// RMS and peak of a frame in dBFS (kSilenceDbfs for digital silence).
FrameLevel measure(const int16_t* samples, size_t count);

}  // namespace voicerec::dsp
