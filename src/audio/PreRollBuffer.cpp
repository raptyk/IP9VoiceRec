// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#include "PreRollBuffer.h"

#include <string.h>

namespace voicerec {

void PreRollBuffer::push(const int16_t* frame) {
    memcpy(frames_[head_], frame, kBytesPerFrame);
    head_ = (head_ + 1) % kPreRollFrames;
    if (count_ < kPreRollFrames) {
        ++count_;
    }
}

const int16_t* PreRollBuffer::frame(size_t i) const {
    // While the ring is not full the oldest frame is at slot 0, afterwards at head_.
    const size_t oldest = count_ < kPreRollFrames ? 0 : head_;
    return frames_[(oldest + i) % kPreRollFrames];
}

}  // namespace voicerec
