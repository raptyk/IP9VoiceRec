// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#pragma once

#include <vector>

#include "storage/SdStorage.h"
#include "ui/Screen.h"

namespace voicerec {

// Recordings on the card, newest first. Lengths are read from the MP3 headers only for the
// rows on screen, so even a long list opens at once.
class LibraryScreen : public ListScreen {
public:
    explicit LibraryScreen(App& app);

    void onEnter() override;
    void onKey(const KeyEvent& event) override;
    const char* footer() const override;
    const char* chipLabel() const override { return str::kChipLibrary; }
    uint16_t chipColour() const override { return theme::kLibrary; }

protected:
    void drawEmpty(M5Canvas& canvas, int top, int height) override;

private:
    void reload();
    String duration(size_t index);
    void confirmDelete(size_t index);

    std::vector<RecordingFile> files_;
    std::vector<int32_t> durations_;  // ms, -1 = not read yet
};

}  // namespace voicerec
