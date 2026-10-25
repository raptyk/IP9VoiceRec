// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#pragma once

#include <stdint.h>

namespace voicerec {
namespace theme {

constexpr int kScreenWidth = 240;
constexpr int kScreenHeight = 135;

constexpr int kHeaderHeight = 18;
constexpr int kFooterHeight = 16;
constexpr int kContentHeight = kScreenHeight - kHeaderHeight - kFooterHeight;

// 16-bit RGB565 Colors
constexpr uint16_t kBg = 0x0842;          // Very dark slate/navy
constexpr uint16_t kPanelBg = 0x18C3;     // Dark surface
constexpr uint16_t kPanelBorder = 0x2965; // Subtle border
constexpr uint16_t kText = 0xFFFF;        // Pure white
constexpr uint16_t kTextDim = 0x8410;     // Muted gray
constexpr uint16_t kPrimary = 0x05BF;     // Electric cyan
constexpr uint16_t kSuccess = 0x2E65;     // Bright green
constexpr uint16_t kWarning = 0xFDA0;     // Amber yellow
constexpr uint16_t kDanger = 0xF800;      // Bright red
constexpr uint16_t kBarBg = 0x18C3;       // VU meter background
constexpr uint16_t kSelection = 0x0370;   // Selected item background

}  // namespace theme
}  // namespace voicerec
