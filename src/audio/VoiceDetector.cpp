// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#include "VoiceDetector.h"

#include <math.h>

#include "audio/AudioConfig.h"

namespace voicerec {

namespace {
constexpr float kMargins[] = {16.0f, 12.0f, 9.0f, 7.0f, 5.0f};
constexpr float kMinTrigger = -66.0f;  // never trigger on the converter's own hiss
constexpr float kNoiseMin = -90.0f;
constexpr float kNoiseMax = -20.0f;
constexpr float kDeviationMin = 0.5f;
constexpr float kDeviationMax = 6.0f;
constexpr float kDeviationStart = 1.5f;
// The trigger stays this many deviations above the noise, so a fluctuating background
// (traffic, fan with a beat) needs more than the configured margin to start a recording.
constexpr float kTriggerDeviations = 3.0f;
constexpr float kReleaseDeviations = 2.0f;
constexpr float kReleaseMarginShare = 0.6f;
constexpr float kMinReleaseAboveNoise = 3.0f;
// Learning: the first second after a reset takes the minimum level seen (speech has gaps,
// a steady background does not) plus two deviations, whatever the start value was.
constexpr uint32_t kLearnFrames = 28;
// Per 36 ms frame, between voices: the mean level of the background.
constexpr float kRiseRate = 0.03f;  // ~1.2 s
constexpr float kFallRate = 0.08f;  // ~0.45 s
constexpr float kDeviationRate = 0.03f;
// While a voice is detected only a falling background is followed, plus a slow pull up to the
// 2 s minimum: a background that got louder (a fan) can only hold a recording for a few seconds.
constexpr float kRecoveryRate = 0.01f;  // ~3.6 s
}  // namespace

float VoiceDetector::marginForSensitivity(uint8_t sensitivity) {
    if (sensitivity < kSensitivityMin) {
        sensitivity = kSensitivityMin;
    } else if (sensitivity > kSensitivityMax) {
        sensitivity = kSensitivityMax;
    }
    return kMargins[sensitivity - kSensitivityMin];
}

void VoiceDetector::configure(uint8_t sensitivity, uint32_t holdMs) {
    margin_ = marginForSensitivity(sensitivity);
    holdMs_ = holdMs;
}

void VoiceDetector::reset(float noiseFloorDbfs) {
    state_ = VoiceState::Silence;
    noise_ = fminf(kNoiseMax, fmaxf(kNoiseMin, noiseFloorDbfs));
    deviation_ = kDeviationStart;
    holdRemainingMs_ = 0;
    previousAboveTrigger_ = false;
    previousAboveRelease_ = false;
    framesSinceReset_ = 0;
    windowHead_ = 0;
    windowCount_ = 0;
}

void VoiceDetector::shiftNoise(float db) {
    noise_ = fminf(kNoiseMax, fmaxf(kNoiseMin, noise_ + db));
    for (size_t i = 0; i < windowCount_; ++i) {
        window_[i] += db;
    }
}

float VoiceDetector::triggerLevel() const {
    return fmaxf(kMinTrigger, noise_ + fmaxf(margin_, kTriggerDeviations * deviation_));
}

float VoiceDetector::releaseLevel() const {
    const float above = fmaxf(kMinReleaseAboveNoise,
                              fmaxf(margin_ * kReleaseMarginShare, kReleaseDeviations * deviation_));
    return fminf(triggerLevel(), noise_ + above);
}

void VoiceDetector::trackNoise(float levelDbfs, float trigger) {
    window_[windowHead_] = levelDbfs;
    windowHead_ = (windowHead_ + 1) % kWindowFrames;
    if (windowCount_ < kWindowFrames) {
        ++windowCount_;
    }
    ++framesSinceReset_;
    float windowMin = window_[0];
    for (size_t i = 1; i < windowCount_; ++i) {
        windowMin = fminf(windowMin, window_[i]);
    }

    if (framesSinceReset_ <= kLearnFrames) {
        noise_ = windowMin + kReleaseDeviations * deviation_;
    } else if (state_ == VoiceState::Silence && levelDbfs < trigger) {
        const float difference = levelDbfs - noise_;
        noise_ += difference * (difference > 0 ? kRiseRate : kFallRate);
        deviation_ += (fabsf(difference) - deviation_) * kDeviationRate;
    } else {
        if (levelDbfs < noise_) {
            noise_ += (levelDbfs - noise_) * kFallRate;
        }
        const float target = windowMin + kReleaseDeviations * deviation_;
        if (target > noise_) {
            noise_ += (target - noise_) * kRecoveryRate;
        }
    }
    deviation_ = fminf(kDeviationMax, fmaxf(kDeviationMin, deviation_));
    noise_ = fminf(kNoiseMax, fmaxf(kNoiseMin, noise_));
}

VoiceDecision VoiceDetector::process(float levelDbfs) {
    VoiceDecision decision{state_, false, false, false};
    const float trigger = triggerLevel();
    const float release = releaseLevel();
    const bool aboveTrigger = levelDbfs >= trigger;
    const bool aboveRelease = levelDbfs >= release;

    switch (state_) {
        case VoiceState::Silence:
            // Two loud frames in a row (72 ms): a syllable, not a key click or a knock.
            if (aboveTrigger && previousAboveTrigger_) {
                state_ = VoiceState::Voice;
                holdRemainingMs_ = holdMs_;
                decision.onset = true;
            }
            break;
        case VoiceState::Voice:
            if (aboveRelease) {
                holdRemainingMs_ = holdMs_;
            } else {
                state_ = VoiceState::Hold;
            }
            break;
        case VoiceState::Hold:
            // Back to voice on a loud frame, or on two frames above the release level
            // (a single noisy frame must not restart the hold time).
            if (aboveTrigger || (aboveRelease && previousAboveRelease_)) {
                state_ = VoiceState::Voice;
                holdRemainingMs_ = holdMs_;
            } else if (holdRemainingMs_ > kFrameMs) {
                holdRemainingMs_ -= kFrameMs;
            } else {
                holdRemainingMs_ = 0;
                state_ = VoiceState::Silence;
                decision.released = true;
            }
            break;
    }
    previousAboveTrigger_ = aboveTrigger;
    previousAboveRelease_ = aboveRelease;
    trackNoise(levelDbfs, trigger);

    decision.state = state_;
    decision.writing = state_ != VoiceState::Silence;
    return decision;
}

}  // namespace voicerec
