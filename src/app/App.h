// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#pragma once

#include <memory>
#include <vector>
#include <string>
#include <M5Cardputer.h>
#include "../ui/Screen.h"
#include "Settings.h"

namespace voicerec {

class App {
public:
    App();
    ~App() = default;

    void begin();
    void loop();

    void push(std::unique_ptr<Screen> screen);
    void pop();
    void replace(std::unique_ptr<Screen> screen);

    void toast(const std::string& message, uint32_t durationMs = 1800);

    M5Canvas& canvas() { return canvas_; }
    Settings& settings() { return settings_; }

private:
    void render();

    M5Canvas canvas_;
    Settings settings_;

    std::vector<std::unique_ptr<Screen>> screens_;
    std::string toastMessage_;
    uint32_t toastUntil_ = 0;

    uint32_t lastRenderAt_ = 0;
};

}  // namespace voicerec
