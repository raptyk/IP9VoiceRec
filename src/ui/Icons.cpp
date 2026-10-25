// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#include "Icons.h"

#include <math.h>

namespace voicerec::icons {

namespace {

int s(float value, float scale) {
    return static_cast<int>(lroundf(value * scale));
}

void drawMic(M5Canvas& canvas, int cx, int cy, uint16_t colour, float k, uint16_t background) {
    // Capsule, U-shaped holder, stem and foot.
    const int capsuleWidth = s(14, k);
    const int capsuleHeight = s(22, k);
    canvas.fillRoundRect(cx - capsuleWidth / 2, cy - s(17, k), capsuleWidth, capsuleHeight, capsuleWidth / 2, colour);
    const int width = max(2, s(3, k));
    canvas.fillArc(cx, cy - s(2, k), s(12, k), s(12, k) - width, 0, 180, colour);
    canvas.fillRect(cx - width / 2, cy + s(10, k), width, s(6, k), colour);
    canvas.fillRoundRect(cx - s(8, k), cy + s(15, k), s(16, k), width, 1, colour);
    // Grille slots on the capsule.
    for (int i = 0; i < 3; ++i) {
        canvas.fillRect(cx - s(3, k), cy - s(12 - i * 5, k), s(6, k), max(1, s(1.5f, k)), background);
    }
}

void drawLibrary(M5Canvas& canvas, int cx, int cy, uint16_t colour, float k) {
    // Three list lines with a play triangle in front of the first one.
    const int height = max(2, s(4, k));
    canvas.fillTriangle(cx - s(16, k), cy - s(14, k), cx - s(16, k), cy - s(4, k), cx - s(8, k), cy - s(9, k), colour);
    canvas.fillRoundRect(cx - s(4, k), cy - s(11, k), s(20, k), height, height / 2, colour);
    canvas.fillRoundRect(cx - s(16, k), cy + s(1, k), s(32, k), height, height / 2, colour);
    canvas.fillRoundRect(cx - s(16, k), cy + s(11, k), s(32, k), height, height / 2, colour);
}

void drawLevels(M5Canvas& canvas, int cx, int cy, uint16_t colour, float k) {
    // Five level bars of different heights, like a VU meter.
    static constexpr float kHeights[] = {10, 18, 30, 22, 14};
    const int barWidth = max(2, s(5, k));
    const int gap = max(1, s(2.5f, k));
    const int total = 5 * barWidth + 4 * gap;
    int x = cx - total / 2;
    const int bottom = cy + s(15, k);
    for (float height : kHeights) {
        const int h = s(height, k);
        canvas.fillRoundRect(x, bottom - h, barWidth, h, max(1, barWidth / 3), colour);
        x += barWidth + gap;
    }
}

void drawSettings(M5Canvas& canvas, int cx, int cy, uint16_t colour, float k, uint16_t background) {
    const int outer = s(15, k);
    for (int tooth = 0; tooth < 8; ++tooth) {
        const float angle = tooth * PI / 4;
        const int x = cx + static_cast<int>(cosf(angle) * outer);
        const int y = cy + static_cast<int>(sinf(angle) * outer);
        canvas.fillCircle(x, y, max(2, s(4, k)), colour);
    }
    canvas.fillCircle(cx, cy, s(13, k), colour);
    canvas.fillCircle(cx, cy, s(6, k), background);
}

void drawHelp(M5Canvas& canvas, int cx, int cy, uint16_t colour, float k) {
    const int radius = s(17, k);
    canvas.fillArc(cx, cy, radius, radius - max(2, s(3, k)), 0, 360, colour);
    canvas.setFont(k >= 0.8f ? &fonts::FreeSansBold18pt7b : &fonts::FreeSansBold9pt7b);
    canvas.setTextSize(1);
    canvas.setTextColor(colour);
    canvas.setTextDatum(middle_center);
    canvas.drawString("?", cx + 1, cy + 1);
}

void drawPlay(M5Canvas& canvas, int cx, int cy, uint16_t colour, float k) {
    canvas.fillTriangle(cx - s(10, k), cy - s(14, k), cx - s(10, k), cy + s(14, k), cx + s(14, k), cy, colour);
}

void drawPause(M5Canvas& canvas, int cx, int cy, uint16_t colour, float k) {
    const int width = s(8, k);
    const int height = s(28, k);
    canvas.fillRoundRect(cx - s(11, k), cy - height / 2, width, height, 2, colour);
    canvas.fillRoundRect(cx + s(3, k), cy - height / 2, width, height, 2, colour);
}

}  // namespace

void draw(M5Canvas& canvas, Icon icon, int cx, int cy, uint16_t colour, float scale, uint16_t background) {
    switch (icon) {
        case Icon::Mic: drawMic(canvas, cx, cy, colour, scale, background); break;
        case Icon::Library: drawLibrary(canvas, cx, cy, colour, scale); break;
        case Icon::Levels: drawLevels(canvas, cx, cy, colour, scale); break;
        case Icon::Settings: drawSettings(canvas, cx, cy, colour, scale, background); break;
        case Icon::Help: drawHelp(canvas, cx, cy, colour, scale); break;
        case Icon::Play: drawPlay(canvas, cx, cy, colour, scale); break;
        case Icon::Pause: drawPause(canvas, cx, cy, colour, scale); break;
    }
}

}  // namespace voicerec::icons
