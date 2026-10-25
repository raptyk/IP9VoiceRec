// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#pragma once

#include "ui/Screen.h"

namespace voicerec {

// Start screen: a carousel of large icons (Record, Recordings, Mic check, Settings, Help).
// Each card explains itself and shows whether it is ready (SD card, calibration).
class HomeScreen : public Screen {
public:
    using Screen::Screen;

    void onKey(const KeyEvent& event) override;
    void draw(M5Canvas& canvas, int top, int height) override;
    const char* footer() const override;

private:
    void open();
    void move(int delta);

    int selected_ = 0;
    int slideDirection_ = 0;  // -1 / +1 during the short slide animation
    uint32_t slideStartedAt_ = 0;
};

}  // namespace voicerec
