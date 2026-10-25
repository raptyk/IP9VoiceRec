// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#include "Painter.h"
#include <algorithm>
#include <cmath>

namespace voicerec {

void Painter::drawHeader(M5Canvas& canvas, const char* title, int batteryPercent, bool charging, bool sdOk) {
    canvas.fillRect(0, 0, theme::kScreenWidth, theme::kHeaderHeight, 0x0000);
    canvas.drawFastHLine(0, theme::kHeaderHeight - 1, theme::kScreenWidth, theme::kPanelBorder);

    // Title on left
    canvas.setTextColor(theme::kPrimary, 0x0000);
    canvas.setTextSize(1);
    canvas.drawString(title, 6, 4);

    // Status indicators on right
    int x = theme::kScreenWidth - 6;

    // Battery icon + text
    char batBuf[16];
    if (batteryPercent >= 0) {
        snprintf(batBuf, sizeof(batBuf), "%d%%%s", batteryPercent, charging ? "+" : "");
    } else {
        snprintf(batBuf, sizeof(batBuf), "--%%");
    }
    const int batW = canvas.textWidth(batBuf);
    x -= batW;
    canvas.setTextColor(charging ? theme::kSuccess : theme::kText, 0x0000);
    canvas.drawString(batBuf, x, 4);

    // SD indicator
    x -= 24;
    canvas.setTextColor(sdOk ? theme::kSuccess : theme::kDanger, 0x0000);
    canvas.drawString("SD", x, 4);
}

void Painter::drawFooter(M5Canvas& canvas, const char* hints) {
    const int y = theme::kScreenHeight - theme::kFooterHeight;
    canvas.fillRect(0, y, theme::kScreenWidth, theme::kFooterHeight, 0x0000);
    canvas.drawFastHLine(0, y, theme::kScreenWidth, theme::kPanelBorder);

    canvas.setTextColor(theme::kTextDim, 0x0000);
    canvas.setTextSize(1);
    canvas.drawString(hints, 6, y + 4);
}

void Painter::drawCard(M5Canvas& canvas, int x, int y, int w, int h, uint16_t bg, uint16_t border) {
    canvas.fillRect(x, y, w, h, bg);
    canvas.drawRect(x, y, w, h, border);
}

void Painter::drawVuMeter(M5Canvas& canvas, int x, int y, int w, int h,
                         float rmsDbfs, float peakDbfs, float thresholdDbfs, VadState state) {
    // Meter spans -60.0 dBFS to 0.0 dBFS
    constexpr float kMinDb = -60.0f;
    constexpr float kMaxDb = 0.0f;
    constexpr float kRange = kMaxDb - kMinDb;

    // Draw background track
    canvas.fillRect(x, y, w, h, theme::kBarBg);
    canvas.drawRect(x, y, w, h, theme::kPanelBorder);

    // Calculate filled width
    float clampedRms = std::max(kMinDb, std::min(kMaxDb, rmsDbfs));
    int fillW = static_cast<int>(((clampedRms - kMinDb) / kRange) * (w - 2));

    if (fillW > 0) {
        uint16_t fillColor = theme::kSuccess;
        if (state == VadState::Speech) {
            fillColor = (rmsDbfs > -6.0f) ? theme::kDanger : ((rmsDbfs > -20.0f) ? theme::kWarning : theme::kSuccess);
        } else if (state == VadState::Hangover) {
            fillColor = theme::kWarning;
        } else {
            fillColor = 0x2289; // Muted green/gray while silent
        }
        canvas.fillRect(x + 1, y + 1, fillW, h - 2, fillColor);
    }

    // Peak tick mark
    float clampedPeak = std::max(kMinDb, std::min(kMaxDb, peakDbfs));
    int peakX = x + 1 + static_cast<int>(((clampedPeak - kMinDb) / kRange) * (w - 3));
    if (peakX >= x + 1 && peakX < x + w - 1) {
        canvas.drawFastVLine(peakX, y + 1, h - 2, theme::kText);
    }

    // Threshold marker line (vertical line in cyan)
    float clampedThresh = std::max(kMinDb, std::min(kMaxDb, thresholdDbfs));
    int threshX = x + 1 + static_cast<int>(((clampedThresh - kMinDb) / kRange) * (w - 3));
    if (threshX >= x + 1 && threshX < x + w - 1) {
        canvas.drawFastVLine(threshX, y - 1, h + 2, theme::kPrimary);
        canvas.drawFastVLine(threshX - 1, y - 1, h + 2, theme::kPrimary);
    }
}

void Painter::drawProgressBar(M5Canvas& canvas, int x, int y, int w, int h, float progress, uint16_t color) {
    canvas.fillRect(x, y, w, h, theme::kBarBg);
    canvas.drawRect(x, y, w, h, theme::kPanelBorder);

    progress = std::max(0.0f, std::min(1.0f, progress));
    int fillW = static_cast<int>(progress * (w - 2));
    if (fillW > 0) {
        canvas.fillRect(x + 1, y + 1, fillW, h - 2, color);
    }
}

void Painter::drawBadge(M5Canvas& canvas, int x, int y, const char* text, uint16_t bg, uint16_t fg) {
    const int textW = canvas.textWidth(text);
    const int badgeW = textW + 8;
    const int badgeH = 13;

    canvas.fillRoundRect(x, y, badgeW, badgeH, 2, bg);
    canvas.setTextColor(fg, bg);
    canvas.setTextSize(1);
    canvas.drawString(text, x + 4, y + 3);
}

}  // namespace voicerec
