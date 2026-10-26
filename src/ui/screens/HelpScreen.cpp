// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#include "HelpScreen.h"

#include "app/App.h"
#include "ui/Strings.h"

namespace voicerec {

namespace {

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
    {"Mic check",
     {{"enter", "Calibrate"},
      {"s", "Speaker test"},
      {"v", "Voice act."},
      {"[ ]", "Sensitivity"},
      {"- =", "Mic gain"},
      {"tab", "This help"},
      {"esc", "Back"},
      {nullptr, nullptr},
      {nullptr, nullptr},
      {nullptr, nullptr}}},
};

constexpr int kPageCount = sizeof(kPages) / sizeof(kPages[0]);

}  // namespace

HelpScreen::HelpScreen(App& app, int firstPage) : ip9::HelpScreen(app, str::kHelpTitle, kPages, kPageCount, firstPage) {}

}  // namespace voicerec
