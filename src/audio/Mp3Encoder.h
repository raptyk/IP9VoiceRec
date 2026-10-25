// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#pragma once

#include <stdint.h>
#include <stddef.h>
#include "AudioConfig.h"

// Forward declaration of shine types
struct shine_global_flags;
typedef struct shine_global_flags* shine_t;

namespace voicerec {

class Mp3Encoder {
public:
    Mp3Encoder();
    ~Mp3Encoder();

    bool begin(uint32_t sampleRate = kSampleRate, int bitrateKbps = kDefaultBitrateKbps);
    void end();

    bool isOpen() const { return encoder_ != nullptr; }

    /// Encodes exactly one frame (576 samples). Returns pointer to output MP3 data and written bytes count.
    const uint8_t* encodeFrame(const int16_t* samples, int* writtenBytes);

    /// Flush remaining data (call ONLY upon final stop, not during VAS pause).
    const uint8_t* flush(int* writtenBytes);

    int samplesPerPass() const { return samplesPerPass_; }
    int bitrateKbps() const { return bitrateKbps_; }

private:
    shine_t encoder_ = nullptr;
    int samplesPerPass_ = kSamplesPerFrame;
    int bitrateKbps_ = kDefaultBitrateKbps;
    uint32_t sampleRate_ = kSampleRate;
};

}  // namespace voicerec
