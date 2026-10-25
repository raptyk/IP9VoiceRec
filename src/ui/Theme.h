// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#pragma once

#include <stdint.h>

namespace voicerec::theme {

// RGB565 palette (same family as LG Remote): dark background for battery and a dim room,
// one strong accent, and one colour per section so the active screen is obvious at a glance.
constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
    return static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

constexpr uint16_t kBackground = rgb(10, 10, 14);
constexpr uint16_t kSurface = rgb(28, 28, 36);
constexpr uint16_t kSurfaceHigh = rgb(48, 48, 60);
constexpr uint16_t kText = rgb(236, 236, 240);
constexpr uint16_t kTextMuted = rgb(140, 140, 155);
constexpr uint16_t kTextDisabled = rgb(80, 80, 92);
constexpr uint16_t kAccent = rgb(226, 36, 74);
constexpr uint16_t kOk = rgb(60, 200, 110);
constexpr uint16_t kWarn = rgb(245, 170, 40);
constexpr uint16_t kError = rgb(235, 70, 60);

// Sections: colour of the home card, the status bar chip and the screen band.
constexpr uint16_t kRecord = rgb(214, 40, 56);
constexpr uint16_t kRecordListening = rgb(110, 24, 34);  // record band while waiting for a voice
constexpr uint16_t kLibrary = rgb(30, 120, 230);
constexpr uint16_t kMicCheck = rgb(20, 150, 140);
constexpr uint16_t kPaused = rgb(200, 130, 20);

// Dark tints of the section colours: background of the full-screen views.
constexpr uint16_t kTintRecord = rgb(40, 8, 12);
constexpr uint16_t kTintLibrary = rgb(8, 22, 46);
constexpr uint16_t kTintMicCheck = rgb(5, 30, 28);

constexpr int kScreenWidth = 240;
constexpr int kScreenHeight = 135;
constexpr int kStatusBarHeight = 18;
constexpr int kFooterHeight = 16;
constexpr int kPadding = 6;

}  // namespace voicerec::theme
