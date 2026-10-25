// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#pragma once

#include "ui/Screen.h"

namespace voicerec {

// Cheat sheet of all keys, a few pages; , / turn the pages.
class HelpScreen : public Screen {
public:
    // `firstPage` opens the page that belongs to the calling screen.
    HelpScreen(App& app, int firstPage = 0) : Screen(app), page_(firstPage) {}

    void onKey(const KeyEvent& event) override;
    void draw(M5Canvas& canvas, int top, int height) override;
    const char* footer() const override;

    static constexpr int kPageGeneral = 0;
    static constexpr int kPageRecording = 1;
    static constexpr int kPagePlayback = 2;

private:
    int page_ = 0;
};

}  // namespace voicerec
