// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#include "PlayerScreen.h"
#include <esp_log.h>
#include <algorithm>
#include "../../app/App.h"
#include "../../hw/Device.h"
#include "../../hw/StatusLed.h"
#include "../../audio/AudioPlayer.h"
#include "../../ui/Painter.h"

namespace voicerec {

PlayerScreen::PlayerScreen(App& app)
    : Screen(app) {
    AudioPlayer::instance().begin();
    AudioPlayer::instance().setVolume(app_.settings().speakerVolume);
    refreshList();
}

PlayerScreen::~PlayerScreen() {
    AudioPlayer::instance().stop();
    StatusLed::instance().setState(LedState::Idle);
}

void PlayerScreen::refreshList() {
    files_ = SdStorage::instance().listRecordings();
    if (selectedIndex_ >= static_cast<int>(files_.size())) {
        selectedIndex_ = std::max(0, static_cast<int>(files_.size()) - 1);
    }
}

void PlayerScreen::tick() {
    auto& player = AudioPlayer::instance();
    if (player.isPlaying()) {
        StatusLed::instance().setState(LedState::Playback);
    } else {
        StatusLed::instance().setState(LedState::Idle);
    }
}

void PlayerScreen::draw(M5Canvas& canvas) {
    canvas.fillScreen(theme::kBg);

    const int bat = Device::instance().batteryLevel();
    const bool chg = Device::instance().isCharging();
    const bool sdOk = SdStorage::instance().isMounted();
    Painter::drawHeader(canvas, "Library & Player", bat, chg, sdOk);

    auto& player = AudioPlayer::instance();

    if (files_.empty()) {
        Painter::drawCard(canvas, 10, 35, theme::kScreenWidth - 20, 60);
        canvas.setTextColor(theme::kTextDim, theme::kPanelBg);
        canvas.setTextSize(1);
        canvas.drawString("No recordings found on SD.", 35, 52);
        canvas.drawString("Press [R] to record audio.", 42, 68);
        Painter::drawFooter(canvas, "[ESC] Return  [R] Record");
        return;
    }

    // Scrollable File List (max 3 items shown)
    constexpr int kVisibleCount = 3;
    constexpr int kItemH = 22;
    constexpr int kItemW = theme::kScreenWidth - 12;
    const int startY = 22;

    if (selectedIndex_ < scrollOffset_) {
        scrollOffset_ = selectedIndex_;
    } else if (selectedIndex_ >= scrollOffset_ + kVisibleCount) {
        scrollOffset_ = selectedIndex_ - kVisibleCount + 1;
    }

    for (int i = 0; i < kVisibleCount; ++i) {
        const int itemIdx = scrollOffset_ + i;
        if (itemIdx >= static_cast<int>(files_.size())) break;

        const auto& file = files_[itemIdx];
        const bool selected = (itemIdx == selectedIndex_);
        const bool isPlayingThis = (player.currentPath() == file.path && (player.isPlaying() || player.isPaused()));

        const uint16_t bg = selected ? theme::kSelection : theme::kPanelBg;
        const uint16_t border = selected ? theme::kPrimary : theme::kPanelBorder;
        const int y = startY + i * (kItemH + 2);

        Painter::drawCard(canvas, 6, y, kItemW, kItemH, bg, border);

        // Name
        canvas.setTextSize(1);
        canvas.setTextColor(isPlayingThis ? theme::kSuccess : (selected ? theme::kText : theme::kTextDim), bg);
        canvas.drawString(file.name.c_str(), 12, y + 6);

        // Size & Duration
        char metaBuf[32];
        const uint32_t durMin = file.estimatedDurationSec / 60;
        const uint32_t durSec = file.estimatedDurationSec % 60;
        snprintf(metaBuf, sizeof(metaBuf), "%02u:%02u | %u KB", durMin, durSec, static_cast<unsigned>(file.sizeBytes / 1024));

        canvas.setTextColor(theme::kTextDim, bg);
        const int metaW = canvas.textWidth(metaBuf);
        canvas.drawString(metaBuf, theme::kScreenWidth - 12 - metaW, y + 6);
    }

    // Playback Controller Card at bottom
    const int playerY = startY + kVisibleCount * (kItemH + 2) + 2;
    Painter::drawCard(canvas, 6, playerY, kItemW, 25, theme::kPanelBg, theme::kPanelBorder);

    if (player.isPlaying() || player.isPaused()) {
        const uint32_t cur = player.currentPositionSec();
        const uint32_t tot = player.totalDurationSec();
        const float prog = (tot > 0) ? (static_cast<float>(cur) / tot) : 0.0f;

        // Progress Bar
        Painter::drawProgressBar(canvas, 12, playerY + 14, kItemW - 70, 5, prog, theme::kPrimary);

        // Status text
        char playBuf[32];
        snprintf(playBuf, sizeof(playBuf), "%s %02u:%02u / %02u:%02u",
                 player.isPaused() ? "PAUSE" : "PLAY",
                 cur / 60, cur % 60, tot / 60, tot % 60);
        canvas.setTextColor(theme::kText, theme::kPanelBg);
        canvas.drawString(playBuf, 12, playerY + 3);

        // Volume
        char volBuf[16];
        snprintf(volBuf, sizeof(volBuf), "Vol: %d%%", (app_.settings().speakerVolume * 100) / 255);
        canvas.setTextColor(theme::kTextDim, theme::kPanelBg);
        const int volW = canvas.textWidth(volBuf);
        canvas.drawString(volBuf, theme::kScreenWidth - 12 - volW, playerY + 8);
    } else {
        canvas.setTextColor(theme::kTextDim, theme::kPanelBg);
        if (confirmDelete_) {
            canvas.setTextColor(theme::kDanger, theme::kPanelBg);
            canvas.drawString("Delete file? [Y] Confirm  [N] Cancel", 12, playerY + 8);
        } else {
            canvas.drawString("Select track and press [ENTER] to play", 12, playerY + 8);
        }
    }

    Painter::drawFooter(canvas, confirmDelete_ ? "[Y] Yes  [N] No" : "[ENTER] Play  [D] Del  [+/-] Vol  [ESC] Back");
}

void PlayerScreen::onKey(const KeyEvent& event) {
    auto& player = AudioPlayer::instance();

    if (confirmDelete_) {
        if (event.code == 'y' || event.code == 'Y' || event.code == keycode::kEnter) {
            if (selectedIndex_ >= 0 && selectedIndex_ < static_cast<int>(files_.size())) {
                if (player.currentPath() == files_[selectedIndex_].path) {
                    player.stop();
                }
                SdStorage::instance().deleteRecording(files_[selectedIndex_].path);
                app_.toast("Deleted");
                refreshList();
            }
            confirmDelete_ = false;
        } else if (event.code == 'n' || event.code == 'N' || event.code == keycode::kOpt || event.code == 0x1B) {
            confirmDelete_ = false;
        }
        return;
    }

    if (event.code == ';' || event.code == ',' || event.code == 0x1E) { // Up
        if (selectedIndex_ > 0) {
            --selectedIndex_;
        }
    } else if (event.code == '.' || event.code == '/' || event.code == 0x1F) { // Down
        if (selectedIndex_ + 1 < static_cast<int>(files_.size())) {
            ++selectedIndex_;
        }
    } else if (event.code == keycode::kEnter || event.code == keycode::kSpace || event.code == 'p' || event.code == 'P') {
        if (selectedIndex_ >= 0 && selectedIndex_ < static_cast<int>(files_.size())) {
            const std::string& targetPath = files_[selectedIndex_].path;
            if (player.currentPath() == targetPath) {
                if (player.isPlaying()) {
                    player.pause();
                } else if (player.isPaused()) {
                    player.resume();
                } else {
                    player.play(targetPath);
                }
            } else {
                player.play(targetPath);
            }
        }
    } else if (event.code == 'd' || event.code == 'D' || event.code == keycode::kBackspace) {
        if (!files_.empty()) {
            confirmDelete_ = true;
        }
    } else if (event.code == '+' || event.code == '=') {
        int v = std::min(255, static_cast<int>(app_.settings().speakerVolume) + 20);
        app_.settings().speakerVolume = static_cast<uint8_t>(v);
        player.setVolume(v);
    } else if (event.code == '-' || event.code == '_') {
        int v = std::max(20, static_cast<int>(app_.settings().speakerVolume) - 20);
        app_.settings().speakerVolume = static_cast<uint8_t>(v);
        player.setVolume(v);
    } else if (event.code == keycode::kOpt || event.code == 0x1B) {
        player.stop();
        app_.pop();
    }
}

}  // namespace voicerec
