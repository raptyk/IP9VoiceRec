// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#pragma once

#include <stdint.h>
#include <stddef.h>

namespace voicerec {

enum class LedState : uint8_t {
    Off,
    Idle,
    Recording,      // Active speech recording (Red pulse)
    WaitingSilence, // VAS waiting for voice (Blue/Cyan steady)
    Hangover,       // VAS holding before silence (Yellow)
    Playback        // Audio playback (Green)
};

class StatusLed {
public:
    static StatusLed& instance() {
        static StatusLed inst;
        return inst;
    }

    void begin();
    void setEnabled(bool enabled);
    bool isEnabled() const { return enabled_; }

    void setState(LedState state);
    LedState state() const { return state_; }

    void loop();

private:
    StatusLed() = default;
    void show(uint8_t r, uint8_t g, uint8_t b);

    int pin_ = 21;
    bool enabled_ = true;
    LedState state_ = LedState::Off;
    uint32_t lastUpdateAt_ = 0;
};

}  // namespace voicerec
