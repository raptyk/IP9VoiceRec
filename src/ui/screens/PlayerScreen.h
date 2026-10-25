// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#pragma once

#include <vector>

#include "storage/SdStorage.h"
#include "ui/Screen.h"

namespace voicerec {

// Plays one recording; ; . step through the list it was opened from.
class PlayerScreen : public Screen {
public:
    PlayerScreen(App& app, std::vector<RecordingFile> files, int index);
    ~PlayerScreen() override;

    void onEnter() override;
    void onKey(const KeyEvent& event) override;
    void tick() override;
    void draw(M5Canvas& canvas, int top, int height) override;
    const char* footer() const override;
    const char* chipLabel() const override { return str::kChipPlayer; }
    uint16_t chipColour() const override { return theme::kLibrary; }

private:
    void playCurrent();
    void step(int delta);
    void changeVolume(int delta);
    void confirmDelete();

    void drawBand(M5Canvas& canvas, int top);
    void drawKeyRow(M5Canvas& canvas, int top, int height);

    std::vector<RecordingFile> files_;
    int index_;
    bool started_ = false;
    bool errorShown_ = false;
    bool volumeChanged_ = false;
};

}  // namespace voicerec
