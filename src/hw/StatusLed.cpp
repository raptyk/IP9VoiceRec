// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#include "StatusLed.h"

#include <M5Unified.h>
#include <math.h>

namespace voicerec {

namespace {
constexpr uint32_t kBreathPeriodMs = 2400;
constexpr uint8_t kSteadyLevel = 60;  // recording: clearly visible, not a torch
// A lit LED is re-sent regularly so it recovers by itself after its supply was off
// (on the ADV the LED VDD is the backlight rail, which is off while the screen is off).
constexpr uint32_t kRefreshMs = 50;
}  // namespace

void StatusLed::begin() {
    // Plain NeoPixel output through the Arduino core (RMT, 24-bit GRB). M5.Led is not used:
    // M5Unified's RMT LED bus is empty on ESP-IDF 4.x (Arduino core 2.x) and sends nothing.
    pin_ = M5.getPin(m5::pin_name_t::rgb_led);  // GPIO21 on both models
    show(0, 0);
}

void StatusLed::setEnabled(bool enabled) {
    enabled_ = enabled;
    if (!enabled_) {
        stepCount_ = 0;
        show(0, 0);
    }
}

void StatusLed::setBackground(LedPattern pattern, uint32_t colour) {
    pattern_ = pattern;
    patternColour_ = colour;
}

void StatusLed::startSteps() {
    stepCount_ = 0;
    stepsStartedAt_ = millis();
}

bool StatusLed::stepsPlaying() const {
    uint32_t total = 0;
    for (size_t i = 0; i < stepCount_; ++i) {
        total += steps_[i].durationMs;
    }
    return millis() - stepsStartedAt_ < total;
}

void StatusLed::addStep(uint32_t colour, uint16_t durationMs) {
    if (stepCount_ < kMaxSteps && durationMs > 0) {
        steps_[stepCount_++] = {colour, durationMs};
    }
}

void StatusLed::flash(uint32_t colour, uint16_t durationMs) {
    startSteps();
    addStep(colour, durationMs);
}

void StatusLed::blink(uint32_t colour, uint8_t count, uint16_t onMs, uint16_t offMs, bool afterCurrent) {
    if (!afterCurrent || !stepsPlaying()) {
        startSteps();
    }
    for (uint8_t i = 0; i < count; ++i) {
        addStep(colour, onMs);
        addStep(0, offMs);
    }
}

void StatusLed::playSequence(const uint32_t* colours, size_t count, uint16_t stepMs) {
    startSteps();
    for (size_t i = 0; i < count; ++i) {
        addStep(colours[i], stepMs);
    }
}

void StatusLed::loop() {
    if (pin_ < 0 || !enabled_) {
        return;
    }
    const uint32_t now = millis();

    // Timed steps win over the background pattern.
    uint32_t elapsed = now - stepsStartedAt_;
    for (size_t i = 0; i < stepCount_; ++i) {
        if (elapsed < steps_[i].durationMs) {
            show(steps_[i].colour, steps_[i].colour != 0 ? kMaxLevel : 0);
            return;
        }
        elapsed -= steps_[i].durationMs;
    }
    stepCount_ = 0;

    switch (pattern_) {
        case LedPattern::Off:
            show(0, 0);
            break;
        case LedPattern::Steady:
            show(patternColour_, kSteadyLevel);
            break;
        case LedPattern::Breathing: {
            const float phase = static_cast<float>(now % kBreathPeriodMs) / kBreathPeriodMs;
            const float wave = 0.5f - 0.5f * cosf(phase * 2.0f * PI);
            // Quantised to steps of 4 so the LED is not rewritten on every loop iteration.
            const uint8_t level = static_cast<uint8_t>(8 + wave * (kMaxLevel / 2)) & 0xFC;
            show(patternColour_, level);
            break;
        }
    }
}

void StatusLed::show(uint32_t colour, uint8_t level) {
    if (pin_ < 0) {
        return;
    }
    const uint32_t now = millis();
    const bool unchanged = colour == shownColour_ && level == shownLevel_;
    if (unchanged && (level == 0 || now - lastWriteAt_ < kRefreshMs)) {
        return;
    }
    shownColour_ = colour;
    shownLevel_ = level;
    lastWriteAt_ = now;
    const uint8_t red = ((colour >> 16) & 0xFF) * level / 255;
    const uint8_t green = ((colour >> 8) & 0xFF) * level / 255;
    const uint8_t blue = (colour & 0xFF) * level / 255;
    neopixelWrite(static_cast<uint8_t>(pin_), red, green, blue);
}

}  // namespace voicerec
