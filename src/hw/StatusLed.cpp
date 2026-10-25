// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#include "StatusLed.h"
#include <Arduino.h>
#include <M5Unified.h>
#include <cmath>

namespace voicerec {

namespace {
constexpr uint32_t kBreathPeriodMs = 1800;
}  // namespace

void StatusLed::begin() {
    pin_ = M5.getPin(m5::pin_name_t::rgb_led);
    if (pin_ < 0) {
        pin_ = 21;
    }
    show(0, 0, 0);
}

void StatusLed::setEnabled(bool enabled) {
    enabled_ = enabled;
    if (!enabled_) {
        show(0, 0, 0);
    }
}

void StatusLed::setState(LedState state) {
    state_ = state;
}

void StatusLed::loop() {
    if (!enabled_ || pin_ < 0) {
        return;
    }

    const uint32_t now = millis();
    if (now - lastUpdateAt_ < 30) {
        return;
    }
    lastUpdateAt_ = now;

    switch (state_) {
        case LedState::Off:
        case LedState::Idle:
            show(0, 0, 0);
            break;

        case LedState::Recording: {
            // Pulsing bright red
            const float phase = static_cast<float>(now % kBreathPeriodMs) / kBreathPeriodMs;
            const float wave = 0.5f - 0.5f * cosf(phase * 2.0f * PI);
            const uint8_t red = static_cast<uint8_t>(20 + wave * 180);
            show(red, 0, 0);
            break;
        }

        case LedState::WaitingSilence:
            // Gentle cyan/blue
            show(0, 20, 50);
            break;

        case LedState::Hangover:
            // Yellow / amber
            show(40, 30, 0);
            break;

        case LedState::Playback:
            // Bright green
            show(0, 50, 0);
            break;
    }
}

void StatusLed::show(uint8_t r, uint8_t g, uint8_t b) {
    if (pin_ >= 0) {
        neopixelWrite(static_cast<uint8_t>(pin_), r, g, b);
    }
}

}  // namespace voicerec
