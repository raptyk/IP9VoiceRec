// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#pragma once

#include "../Screen.h"
#include <vector>
#include <string>

namespace voicerec {

class HomeScreen : public Screen {
public:
    explicit HomeScreen(App& app);

    void draw(M5Canvas& canvas) override;
    void onKey(const KeyEvent& event) override;
    void tick() override;

private:
    struct MenuItem {
        std::string label;
        std::string subtext;
    };

    std::vector<MenuItem> items_;
    int selectedIndex_ = 0;
    uint32_t lastRefreshAt_ = 0;
    size_t recordingsCount_ = 0;
    uint64_t sdFreeMb_ = 0;
};

}  // namespace voicerec
