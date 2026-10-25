// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <functional>
#include "AudioConfig.h"

namespace voicerec {

class PreRollBuffer {
public:
    PreRollBuffer();

    void reset();

    /// Push a 576-sample frame into the ring buffer
    void pushFrame(const int16_t* samples);

    /// Flush all available frames (oldest to newest) to a consumer callback
    /// Returns the number of frames flushed
    size_t flush(const std::function<void(const int16_t* samples, size_t count)>& consumer);

    size_t availableFrames() const { return count_; }
    bool isFull() const { return count_ == kPreRollFrameCount; }

private:
    int16_t buffer_[kPreRollFrameCount][kSamplesPerFrame];
    size_t head_ = 0;   // Next write index
    size_t count_ = 0;  // Stored frames count (up to kPreRollFrameCount)
};

}  // namespace voicerec
