// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace voicerec {

enum class VoiceState : uint8_t {
    Silence,  // waiting for a voice: nothing is written
    Voice,    // somebody speaks: frames are written
    Hold      // voice just stopped: still writing until the hold time runs out
};

struct VoiceDecision {
    VoiceState state;
    bool writing;   // this frame belongs in the recording
    bool onset;     // first frame of a new voice segment: write the pre-roll in front of it
    bool released;  // the hold time just ran out (segment ended)
};

// Voice activation that adapts to the room.
//
// The trigger level is not an absolute dBFS value but a margin above a continuously tracked
// noise floor, so the same sensitivity works in a quiet bedroom and a noisy office. Between
// voices the floor follows the mean background level and its fluctuation (a restless background
// widens the margin by itself). While a voice is heard it only follows a falling background,
// plus a slow pull up to the minimum of the last two seconds - speech always has gaps, a steady
// noise does not - so a fan switched on mid-recording cannot keep the recording running.
//
// A voice starts after two loud frames in a row (72 ms: ignores key clicks and knocks; the
// pre-roll keeps the start of the syllable). It ends when the level stays below the lower
// release level for the hold time, so short pauses between words do not split the recording.
class VoiceDetector {
public:
    // sensitivity 1 (least, 16 dB above noise) .. 5 (most, 5 dB above noise).
    void configure(uint8_t sensitivity, uint32_t holdMs);
    // Starts a new session from a known (calibrated) or guessed noise floor.
    void reset(float noiseFloorDbfs);

    // The input gain changed by `db`: move the noise floor along instead of re-learning it.
    void shiftNoise(float db);

    VoiceDecision process(float levelDbfs);

    VoiceState state() const { return state_; }
    float noiseFloor() const { return noise_; }
    float triggerLevel() const;
    float releaseLevel() const;
    uint32_t holdRemainingMs() const { return holdRemainingMs_; }

    // Trigger margin above the noise floor for a sensitivity step (dB).
    static float marginForSensitivity(uint8_t sensitivity);

private:
    static constexpr size_t kWindowFrames = 56;  // ~2 s of minimum tracking

    void trackNoise(float levelDbfs, float trigger);

    float margin_ = 9.0f;
    uint32_t holdMs_ = 1500;

    VoiceState state_ = VoiceState::Silence;
    float noise_ = -60.0f;
    float deviation_ = 1.5f;  // mean distance of background frames from noise_ (dB)
    uint32_t holdRemainingMs_ = 0;
    bool previousAboveTrigger_ = false;
    bool previousAboveRelease_ = false;
    uint32_t framesSinceReset_ = 0;

    float window_[kWindowFrames] = {};
    size_t windowHead_ = 0;
    size_t windowCount_ = 0;
};

}  // namespace voicerec
