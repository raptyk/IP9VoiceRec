// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#pragma once

#include "../Screen.h"
#include <vector>
#include <string>
#include "../../storage/SdStorage.h"

namespace voicerec {

class PlayerScreen : public Screen {
public:
    explicit PlayerScreen(App& app);
    ~PlayerScreen() override;

    void draw(M5Canvas& canvas) override;
    void onKey(const KeyEvent& event) override;
    void tick() override;

private:
    void refreshList();

    std::vector<RecordingFile> files_;
    int selectedIndex_ = 0;
    int scrollOffset_ = 0;
    bool confirmDelete_ = false;
};

}  // namespace voicerec
