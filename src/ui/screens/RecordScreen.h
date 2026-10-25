// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#pragma once

#include "../Screen.h"
#include <string>

namespace voicerec {

class RecordScreen : public Screen {
public:
    explicit RecordScreen(App& app);
    ~RecordScreen() override;

    void draw(M5Canvas& canvas) override;
    void onKey(const KeyEvent& event) override;
    void tick() override;

private:
    void updateStatusLed();

    std::string filePath_;
    bool stopped_ = false;
};

}  // namespace voicerec
