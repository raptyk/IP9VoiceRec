// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#include "PreRollBuffer.h"
#include <cstring>

namespace voicerec {

PreRollBuffer::PreRollBuffer() {
    reset();
}

void PreRollBuffer::reset() {
    head_ = 0;
    count_ = 0;
    std::memset(buffer_, 0, sizeof(buffer_));
}

void PreRollBuffer::pushFrame(const int16_t* samples) {
    std::memcpy(buffer_[head_], samples, kBytesPerFrame);
    head_ = (head_ + 1) % kPreRollFrameCount;
    if (count_ < kPreRollFrameCount) {
        ++count_;
    }
}

size_t PreRollBuffer::flush(const std::function<void(const int16_t* samples, size_t count)>& consumer) {
    if (count_ == 0 || !consumer) {
        return 0;
    }

    // Determine starting index of oldest stored frame
    size_t startIdx;
    if (count_ < kPreRollFrameCount) {
        startIdx = 0;
    } else {
        startIdx = head_; // In a full ring buffer, head_ points to the oldest overwritten entry
    }

    const size_t flushedCount = count_;
    for (size_t i = 0; i < flushedCount; ++i) {
        const size_t idx = (startIdx + i) % kPreRollFrameCount;
        consumer(buffer_[idx], kSamplesPerFrame);
    }

    // Reset buffer state after flushing
    count_ = 0;
    head_ = 0;

    return flushedCount;
}

}  // namespace voicerec
