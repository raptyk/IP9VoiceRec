// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#include "HomeScreen.h"
#include <esp_log.h>
#include "../../app/App.h"
#include "../../hw/Device.h"
#include "../../storage/SdStorage.h"
#include "../../ui/Painter.h"
#include "RecordScreen.h"
#include "PlayerScreen.h"
#include "CalibrationScreen.h"
#include "SettingsScreen.h"

namespace voicerec {

HomeScreen::HomeScreen(App& app)
    : Screen(app) {
    items_.push_back({"1. Record Audio", "Start dictaphone with VAS"});
    items_.push_back({"2. Library & Player", "Browse and listen to recordings"});
    items_.push_back({"3. Calibrate Noise", "Measure ambient room sound"});
    items_.push_back({"4. Settings", "Configure thresholds and audio"});
}

void HomeScreen::tick() {
    const uint32_t now = millis();
    if (lastRefreshAt_ == 0 || now - lastRefreshAt_ > 3000) {
        lastRefreshAt_ = now;
        auto list = SdStorage::instance().listRecordings();
        recordingsCount_ = list.size();
        sdFreeMb_ = SdStorage::instance().freeBytes() / (1024 * 1024);
    }
}

void HomeScreen::draw(M5Canvas& canvas) {
    canvas.fillScreen(theme::kBg);

    // Header
    const int bat = Device::instance().batteryLevel();
    const bool chg = Device::instance().isCharging();
    const bool sdOk = SdStorage::instance().isMounted();
    Painter::drawHeader(canvas, "CardputerVoiceRec", bat, chg, sdOk);

    // Menu items
    int y = theme::kHeaderHeight + 4;
    constexpr int kItemH = 19;
    constexpr int kItemW = theme::kScreenWidth - 12;

    for (size_t i = 0; i < items_.size(); ++i) {
        const bool selected = (static_cast<int>(i) == selectedIndex_);
        const uint16_t bg = selected ? theme::kSelection : theme::kPanelBg;
        const uint16_t border = selected ? theme::kPrimary : theme::kPanelBorder;

        Painter::drawCard(canvas, 6, y, kItemW, kItemH, bg, border);

        canvas.setTextSize(1);
        canvas.setTextColor(selected ? theme::kText : theme::kTextDim, bg);
        canvas.drawString(items_[i].label.c_str(), 14, y + 5);

        // Subtext on right
        canvas.setTextColor(theme::kTextDim, bg);
        const int subW = canvas.textWidth(items_[i].subtext.c_str());
        canvas.drawString(items_[i].subtext.c_str(), theme::kScreenWidth - 14 - subW, y + 5);

        y += kItemH + 2;
    }

    // Status Info Card
    y = theme::kScreenHeight - theme::kFooterHeight - 24;
    Painter::drawCard(canvas, 6, y, kItemW, 22, theme::kPanelBg, theme::kPanelBorder);

    char infoBuf[64];
    snprintf(infoBuf, sizeof(infoBuf), "Files: %u | VAS: %s (%.0f dB) | SD: %llu MB free",
             recordingsCount_,
             app_.settings().vasEnabled ? "ON" : "OFF",
             app_.settings().vasAttackThresholdDbfs,
             sdFreeMb_);
    canvas.setTextColor(theme::kTextDim, theme::kPanelBg);
    canvas.drawString(infoBuf, 12, y + 7);

    // Footer
    Painter::drawFooter(canvas, "[UP/DN] Move  [ENTER] Open  [R] Rec");
}

void HomeScreen::onKey(const KeyEvent& event) {
    if (event.code == ';' || event.code == ',' || event.code == 0x1E) { // Up
        if (selectedIndex_ > 0) {
            --selectedIndex_;
        } else {
            selectedIndex_ = items_.size() - 1;
        }
    } else if (event.code == '.' || event.code == '/' || event.code == 0x1F) { // Down
        if (selectedIndex_ + 1 < static_cast<int>(items_.size())) {
            ++selectedIndex_;
        } else {
            selectedIndex_ = 0;
        }
    } else if (event.code == keycode::kEnter || event.code == keycode::kSpace) {
        switch (selectedIndex_) {
            case 0: app_.push(std::make_unique<RecordScreen>(app_)); break;
            case 1: app_.push(std::make_unique<PlayerScreen>(app_)); break;
            case 2: app_.push(std::make_unique<CalibrationScreen>(app_)); break;
            case 3: app_.push(std::make_unique<SettingsScreen>(app_)); break;
        }
    } else if (event.code == '1') {
        app_.push(std::make_unique<RecordScreen>(app_));
    } else if (event.code == '2') {
        app_.push(std::make_unique<PlayerScreen>(app_));
    } else if (event.code == '3') {
        app_.push(std::make_unique<CalibrationScreen>(app_));
    } else if (event.code == '4') {
        app_.push(std::make_unique<SettingsScreen>(app_));
    } else if (event.code == 'r' || event.code == 'R') {
        app_.push(std::make_unique<RecordScreen>(app_));
    } else if (event.code == 'c' || event.code == 'C') {
        app_.push(std::make_unique<CalibrationScreen>(app_));
    }
}

}  // namespace voicerec
