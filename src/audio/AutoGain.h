// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace voicerec {

// What the capture task knows about a frame when it hands it to the writer.
struct FrameInfo {
    float rmsDbfs = -90.0f;    // after the microphone gain and the limiter
    float peakDbfs = -90.0f;
    float noiseDbfs = -60.0f;  // tracked noise floor at that moment
    bool voiced = false;       // the voice detector hears a voice in this frame
    int8_t inputGainDb = 0;    // microphone gain the frame was recorded with
};

// Automatic level ("Auto level") of the recorded file: brings speech to a steady, clearly
// audible level whatever the distance to the microphone, the board and the gain setting.
//
// It runs in the MP3 writer with a lookahead of kLookaheadFrames (288 ms): every frame is
// observed when it enters the window and gets its gain when it leaves, so the gain is known
// before a voice starts (the pre-roll and the first syllable already have it) and is lowered
// before a loud sound arrives instead of after.
//
//  - Speech level: the envelope of the loud syllables of voiced frames only (fast up, slowly
//    down), so silence, pauses, consonants and steady noise never pull the gain up.
//  - Gain = target - speech level, between kMinGainDb and kMaxGainDb, and never so high that
//    the noise floor would rise above kNoiseCeilingDbfs (a noisy room stays a quiet hiss).
//  - Until a session has heard ~1 s of voice the gain follows at once (new speaker, new
//    distance); afterwards it rises at most kRiseDbPerFrame and only while a voice is in the
//    window (no pumping in pauses), and falls at most kFallDbPerFrame.
//  - Peak guard: the gain of a frame never lets the peaks of this and the next two frames
//    exceed kPeakCeilingDbfs, so knocks and sudden loud words are not amplified into the limiter.
//  - The learned speech level and gain carry over to the next recording as a starting point.
//
// Not thread-safe: used by the writer task only.
class AutoGain {
public:
    static constexpr size_t kLookaheadFrames = 8;
    static constexpr float kTargetSpeechDbfs = -19.0f;  // loud speech frames; peaks around -5 dBFS
    static constexpr float kMinGainDb = -12.0f;
    static constexpr float kMaxGainDb = 30.0f;
    static constexpr float kNoiseCeilingDbfs = -42.0f;
    static constexpr float kPeakCeilingDbfs = -3.0f;
    static constexpr float kRiseDbPerFrame = 0.2f;  // ~5.5 dB/s once settled
    static constexpr float kFallDbPerFrame = 1.0f;
    static constexpr uint32_t kSettleVoicedFrames = 28;  // ~1 s of voice

    // Starts a recording: empties the window, keeps the learned level as the starting point.
    void beginSession();
    // Forgets everything learned (tests, a different microphone).
    void resetLearning();

    // Adds a frame at the end of the window.
    void observe(const FrameInfo& info);
    // Frames in the window.
    size_t pending() const { return count_; }
    bool full() const { return count_ == kLookaheadFrames; }
    // Removes the oldest frame from the window and returns its gain in dB.
    float takeGainDb();

    // Last gain handed out (dB).
    float currentGainDb() const { return appliedDb_; }
    bool settled() const { return voicedFrames_ >= kSettleVoicedFrames; }
    bool knowsSpeech() const { return speechKnown_; }
    float speechDbfs() const { return speechDb_; }

private:
    const FrameInfo& at(size_t i) const { return window_[(head_ + i) % kLookaheadFrames]; }
    void trackSpeech(const FrameInfo& info);
    float targetGainDb(const FrameInfo& front) const;
    float peakLimitDb() const;
    bool voiceAhead() const;

    FrameInfo window_[kLookaheadFrames];
    size_t head_ = 0;  // oldest frame
    size_t count_ = 0;

    bool speechKnown_ = false;
    float speechDb_ = -90.0f;
    float envelopeDb_ = -90.0f;  // syllable peaks
    uint32_t voicedFrames_ = 0;  // this session, counted on entry to the window
    bool inputGainKnown_ = false;
    int8_t inputGainDb_ = 0;
    float stateDb_ = 0.0f;    // smoothed gain
    float appliedDb_ = 0.0f;  // after the peak guard
};

}  // namespace voicerec
