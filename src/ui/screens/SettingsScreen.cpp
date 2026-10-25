// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#include "SettingsScreen.h"
#include <esp_log.h>
#include <algorithm>
#include "../../app/App.h"
#include "../../hw/Device.h"
#include "../../hw/StatusLed.h"
#include "../../storage/SdStorage.h"
#include "../../audio/AudioPlayer.h"
#include "../../ui/Painter.h"

namespace voicerec {

SettingsScreen::SettingsScreen(App& app)
    : Screen(app) {}

void SettingsScreen::adjustValue(int delta) {
    auto& s = app_.settings();

    switch (selectedIndex_) {
        case SettingId::VasEnabled:
            s.vasEnabled = !s.vasEnabled;
            break;

        case SettingId::AttackThreshold:
            s.vasAttackThresholdDbfs = std::max(-55.0f, std::min(-20.0f, s.vasAttackThresholdDbfs + delta));
            break;

        case SettingId::HysteresisMargin:
            s.vasHysteresisMarginDb = std::max(2.0f, std::min(10.0f, s.vasHysteresisMarginDb + delta));
            break;

        case SettingId::Hangover: {
            int h = static_cast<int>(s.vasHangoverMs) + delta * 250;
            s.vasHangoverMs = std::max(500, std::min(3500, h));
            break;
        }

        case SettingId::Bitrate:
            if (s.bitrateKbps == 32) s.bitrateKbps = (delta > 0) ? 48 : 32;
            else if (s.bitrateKbps == 48) s.bitrateKbps = (delta > 0) ? 64 : 32;
            else if (s.bitrateKbps == 64) s.bitrateKbps = (delta > 0) ? 64 : 48;
            break;

        case SettingId::Brightness: {
            int b = static_cast<int>(s.screenBrightness) + delta * 25;
            s.screenBrightness = std::max(50, std::min(255, b));
            Device::instance().setBrightness(s.screenBrightness);
            break;
        }

        case SettingId::StatusLed:
            s.statusLedEnabled = !s.statusLedEnabled;
            StatusLed::instance().setEnabled(s.statusLedEnabled);
            break;

        case SettingId::Volume: {
            int v = static_cast<int>(s.speakerVolume) + delta * 15;
            s.speakerVolume = std::max(50, std::min(255, v));
            AudioPlayer::instance().setVolume(s.speakerVolume);
            break;
        }

        default:
            break;
    }
}

void SettingsScreen::draw(M5Canvas& canvas) {
    canvas.fillScreen(theme::kBg);

    const int bat = Device::instance().batteryLevel();
    const bool chg = Device::instance().isCharging();
    const bool sdOk = SdStorage::instance().isMounted();
    Painter::drawHeader(canvas, "Settings", bat, chg, sdOk);

    auto& s = app_.settings();

    struct SettingDisplay {
        const char* label;
        char value[32];
    };

    SettingDisplay list[SettingId::Count];
    snprintf(list[SettingId::VasEnabled].value, 32, "%s", s.vasEnabled ? "ON" : "OFF");
    list[SettingId::VasEnabled].label = "VAS System:";

    snprintf(list[SettingId::AttackThreshold].value, 32, "%.0f dBFS", s.vasAttackThresholdDbfs);
    list[SettingId::AttackThreshold].label = "Sensitivity:";

    snprintf(list[SettingId::HysteresisMargin].value, 32, "%.0f dB", s.vasHysteresisMarginDb);
    list[SettingId::HysteresisMargin].label = "Hysteresis:";

    snprintf(list[SettingId::Hangover].value, 32, "%.1f s", s.vasHangoverMs / 1000.0f);
    list[SettingId::Hangover].label = "Hangover:";

    snprintf(list[SettingId::Bitrate].value, 32, "%d kbps", s.bitrateKbps);
    list[SettingId::Bitrate].label = "MP3 Bitrate:";

    snprintf(list[SettingId::Brightness].value, 32, "%d", s.screenBrightness);
    list[SettingId::Brightness].label = "Brightness:";

    snprintf(list[SettingId::StatusLed].value, 32, "%s", s.statusLedEnabled ? "ON" : "OFF");
    list[SettingId::StatusLed].label = "Status LED:";

    snprintf(list[SettingId::Volume].value, 32, "%d%%", (s.speakerVolume * 100) / 255);
    list[SettingId::Volume].label = "Volume:";

    // Scrollable list: 4 items visible
    constexpr int kVisible = 4;
    constexpr int kItemH = 18;
    constexpr int kItemW = theme::kScreenWidth - 12;

    int offset = 0;
    if (selectedIndex_ >= kVisible) {
        offset = selectedIndex_ - kVisible + 1;
    }

    int y = 22;
    for (int i = 0; i < kVisible; ++i) {
        const int idx = offset + i;
        if (idx >= SettingId::Count) break;

        const bool selected = (idx == selectedIndex_);
        const uint16_t bg = selected ? theme::kSelection : theme::kPanelBg;
        const uint16_t border = selected ? theme::kPrimary : theme::kPanelBorder;

        Painter::drawCard(canvas, 6, y, kItemW, kItemH, bg, border);

        canvas.setTextSize(1);
        canvas.setTextColor(selected ? theme::kText : theme::kTextDim, bg);
        canvas.drawString(list[idx].label, 12, y + 5);

        // Value on right with arrow hints
        char valWithArrows[48];
        if (selected) {
            snprintf(valWithArrows, sizeof(valWithArrows), "< %s >", list[idx].value);
            canvas.setTextColor(theme::kPrimary, bg);
        } else {
            snprintf(valWithArrows, sizeof(valWithArrows), "%s", list[idx].value);
            canvas.setTextColor(theme::kTextDim, bg);
        }

        const int valW = canvas.textWidth(valWithArrows);
        canvas.drawString(valWithArrows, theme::kScreenWidth - 14 - valW, y + 5);

        y += kItemH + 2;
    }

    Painter::drawFooter(canvas, "[UP/DN] Select  [<- / ->] Edit  [S] Save  [ESC]");
}

void SettingsScreen::onKey(const KeyEvent& event) {
    if (event.code == ';' || event.code == ',' || event.code == 0x1E) { // Up
        if (selectedIndex_ > 0) {
            --selectedIndex_;
        }
    } else if (event.code == '.' || event.code == '/' || event.code == 0x1F) { // Down
        if (selectedIndex_ + 1 < SettingId::Count) {
            ++selectedIndex_;
        }
    } else if (event.code == '[' || event.code == '-' || event.code == '<') { // Left / Decrement
        adjustValue(-1);
    } else if (event.code == ']' || event.code == '=' || event.code == '+' || event.code == '>') { // Right / Increment
        adjustValue(1);
    } else if (event.code == 's' || event.code == 'S' || event.code == keycode::kEnter) {
        app_.settings().save();
        app_.toast("Settings Saved");
        app_.pop();
    } else if (event.code == keycode::kOpt || event.code == 0x1B) {
        app_.pop();
    }
}

}  // namespace voicerec
