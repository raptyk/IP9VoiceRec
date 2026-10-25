// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#pragma once

#include <stdint.h>

namespace voicerec {

enum class AudioMode : uint8_t {
    Idle = 0,       // Both Mic and Speaker off (saves battery, no amp hiss)
    Record = 1,     // Mic active (Speaker strictly off)
    Playback = 2    // Speaker active (Mic strictly off)
};

class AudioModeManager {
public:
    static AudioModeManager& instance() {
        static AudioModeManager inst;
        return inst;
    }

    void begin();
    bool setMode(AudioMode mode);
    AudioMode currentMode() const { return currentMode_; }

    void setSpeakerVolume(uint8_t volume);
    uint8_t speakerVolume() const { return speakerVolume_; }

private:
    AudioModeManager() = default;

    AudioMode currentMode_ = AudioMode::Idle;
    uint8_t speakerVolume_ = 180;
};

}  // namespace voicerec
