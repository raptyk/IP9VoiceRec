// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <vector>
#include "AudioConfig.h"

namespace voicerec {

enum class VadState : uint8_t {
    Silence = 0,    // Ambient noise / silence (waiting for speech)
    Speech = 1,     // Active speech detected (writing audio)
    Hangover = 2    // Voice dropped below release threshold; hold timer active
};

struct VadFrameResult {
    float rmsDbfs = -90.0f;
    float peakDbfs = -90.0f;
    VadState state = VadState::Silence;
    bool speechTriggered = false; // True only on the exact transition from Silence -> Speech
    bool isWriting() const { return state == VadState::Speech || state == VadState::Hangover; }
};

struct CalibrationResult {
    float noiseFloorDbfs = -60.0f; // P40 percentile (stable ambient noise)
    float peakNoiseDbfs = -50.0f;  // P85 percentile (ambient peaks)
    float recommendedAttackDbfs = -36.0f;
    float recommendedReleaseDbfs = -41.0f;
    bool valid = false;
};

class VadEngine {
public:
    VadEngine();

    void reset();

    // Configuration
    void setAttackThreshold(float attackDbfs);
    float attackThreshold() const { return attackThresholdDbfs_; }

    void setHysteresisMargin(float marginDb);
    float hysteresisMargin() const { return hysteresisMarginDb_; }
    float releaseThreshold() const { return attackThresholdDbfs_ - hysteresisMarginDb_; }

    void setHangoverMs(uint32_t hangoverMs);
    uint32_t hangoverMs() const { return hangoverMs_; }

    void setVasEnabled(bool enabled) { vasEnabled_ = enabled; }
    bool isVasEnabled() const { return vasEnabled_; }

    // Real-time processing
    VadFrameResult processFrame(const int16_t* samples, size_t count);

    // Live level inspection
    float currentRmsDbfs() const { return lastRmsDbfs_; }
    float currentPeakDbfs() const { return lastPeakDbfs_; }
    VadState currentState() const { return state_; }
    uint32_t remainingHangoverMs() const { return remainingHangoverMs_; }

    // Calibration support
    void startCalibration();
    bool addCalibrationFrame(const int16_t* samples, size_t count);
    bool isCalibrating() const { return calibrating_; }
    size_t calibrationFramesCollected() const { return calibrationLevels_.size(); }
    CalibrationResult finishCalibration();

    static float calculateRmsDbfs(const int16_t* samples, size_t count);
    static float calculatePeakDbfs(const int16_t* samples, size_t count);

private:
    bool vasEnabled_ = true;
    float attackThresholdDbfs_ = kDefaultAttackThresholdDbfs;
    float hysteresisMarginDb_ = kDefaultHysteresisMarginDb;
    uint32_t hangoverMs_ = kDefaultHangoverMs;

    VadState state_ = VadState::Silence;
    uint32_t remainingHangoverMs_ = 0;
    float lastRmsDbfs_ = -90.0f;
    float lastPeakDbfs_ = -90.0f;

    // Calibration state
    bool calibrating_ = false;
    std::vector<float> calibrationLevels_;
};

}  // namespace voicerec
