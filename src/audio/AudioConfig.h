// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#pragma once

#include <stdint.h>
#include <stddef.h>

namespace voicerec {

// Audio format constants
// 576 samples per MPEG-2 Layer 3 frame at 16 kHz (36.0 ms per frame)
constexpr uint32_t kSampleRate = 16000;
constexpr uint8_t kChannels = 1;                     // Mono
constexpr uint8_t kBitsPerSample = 16;
constexpr size_t kSamplesPerFrame = 576;             // Golden frame size for shine MPEG-2 L3
constexpr size_t kBytesPerFrame = kSamplesPerFrame * sizeof(int16_t); // 1152 bytes
constexpr float kFrameDurationMs = 36.0f;            // 576 / 16000 * 1000 ms

// MP3 Encoding defaults
constexpr int kDefaultBitrateKbps = 48;              // 48 kbps mono MP3 (6 kB/s)
constexpr size_t kMaxMp3FrameBytes = 288;            // Max bytes per MP3 frame at 16kHz

// Pre-Roll ring buffer constants
constexpr size_t kPreRollFrameCount = 10;            // 10 * 36ms = 360 ms audio history
constexpr size_t kPreRollCapacityBytes = kPreRollFrameCount * kBytesPerFrame; // 11520 bytes

// FreeRTOS StreamBuffer (Inter-core audio pipeline)
// Capacity: 32 frames * 1152 bytes = 36864 bytes (~1.15 seconds of raw audio)
constexpr size_t kAudioStreamBufferBytes = 32 * kBytesPerFrame;
constexpr size_t kAudioStreamTriggerBytes = kBytesPerFrame; // Wake writer only on full frame

// SD Card storage buffer constants
constexpr size_t kSdSectorBufferSize = 8192;         // 8 KB RAM staging buffer
constexpr size_t kSdWriteBlockThreshold = 4096;      // Write to SD in 4 KB multi-sector chunks

// VAD & Silence Threshold defaults
constexpr float kDefaultAttackThresholdDbfs = -36.0f; // Speech trigger threshold
constexpr float kDefaultHysteresisMarginDb = 5.0f;    // Release threshold = Attack - Margin
constexpr uint32_t kDefaultHangoverMs = 1500;         // 1.5 seconds hold after voice drops
constexpr uint32_t kMinHangoverMs = 500;
constexpr uint32_t kMaxHangoverMs = 4000;

// DC Blocker filter parameter
// fc ≈ 80 Hz at fs = 16000 Hz: R ≈ 1 - (2*pi*80/16000) ≈ 0.9686
constexpr float kDcBlockerR = 0.9686f;

// Calibration parameters
constexpr size_t kCalibrationFrames = 70;             // 70 frames * 36ms = 2520 ms (~2.5 seconds)

}  // namespace voicerec
