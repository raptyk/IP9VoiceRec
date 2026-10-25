// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#pragma once

#include "../Screen.h"
#include <vector>
#include <string>

namespace voicerec {

class SettingsScreen : public Screen {
public:
    explicit SettingsScreen(App& app);

    void draw(M5Canvas& canvas) override;
    void onKey(const KeyEvent& event) override;

private:
    void adjustValue(int delta);

    enum SettingId {
        VasEnabled,
        AttackThreshold,
        HysteresisMargin,
        Hangover,
        Bitrate,
        Brightness,
        StatusLed,
        Volume,
        Count
    };

    int selectedIndex_ = 0;
};

}  // namespace voicerec
