// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace voicerec {

// Audio format: 16 kHz mono, one MPEG-2 Layer III frame = 576 samples = 36 ms.
// One PCM frame from the microphone is exactly one MP3 frame from the encoder.
constexpr uint32_t kSampleRate = 16000;
constexpr size_t kSamplesPerFrame = 576;
constexpr size_t kBytesPerFrame = kSamplesPerFrame * sizeof(int16_t);
constexpr uint32_t kFrameMs = 36;

// MP3 bitrates offered in the settings (kbit/s); 48 is a good speech default.
constexpr uint8_t kBitrates[] = {32, 48, 64};
constexpr uint8_t kDefaultBitrateKbps = 48;

// Voice activation keeps the last 360 ms of "silence" so the first syllable is never cut.
constexpr size_t kPreRollFrames = 10;

// Frames waiting for the MP3 writer (core 0). 32 frames = 1.15 s, enough to ride out
// the occasional slow SD card write without losing audio.
constexpr size_t kWriterQueueFrames = 32;
constexpr size_t kSdWriteBlock = 4096;       // written to the card in whole 4 KB blocks
constexpr uint32_t kSyncIntervalMs = 10000;  // file metadata synced at least every 10 s

// Frames dropped after the microphone starts (power-up pop, codec warm-up).
constexpr uint32_t kMicSettleFrames = 8;           // ~290 ms
constexpr uint32_t kMicMaxSilentStartFrames = 40;  // ES8311 delivers zeros for up to ~1 s

// DC blocker (first-order high-pass) pole: fc = 80 Hz at 16 kHz, R = 1 - 2*pi*80/16000.
constexpr float kDcBlockerR = 0.9686f;

// Software microphone gain on top of the board's base gain (dB, in 3 dB steps).
constexpr int kMicGainMinDb = 0;
constexpr int kMicGainMaxDb = 30;
constexpr int kMicGainStepDb = 3;

// Voice activation.
constexpr uint8_t kSensitivityMin = 1;
constexpr uint8_t kSensitivityMax = 5;
constexpr uint16_t kHangoverMinMs = 500;
constexpr uint16_t kHangoverMaxMs = 4000;
constexpr uint16_t kHangoverStepMs = 500;

// Lowest level reported by the meters.
constexpr float kSilenceDbfs = -90.0f;

}  // namespace voicerec
