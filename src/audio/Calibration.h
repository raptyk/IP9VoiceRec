// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace voicerec::calibration {

// Frame levels collected in each calibration phase.
constexpr size_t kQuietFrames = 83;   // 3 s of quiet
constexpr size_t kSpeechFrames = 125;  // 4.5 s of normal speech

enum class RoomKind : uint8_t { Quiet, Normal, Noisy };

struct NoiseResult {
    float floorDbfs;   // median frame level: the steady background
    float spreadDb;    // P90 - median: how much the background fluctuates
    RoomKind room;
};

struct SpeechResult {
    bool heard;            // enough frames clearly above the noise
    float voiceDbfs;       // typical loud speech frame (P80 of the voiced frames)
    float snrDb;           // voice above noise
    int recommendedGainDb; // gain that brings the voice to the target level
};

// `levels` is modified (sorted).
NoiseResult analyzeNoise(float* levels, size_t count);

// `levels` is modified (sorted). The levels were measured with `currentGainDb` applied.
SpeechResult analyzeSpeech(float* levels, size_t count, const NoiseResult& noise, int currentGainDb);

// Suggested sensitivity (1..5) for a measured room: a fluctuating background needs a larger margin.
uint8_t sensitivityFor(const NoiseResult& noise, const SpeechResult& speech);

}  // namespace voicerec::calibration
