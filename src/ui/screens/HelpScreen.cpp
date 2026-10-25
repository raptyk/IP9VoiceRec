// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#include "HelpScreen.h"

#include "app/App.h"
#include "ui/Painter.h"
#include "ui/Strings.h"
#include "ui/Theme.h"

namespace voicerec {

namespace {

struct HelpEntry {
    const char* keys;
    const char* action;
};

struct HelpPage {
    const char* title;
    HelpEntry entries[10];
};

// Two columns x five rows per page.
constexpr HelpPage kPages[] = {
    {"General",
     {{", /", "Pick card"},
      {"; .", "Up / down"},
      {"enter", "Open"},
      {"esc del", "Back"},
      {"r", "Record now"},
      {"1-5", "Open card"},
      {"G0", "Open / stop"},
      {"tab", "This help"},
      {nullptr, nullptr},
      {nullptr, nullptr}}},
    {"Recording",
     {{"enter", "Stop, save"},
      {"G0", "Stop, save"},
      {"space", "Pause"},
      {"v", "Voice act."},
      {"esc", "Stop, save"},
      {"[ ]", "Sensitivity"},
      {"- =", "Mic gain"},
      {"tab", "This help"},
      {nullptr, nullptr},
      {nullptr, nullptr}}},
    {"Playback",
     {{"space", "Play/pause"},
      {", /", "-/+ 10 s"},
      {"; .", "Prev/next"},
      {"- =", "Volume"},
      {"d", "Delete"},
      {"esc", "Back"},
      {nullptr, nullptr},
      {nullptr, nullptr},
      {nullptr, nullptr},
      {nullptr, nullptr}}},
};

constexpr int kPageCount = sizeof(kPages) / sizeof(kPages[0]);

}  // namespace

void HelpScreen::onKey(const KeyEvent& event) {
    switch (event.code) {
        case keycode::kLeft:
            page_ = (page_ + kPageCount - 1) % kPageCount;
            break;
        case keycode::kRight:
            if (event.shift) {
                app_.pop();  // "?" again closes the help
                return;
            }
            page_ = (page_ + 1) % kPageCount;
            break;
        case keycode::kEsc:
        case keycode::kBackspace:
        case keycode::kEnter:
        case keycode::kTab:
            if (!event.repeat) {
                app_.pop();
            }
            return;
        default:
            break;
    }
}

const char* HelpScreen::footer() const {
    return str::kHelpFooter;
}

void HelpScreen::draw(M5Canvas& canvas, int top, int height) {
    using namespace theme;
    page_ = constrain(page_, 0, kPageCount - 1);
    char title[40];
    snprintf(title, sizeof(title), "%s  %d/%d  %s", str::kHelpTitle, page_ + 1, kPageCount, kPages[page_].title);
    const int contentTop = painter::drawTitle(canvas, top, title);
    const int rowHeight = (top + height - contentTop - 2) / 5;
    const int columnWidth = kScreenWidth / 2;

    canvas.setFont(&fonts::Font0);
    canvas.setTextSize(1);
    for (int i = 0; i < 10; ++i) {
        const HelpEntry& entry = kPages[page_].entries[i];
        if (entry.keys == nullptr) {
            continue;
        }
        const int column = i / 5;
        const int row = i % 5;
        const int x = column * columnWidth + kPadding;
        const int y = contentTop + row * rowHeight + rowHeight / 2;

        // Key cap.
        const int capWidth = canvas.textWidth(entry.keys) + 8;
        canvas.fillRoundRect(x, y - 7, capWidth, 14, 3, kSurfaceHigh);
        canvas.setTextColor(kText);
        canvas.setTextDatum(middle_center);
        canvas.drawString(entry.keys, x + capWidth / 2, y);

        canvas.setTextColor(kTextMuted);
        canvas.setTextDatum(middle_left);
        canvas.drawString(entry.action, x + capWidth + 5, y);
    }
}

}  // namespace voicerec
