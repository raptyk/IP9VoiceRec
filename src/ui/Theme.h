// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#pragma once

#include "ip9/ui/Theme.h"

namespace voicerec::theme {

// The shared IP9 palette (ip9::theme) plus the colours of this app's sections.
using namespace ip9::theme;

constexpr uint16_t kAccent = kBrandAccent;

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

}  // namespace voicerec::theme
