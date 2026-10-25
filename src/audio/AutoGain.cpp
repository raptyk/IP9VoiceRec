// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#include "AutoGain.h"

#include <math.h>

namespace voicerec {

namespace {
// A voiced frame must be this far above the noise to count as speech (weak frames at the
// edges of words would otherwise drag the speech level towards the noise).
constexpr float kVoicedAboveNoiseDb = 6.0f;
// Syllable envelope: jumps to every louder voiced frame and sinks by this much per voiced
// frame, so dips between syllables (consonants, word endings) barely move it, while a voice
// that stays quieter (after a shout, a step back) pulls it down within a few seconds.
constexpr float kEnvelopeDecayDb = 0.35f;
// The speech level follows the envelope: fast up (a louder voice would clip otherwise),
// slower down. Until settled both ways are quick: the first words decide the gain.
constexpr float kSpeechRise = 0.3f;
constexpr float kSpeechFall = 0.04f;
constexpr float kSpeechRiseLearning = 0.5f;
constexpr float kSpeechFallLearning = 0.2f;
// Frames whose peak counts for the peak guard: the current one and the next two.
constexpr size_t kPeakGuardFrames = 3;
// Peaks at or below this are digital silence: no limit.
constexpr float kNoPeakDbfs = -89.0f;
// The gain carried into the next recording is pulled towards 0 dB a little: a new session
// starts slightly cautious and rises with the first voice, rather than starting too loud.
constexpr float kCarryOverFactor = 0.75f;
}  // namespace

void AutoGain::beginSession() {
    head_ = 0;
    count_ = 0;
    voicedFrames_ = 0;
    // inputGainDb_ stays: a mic gain changed between recordings shifts the learned level too.
    stateDb_ *= kCarryOverFactor;
    appliedDb_ = stateDb_;
}

void AutoGain::resetLearning() {
    beginSession();
    inputGainKnown_ = false;
    speechKnown_ = false;
    speechDb_ = -90.0f;
    envelopeDb_ = -90.0f;
    stateDb_ = 0.0f;
    appliedDb_ = 0.0f;
}

void AutoGain::observe(const FrameInfo& info) {
    // The microphone gain changed: the same voice now arrives that much louder or quieter.
    if (inputGainKnown_ && info.inputGainDb != inputGainDb_ && speechKnown_) {
        speechDb_ += static_cast<float>(info.inputGainDb - inputGainDb_);
        envelopeDb_ += static_cast<float>(info.inputGainDb - inputGainDb_);
    }
    inputGainKnown_ = true;
    inputGainDb_ = info.inputGainDb;

    trackSpeech(info);

    if (count_ == kLookaheadFrames) {
        // Callers take a gain before adding more; never overwrite silently.
        takeGainDb();
    }
    window_[(head_ + count_) % kLookaheadFrames] = info;
    ++count_;
}

void AutoGain::trackSpeech(const FrameInfo& info) {
    if (!info.voiced || info.rmsDbfs < info.noiseDbfs + kVoicedAboveNoiseDb) {
        return;
    }
    ++voicedFrames_;
    if (!speechKnown_) {
        speechKnown_ = true;
        speechDb_ = info.rmsDbfs;
        envelopeDb_ = info.rmsDbfs;
        return;
    }
    envelopeDb_ = fmaxf(info.rmsDbfs, envelopeDb_ - kEnvelopeDecayDb);
    const bool learning = !settled();
    const float delta = envelopeDb_ - speechDb_;
    if (delta > 0.0f) {
        speechDb_ += delta * (learning ? kSpeechRiseLearning : kSpeechRise);
    } else {
        speechDb_ += delta * (learning ? kSpeechFallLearning : kSpeechFall);
    }
}

float AutoGain::targetGainDb(const FrameInfo& front) const {
    if (!speechKnown_) {
        return stateDb_;  // nothing heard yet: keep what we have
    }
    // A noisy room may stop the gain from rising, but never forces it below 0 dB.
    const float noiseLimit = fmaxf(0.0f, fminf(kMaxGainDb, kNoiseCeilingDbfs - front.noiseDbfs));
    const float wanted = kTargetSpeechDbfs - speechDb_;
    return fmaxf(kMinGainDb, fminf(noiseLimit, wanted));
}

float AutoGain::peakLimitDb() const {
    float limit = kMaxGainDb;
    const size_t frames = count_ < kPeakGuardFrames ? count_ : kPeakGuardFrames;
    for (size_t i = 0; i < frames; ++i) {
        const float peak = at(i).peakDbfs;
        if (peak > kNoPeakDbfs) {
            limit = fminf(limit, kPeakCeilingDbfs - peak);
        }
    }
    return limit;
}

bool AutoGain::voiceAhead() const {
    for (size_t i = 0; i < count_; ++i) {
        if (at(i).voiced) {
            return true;
        }
    }
    return false;
}

float AutoGain::takeGainDb() {
    if (count_ == 0) {
        return appliedDb_;
    }
    const FrameInfo& front = at(0);
    const float target = targetGainDb(front);
    if (!settled()) {
        // Learning: jump straight to the target once there is speech to judge.
        if (speechKnown_ && (target < stateDb_ || voiceAhead())) {
            stateDb_ = target;
        }
    } else if (target < stateDb_) {
        stateDb_ = fmaxf(target, stateDb_ - kFallDbPerFrame);
    } else if (voiceAhead()) {
        stateDb_ = fminf(target, stateDb_ + kRiseDbPerFrame);
    }
    // The peak guard only lowers this frame's gain; the smoothed gain is kept for later.
    appliedDb_ = fminf(stateDb_, peakLimitDb());

    head_ = (head_ + 1) % kLookaheadFrames;
    --count_;
    return appliedDb_;
}

}  // namespace voicerec
