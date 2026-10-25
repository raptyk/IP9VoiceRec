// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#pragma once

#include <M5Cardputer.h>
#include "Theme.h"
#include "../audio/VadEngine.h"

namespace voicerec {

class Painter {
public:
    static void drawHeader(M5Canvas& canvas, const char* title, int batteryPercent, bool charging, bool sdOk);
    static void drawFooter(M5Canvas& canvas, const char* hints);

    static void drawCard(M5Canvas& canvas, int x, int y, int w, int h,
                         uint16_t bg = theme::kPanelBg, uint16_t border = theme::kPanelBorder);

    static void drawVuMeter(M5Canvas& canvas, int x, int y, int w, int h,
                            float rmsDbfs, float peakDbfs, float thresholdDbfs, VadState state);

    static void drawProgressBar(M5Canvas& canvas, int x, int y, int w, int h,
                                float progress, uint16_t color = theme::kPrimary);

    static void drawBadge(M5Canvas& canvas, int x, int y, const char* text,
                          uint16_t bg, uint16_t fg = theme::kText);
};

}  // namespace voicerec
