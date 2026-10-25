// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#pragma once

#include "app/Settings.h"
#include "ui/Screen.h"

namespace voicerec {

// Owns the services and drives the main loop (Arduino loop task, core 1) on top of the shared
// IP9Kit shell (screen stack, status bar, toasts, dimming, status LED).
// Audio runs in its own tasks (Recorder, Player), so nothing here ever waits for audio.
class App : public ip9::Shell {
public:
    void begin();
    // Shell::loop() plus a 1 ms yield for the audio tasks.
    void loop();

    // Starts a recording and opens the record screen; warns and returns false if it cannot.
    bool startRecording();

    Settings& settings() { return settings_; }
    void saveSettings();
    // Pushes the settings to display, LED, recorder and player.
    void applySettings();

protected:
    ip9::StatusLine statusLine(const ip9::Screen& top) override;
    void onLoop() override;
    void updateStatusLed() override;

private:
    void refreshStorage();

    Settings settings_;
    uint32_t lastStorageCheckAt_ = 0;
};

}  // namespace voicerec
