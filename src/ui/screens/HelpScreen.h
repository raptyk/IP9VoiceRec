// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#pragma once

#include "ui/Screen.h"

namespace voicerec {

// Cheat sheet of all keys, a few pages; , / turn the pages.
class HelpScreen : public ip9::HelpScreen {
public:
    // `firstPage` opens the page that belongs to the calling screen.
    explicit HelpScreen(App& app, int firstPage = kPageGeneral);

    static constexpr int kPageGeneral = 0;
    static constexpr int kPageRecording = 1;
    static constexpr int kPagePlayback = 2;
};

}  // namespace voicerec
