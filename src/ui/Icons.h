// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#pragma once

#include <M5GFX.h>

namespace voicerec::icons {

enum class Icon : uint8_t { Mic, Library, Levels, Settings, Help, Play, Pause };

// Draws a 40x40 vector icon centred at (cx, cy). `scale` 1.0 = 40 px, 0.5 = 20 px.
// `background` is used for cut-outs (the colour behind the icon).
void draw(M5Canvas& canvas, Icon icon, int cx, int cy, uint16_t colour, float scale = 1.0f,
          uint16_t background = 0);

}  // namespace voicerec::icons
