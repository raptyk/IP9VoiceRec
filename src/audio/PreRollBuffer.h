// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "audio/AudioConfig.h"

namespace voicerec {

// Ring of the last kPreRollFrames frames of "silence". When voice starts, the frames are
// written in front of it so the first syllable (which starts below the threshold) is kept.
class PreRollBuffer {
public:
    void reset() { count_ = 0; head_ = 0; }

    void push(const int16_t* frame);

    size_t count() const { return count_; }
    // i = 0 is the oldest stored frame.
    const int16_t* frame(size_t i) const;

private:
    int16_t frames_[kPreRollFrames][kSamplesPerFrame] = {};
    size_t head_ = 0;   // next write slot
    size_t count_ = 0;  // stored frames (up to kPreRollFrames)
};

}  // namespace voicerec
