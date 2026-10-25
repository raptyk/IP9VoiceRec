// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#pragma once

#include <M5GFX.h>

#include <memory>
#include <vector>

#include "app/Settings.h"
#include "hw/Device.h"
#include "hw/KeyboardInput.h"
#include "hw/StatusLed.h"
#include "ui/Screen.h"

namespace voicerec {

// Owns the services and the screen stack; drives the main loop (Arduino loop task, core 1).
// Audio runs in its own tasks (Recorder, Player), so nothing here ever waits for audio.
class App {
public:
    App();

    void begin();
    void loop();

    // Screen stack.
    void push(std::unique_ptr<Screen> screen);
    void pop();
    void popToRoot();
    void invalidate() { dirty_ = true; }

    // Short message shown above the footer for ~2 s.
    void toast(const String& message);
    // A toast for something that went wrong or needs doing first, plus a red double blink.
    void warn(const String& message);
    void signalWarning();
    void signalSuccess();

    // Starts a recording and opens the record screen; warns and returns false if it cannot.
    bool startRecording();

    Settings& settings() { return settings_; }
    void saveSettings();
    // Pushes the settings to display, LED, recorder and player.
    void applySettings();

    Device& device() { return device_; }
    KeyboardInput& keyboard() { return keyboard_; }

private:
    void handleInput();
    void updatePower();
    void updateStatusLed();
    void refreshStorage();
    void render();
    void drawStatusBar();
    void drawFooter(const char* text);
    void drawToast();

    Settings settings_;
    Device device_;
    KeyboardInput keyboard_;
    StatusLed statusLed_;

    M5Canvas canvas_;
    std::vector<std::unique_ptr<Screen>> screens_;
    std::vector<std::unique_ptr<Screen>> retired_;  // popped screens, freed after the current event
    bool dirty_ = true;
    uint32_t lastRenderAt_ = 0;
    uint32_t lastStatusRefreshAt_ = 0;
    uint32_t lastStorageCheckAt_ = 0;

    String toastText_;
    uint32_t toastUntil_ = 0;
    uint32_t lastBatteryBlinkAt_ = 0;

    uint32_t lastInputAt_ = 0;
    bool screenDimmed_ = false;
    bool screenOff_ = false;
};

}  // namespace voicerec
