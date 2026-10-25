// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace voicerec {

// Background pattern of the RGB status LED.
enum class LedPattern : uint8_t {
    Off,        // idle: dark, friendly for a dark room and the battery
    Breathing,  // waiting for something (voice activation listening, saving)
    Steady      // something is running (recording a voice, paused)
};

// RGB status LED on GPIO21 (WS2812, 24-bit GRB) on Cardputer and Cardputer ADV.
// A background pattern, overlaid by a short queue of timed steps ("colour for N ms")
// that implements flashes, blink bursts and colour sequences. Colours are 0xRRGGBB.
class StatusLed {
public:
    void begin();
    void setEnabled(bool enabled);

    void setBackground(LedPattern pattern, uint32_t colour);

    // One short flash.
    void flash(uint32_t colour, uint16_t durationMs = 60);
    // `count` blinks, e.g. two green = saved, two red = error.
    // With `afterCurrent` the blinks wait until the steps already playing have finished.
    void blink(uint32_t colour, uint8_t count, uint16_t onMs = 90, uint16_t offMs = 110,
               bool afterCurrent = false);
    // Colours one after another (start-up greeting, LED test).
    void playSequence(const uint32_t* colours, size_t count, uint16_t stepMs);

    void loop();

private:
    struct Step {
        uint32_t colour;  // 0 = dark
        uint16_t durationMs;
    };

    static constexpr size_t kMaxSteps = 8;
    static constexpr uint8_t kMaxLevel = 110;  // of 255: visible in daylight, not glaring at night

    void startSteps();
    bool stepsPlaying() const;
    void addStep(uint32_t colour, uint16_t durationMs);
    void show(uint32_t colour, uint8_t level);

    int pin_ = -1;
    bool enabled_ = true;
    LedPattern pattern_ = LedPattern::Off;
    uint32_t patternColour_ = 0;

    Step steps_[kMaxSteps] = {};
    size_t stepCount_ = 0;
    uint32_t stepsStartedAt_ = 0;

    uint32_t shownColour_ = 0xFFFFFFFF;  // forces the first write
    uint8_t shownLevel_ = 0xFF;
    uint32_t lastWriteAt_ = 0;
};

}  // namespace voicerec
