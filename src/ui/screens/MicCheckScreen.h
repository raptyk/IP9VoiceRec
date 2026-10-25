// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#pragma once

#include "audio/Calibration.h"
#include "ui/Screen.h"

namespace voicerec {

// Live microphone level with the voice detector's view of it (noise floor, trigger level,
// "voice detected"), plus the two-step calibration:
//   1. 3 s of quiet  -> noise floor and how much the background fluctuates,
//   2. 4.5 s of speech -> voice level: sets the gain so speech lands at about -20 dBFS,
//      and picks the sensitivity from the measured voice-to-noise distance.
class MicCheckScreen : public Screen {
public:
    using Screen::Screen;
    ~MicCheckScreen() override;

    void onEnter() override;
    void onKey(const KeyEvent& event) override;
    void tick() override;
    void draw(M5Canvas& canvas, int top, int height) override;
    const char* footer() const override;
    const char* chipLabel() const override { return str::kChipMicCheck; }
    uint16_t chipColour() const override { return theme::kMicCheck; }

private:
    enum class Phase : uint8_t { Live, Quiet, Speech };

    void startCalibration();
    void finishCalibration();
    void changeSetting(char key);

    void drawBand(M5Canvas& canvas, int top);
    void drawLive(M5Canvas& canvas, int top, int height);
    void drawCalibration(M5Canvas& canvas, int top, int height);
    void drawMeter(M5Canvas& canvas, int y, int height, bool markers);
    void drawKeyRow(M5Canvas& canvas, int top, int height);

    Phase phase_ = Phase::Live;
    bool captureStarted_ = false;
    uint32_t phaseStartedAt_ = 0;
    bool settingsChanged_ = false;
    bool failureShown_ = false;
    calibration::NoiseResult noise_{};
    float peakHold_ = -90.0f;
    uint32_t peakHoldAt_ = 0;
};

}  // namespace voicerec
