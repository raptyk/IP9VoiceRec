// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#pragma once

#include <M5GFX.h>

#include "hw/KeyboardInput.h"
#include "ui/Strings.h"
#include "ui/Theme.h"

namespace voicerec {

class App;

// A full-screen view. The App owns a stack of screens; only the top one is drawn
// and receives key events. The status bar and footer are drawn by the App.
class Screen {
public:
    explicit Screen(App& app) : app_(app) {}
    virtual ~Screen() = default;

    virtual void onEnter() {}
    virtual void onKey(const KeyEvent& event) = 0;
    // Called on every main loop iteration while the screen is on top.
    virtual void tick() {}
    // Draws the content area (between status bar and footer).
    virtual void draw(M5Canvas& canvas, int top, int height) = 0;
    virtual const char* footer() const { return nullptr; }
    // Chip on the left of the status bar: which section this is.
    virtual const char* chipLabel() const { return str::kChipApp; }
    virtual uint16_t chipColour() const { return theme::kAccent; }

protected:
    App& app_;
};

}  // namespace voicerec
