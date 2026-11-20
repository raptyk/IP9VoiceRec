// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#pragma once

#include "ui/Screen.h"

namespace voicerec {

// The running recording. Tinted red so it never looks like a menu: a band with the state
// (listening / recording / paused), a large timer of the recorded audio, the level meter with
// the noise floor and the voice trigger, and the three most important keys as drawn caps.
// The recording is already running when this screen opens (App::startRecording).
// With "Lock when recording" the screen is switched off and every key only lights up a neutral
// "Working..." message (whole display, nothing else, status light off) for a moment;
// esc unlocks the screen, the recording goes on.
class RecordScreen : public Screen {
public:
    using Screen::Screen;

    void onEnter() override;
    void onKey(const KeyEvent& event) override;
    void tick() override;
    void draw(M5Canvas& canvas, int top, int height) override;
    const char* footer() const override;
    bool fullScreen() const override { return locked_; }
    const char* chipLabel() const override { return str::kChipRecord; }
    uint16_t chipColour() const override { return theme::kRecord; }

private:
    void requestStop(const char* error = nullptr);
    void finish();
    void changeSensitivity(int delta);
    void changeGain(int delta);
    void lock();
    void unlock();
    // Lights the screen with the locked message for a moment.
    void showLockedMessage();
    void drawLocked(M5Canvas& canvas, int top, int height);

    void drawBand(M5Canvas& canvas, int top);
    void drawMiddle(M5Canvas& canvas, int top, int height);
    void drawMeter(M5Canvas& canvas, int top);
    void drawKeyRow(M5Canvas& canvas, int top, int height);

    bool entered_ = false;
    bool locked_ = false;
    uint32_t lockedMessageUntil_ = 0;  // 0 = screen dark
    bool stopping_ = false;
    const char* error_ = nullptr;  // shown after saving
    bool settingsChanged_ = false;
    float peakHold_ = -90.0f;
    uint32_t peakHoldAt_ = 0;
};

}  // namespace voicerec
