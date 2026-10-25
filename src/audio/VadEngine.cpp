// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#include "VadEngine.h"
#include <cmath>
#include <algorithm>

namespace voicerec {

VadEngine::VadEngine() {
    reset();
}

void VadEngine::reset() {
    state_ = VadState::Silence;
    remainingHangoverMs_ = 0;
    lastRmsDbfs_ = -90.0f;
    lastPeakDbfs_ = -90.0f;
    calibrating_ = false;
    calibrationLevels_.clear();
}

void VadEngine::setAttackThreshold(float attackDbfs) {
    attackThresholdDbfs_ = std::max(-70.0f, std::min(-12.0f, attackDbfs));
}

void VadEngine::setHysteresisMargin(float marginDb) {
    hysteresisMarginDb_ = std::max(2.0f, std::min(15.0f, marginDb));
}

void VadEngine::setHangoverMs(uint32_t hangoverMs) {
    hangoverMs_ = std::max(kMinHangoverMs, std::min(kMaxHangoverMs, hangoverMs));
}

float VadEngine::calculateRmsDbfs(const int16_t* samples, size_t count) {
    if (count == 0) return -90.0f;

    double sumSq = 0.0;
    for (size_t i = 0; i < count; ++i) {
        const double s = static_cast<double>(samples[i]);
        sumSq += s * s;
    }

    const double rms = std::sqrt(sumSq / static_cast<double>(count));
    if (rms <= 1.0) {
        return -90.3f;
    }
    return static_cast<float>(20.0 * std::log10(rms / 32767.0));
}

float VadEngine::calculatePeakDbfs(const int16_t* samples, size_t count) {
    if (count == 0) return -90.0f;

    int16_t maxAbs = 0;
    for (size_t i = 0; i < count; ++i) {
        const int16_t v = std::abs(samples[i]);
        if (v > maxAbs) {
            maxAbs = v;
        }
    }

    if (maxAbs <= 1) {
        return -90.3f;
    }
    return static_cast<float>(20.0 * std::log10(static_cast<double>(maxAbs) / 32767.0));
}

VadFrameResult VadEngine::processFrame(const int16_t* samples, size_t count) {
    const float rmsDbfs = calculateRmsDbfs(samples, count);
    const float peakDbfs = calculatePeakDbfs(samples, count);
    lastRmsDbfs_ = rmsDbfs;
    lastPeakDbfs_ = peakDbfs;

    VadFrameResult res;
    res.rmsDbfs = rmsDbfs;
    res.peakDbfs = peakDbfs;
    res.speechTriggered = false;

    // If VAS is disabled, always record
    if (!vasEnabled_) {
        state_ = VadState::Speech;
        res.state = VadState::Speech;
        return res;
    }

    const float attackT = attackThresholdDbfs_;
    const float releaseT = attackT - hysteresisMarginDb_;
    const uint32_t frameMs = static_cast<uint32_t>(kFrameDurationMs);

    switch (state_) {
        case VadState::Silence:
            if (rmsDbfs >= attackT) {
                // Speech onset!
                state_ = VadState::Speech;
                remainingHangoverMs_ = hangoverMs_;
                res.speechTriggered = true;
            }
            break;

        case VadState::Speech:
            if (rmsDbfs >= releaseT) {
                // Voice maintained above release threshold
                remainingHangoverMs_ = hangoverMs_;
            } else {
                // Voice dipped below release threshold, transition to Hangover
                state_ = VadState::Hangover;
                if (remainingHangoverMs_ >= frameMs) {
                    remainingHangoverMs_ -= frameMs;
                } else {
                    remainingHangoverMs_ = 0;
                }
            }
            break;

        case VadState::Hangover:
            if (rmsDbfs >= releaseT) {
                // Speech resumed before hangover expired
                state_ = VadState::Speech;
                remainingHangoverMs_ = hangoverMs_;
            } else {
                // Decrement hangover timer
                if (remainingHangoverMs_ >= frameMs) {
                    remainingHangoverMs_ -= frameMs;
                } else {
                    remainingHangoverMs_ = 0;
                    state_ = VadState::Silence;
                }
            }
            break;
    }

    res.state = state_;
    return res;
}

void VadEngine::startCalibration() {
    calibrating_ = true;
    calibrationLevels_.clear();
    calibrationLevels_.reserve(kCalibrationFrames);
}

bool VadEngine::addCalibrationFrame(const int16_t* samples, size_t count) {
    if (!calibrating_) return false;

    const float level = calculateRmsDbfs(samples, count);
    calibrationLevels_.push_back(level);

    if (calibrationLevels_.size() >= kCalibrationFrames) {
        calibrating_ = false;
        return true; // calibration complete
    }
    return false;
}

CalibrationResult VadEngine::finishCalibration() {
    CalibrationResult res;
    calibrating_ = false;

    if (calibrationLevels_.size() < 20) {
        res.valid = false;
        return res;
    }

    std::sort(calibrationLevels_.begin(), calibrationLevels_.end());

    // 40th percentile: stable ambient noise floor
    const size_t p40Idx = (calibrationLevels_.size() * 40) / 100;
    // 85th percentile: ambient noise ceiling
    const size_t p85Idx = (calibrationLevels_.size() * 85) / 100;

    res.noiseFloorDbfs = calibrationLevels_[p40Idx];
    res.peakNoiseDbfs = calibrationLevels_[p85Idx];

    // Recommended attack: P40 + 9 dB (bounded between -52 dB and -26 dB)
    float recAttack = res.noiseFloorDbfs + 9.0f;
    recAttack = std::max(-52.0f, std::min(-26.0f, recAttack));

    res.recommendedAttackDbfs = recAttack;
    res.recommendedReleaseDbfs = recAttack - 5.0f;
    res.valid = true;

    // Apply automatically
    attackThresholdDbfs_ = recAttack;
    hysteresisMarginDb_ = 5.0f;

    return res;
}

}  // namespace voicerec
