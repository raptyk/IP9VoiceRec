// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#pragma once

#include "ui/Screen.h"

namespace voicerec {

// Start screen: a carousel of large icons (Record, Recordings, Mic check, Settings, Help).
// Each card explains itself and shows whether it is ready (SD card, calibration).
class HomeScreen : public ip9::AppScreen<App, ip9::CarouselScreen> {
public:
    explicit HomeScreen(App& app);

    const char* footer() const override;

protected:
    CarouselCard card(int index) const override;
    void drawIcon(M5Canvas& canvas, int index, int cx, int cy, uint16_t colour, float scale,
                  uint16_t background) const override;
    void open(int index) override;
    void openHelp() override;
    bool onExtraKey(const KeyEvent& event) override;
};

}  // namespace voicerec
