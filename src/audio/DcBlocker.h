// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <algorithm>
#include "AudioConfig.h"

namespace voicerec {

/**
 * @brief High-pass DC Blocker filter (fc ≈ 80 Hz) using ESP32-S3 hardware FPU.
 * Eliminates microphone DC offset and low-frequency chassis rumble.
 * Difference equation: y[n] = x[n] - x[n-1] + R * y[n-1]
 */
class DcBlocker {
public:
    DcBlocker() = default;

    void reset() {
        prevX_ = 0.0f;
        prevY_ = 0.0f;
    }

    /**
     * @brief Process an in-place buffer of int16_t samples.
     * Takes < 35 microseconds on ESP32-S3 for 576 samples.
     */
    void process(int16_t* samples, size_t count) {
        float px = prevX_;
        float py = prevY_;
        const float R = kDcBlockerR;

        for (size_t i = 0; i < count; ++i) {
            const float x = static_cast<float>(samples[i]);
            float y = x - px + R * py;
            px = x;
            py = y;

            // Clamp to int16_t range to prevent overflow distortion
            if (y > 32767.0f) y = 32767.0f;
            else if (y < -32768.0f) y = -32768.0f;

            samples[i] = static_cast<int16_t>(y);
        }

        prevX_ = px;
        prevY_ = py;
    }

private:
    float prevX_ = 0.0f;
    float prevY_ = 0.0f;
};

}  // namespace voicerec
