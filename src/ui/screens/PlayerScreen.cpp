// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#include "PlayerScreen.h"

#include "app/App.h"
#include "audio/Player.h"
#include "audio/Recorder.h"
#include "ui/Icons.h"
#include "ui/Screen.h"
#include "ui/Strings.h"
#include "ui/Theme.h"
#include "ui/screens/HelpScreen.h"

namespace voicerec {

namespace {
constexpr int kBandHeight = 22;
constexpr int kKeyRowHeight = 18;
constexpr int32_t kSeekMs = 10000;

String withoutExtension(const String& name) {
    const int dot = name.lastIndexOf('.');
    return dot > 0 ? name.substring(0, dot) : name;
}
}  // namespace

PlayerScreen::PlayerScreen(App& app, std::vector<RecordingFile> files, int index)
    : Screen(app), files_(std::move(files)), index_(index) {}

PlayerScreen::~PlayerScreen() {
    Player::instance().stop();
    if (volumeChanged_) {
        app_.saveSettings();
    }
}

void PlayerScreen::onEnter() {
    if (!started_) {
        started_ = true;
        playCurrent();
    }
}

void PlayerScreen::playCurrent() {
    if (files_.empty()) {
        app_.pop();
        return;
    }
    index_ = constrain(index_, 0, static_cast<int>(files_.size()) - 1);
    errorShown_ = false;
    Recorder::instance().stopMonitor();  // the speaker needs the pins the microphone uses
    Player::instance().setVolume(app_.settings().speakerVolume());
    Player::instance().play(files_[index_].path);
}

void PlayerScreen::step(int delta) {
    const int next = index_ + delta;
    if (next < 0 || next >= static_cast<int>(files_.size())) {
        return;
    }
    index_ = next;
    playCurrent();
}

void PlayerScreen::changeVolume(int delta) {
    Settings& settings = app_.settings();
    settings.volume = constrain(settings.volume + delta, 0, 10);
    volumeChanged_ = true;
    Player::instance().setVolume(settings.speakerVolume());
    char text[24];
    snprintf(text, sizeof(text), str::kVolume, settings.volume);
    app_.toast(text);
}

void PlayerScreen::confirmDelete() {
    const RecordingFile file = files_[index_];
    Player::instance().stop();
    char question[64];
    snprintf(question, sizeof(question), str::kConfirmDelete, withoutExtension(file.name).c_str());
    // This screen stays on the stack below the question, so `this` is still there afterwards.
    app_.push(std::make_unique<ConfirmScreen>(app_, question, str::kConfirmDeleteDetail, [this, file]() {
        if (!SdStorage::instance().remove(file.path)) {
            app_.warn(str::kDeleteFailed);
            return;
        }
        app_.toast(str::kDeleted);
        files_.erase(files_.begin() + index_);
        if (files_.empty()) {
            app_.pop();
            return;
        }
        playCurrent();
    }));
}

void PlayerScreen::onKey(const KeyEvent& event) {
    Player& player = Player::instance();
    switch (event.code) {
        case ' ':
        case keycode::kEnter:
        case keycode::kButtonG0:
            if (!event.repeat) {
                if (player.active()) {
                    player.togglePause();
                } else {
                    playCurrent();  // finished or stopped: from the start
                }
            }
            break;
        case keycode::kLeft:
        case keycode::kRight:
            player.seekBy(event.code == keycode::kRight ? kSeekMs : -kSeekMs);
            break;
        case keycode::kUp:
        case keycode::kDown:
            if (!event.repeat) {
                step(event.code == keycode::kDown ? 1 : -1);
            }
            break;
        case '-':
        case '=':
            changeVolume(event.code == '=' ? 1 : -1);
            break;
        case 'd':
            if (!event.repeat) {
                confirmDelete();
            }
            break;
        case keycode::kTab:
            if (!event.repeat) {
                app_.push(std::make_unique<HelpScreen>(app_, HelpScreen::kPagePlayback));
            }
            break;
        case keycode::kEsc:
        case keycode::kBackspace:
            if (!event.repeat) {
                app_.pop();
            }
            break;
        default:
            break;
    }
}

void PlayerScreen::tick() {
    if (Player::instance().state() == PlayerState::Error && !errorShown_) {
        errorShown_ = true;
        app_.warn(str::kPlayError);
    }
    app_.invalidate();  // position and progress
}

const char* PlayerScreen::footer() const {
    return str::kFooterPlayer;
}

void PlayerScreen::draw(M5Canvas& canvas, int top, int height) {
    using namespace theme;
    canvas.fillRect(0, top, kScreenWidth, height, kTintLibrary);
    if (files_.empty()) {
        return;
    }
    drawBand(canvas, top);

    const Player& player = Player::instance();
    const int middleTop = top + kBandHeight;
    const int middleHeight = height - kBandHeight - kKeyRowHeight;

    // Name, large position / length, progress bar.
    canvas.setTextSize(1);
    canvas.setFont(&fonts::FreeSansBold9pt7b);
    canvas.setTextColor(kText);
    canvas.setTextDatum(top_left);
    painter::drawClippedText(canvas, withoutExtension(files_[index_].name), kPadding + 2, middleTop + 4,
                             kScreenWidth - 2 * kPadding - 50);

    char counter[16];
    snprintf(counter, sizeof(counter), "%d / %u", index_ + 1, static_cast<unsigned>(files_.size()));
    canvas.setFont(&fonts::Font0);
    canvas.setTextColor(kTextMuted);
    canvas.setTextDatum(top_right);
    canvas.drawString(counter, kScreenWidth - kPadding, middleTop + 8);

    const uint32_t duration = player.durationMs();
    const uint32_t position = min(player.positionMs(), duration);
    const String now = painter::formatDuration(position);
    const String total = String("/ ") + painter::formatDuration(duration);
    canvas.setFont(&fonts::FreeSansBold12pt7b);
    canvas.setTextColor(kText);
    canvas.setTextDatum(middle_left);
    const int timeY = middleTop + 32;
    canvas.drawString(now, kPadding + 2, timeY);
    const int nowWidth = canvas.textWidth(now);
    canvas.setFont(&fonts::Font2);
    canvas.setTextColor(kTextMuted);
    canvas.drawString(total, kPadding + 2 + nowWidth + 6, timeY + 2);

    const float progress = duration > 0 ? static_cast<float>(position) / duration : 0.0f;
    painter::drawProgress(canvas, kPadding + 4, middleTop + middleHeight - 6, kScreenWidth - 2 * kPadding - 8,
                          progress, kLibrary);

    drawKeyRow(canvas, top + height - kKeyRowHeight, kKeyRowHeight);
}

void PlayerScreen::drawBand(M5Canvas& canvas, int top) {
    using namespace theme;
    const PlayerState state = Player::instance().state();
    const char* title = str::kBandPlaying;
    icons::Icon icon = icons::Icon::Play;
    switch (state) {
        case PlayerState::Paused:
            title = str::kBandPaused;
            icon = icons::Icon::Pause;
            break;
        case PlayerState::Finished:
        case PlayerState::Stopped:
            title = str::kBandFinished;
            icon = icons::Icon::Pause;
            break;
        case PlayerState::Error:
            title = str::kPlayError;
            break;
        case PlayerState::Playing:
            break;
    }
    const uint16_t colour = state == PlayerState::Error ? kError : kLibrary;
    canvas.fillRect(0, top, kScreenWidth, kBandHeight, colour);
    const int centerY = top + kBandHeight / 2;
    icons::draw(canvas, icon, 16, centerY, kText, 0.4f, colour);
    canvas.setFont(&fonts::FreeSansBold9pt7b);
    canvas.setTextSize(1);
    canvas.setTextColor(kText);
    canvas.setTextDatum(middle_left);
    painter::drawClippedText(canvas, title, 32, centerY + 1, 150);

    char volume[16];
    snprintf(volume, sizeof(volume), str::kVolume, app_.settings().volume);
    canvas.setFont(&fonts::Font0);
    canvas.setTextDatum(middle_right);
    canvas.drawString(volume, kScreenWidth - kPadding, centerY);
}

void PlayerScreen::drawKeyRow(M5Canvas& canvas, int top, int height) {
    struct Hint {
        const char* key;
        const char* label;
    };
    const bool playing = Player::instance().state() == PlayerState::Playing;
    const Hint hints[] = {
        {str::kKeySpace, playing ? str::kLabelPause : str::kLabelPlay},
        {str::kKeySeek, str::kLabelSeek},
        {str::kKeyD, str::kLabelDelete},
    };
    constexpr int kCount = sizeof(hints) / sizeof(hints[0]);
    int total = 0;
    for (const Hint& hint : hints) {
        total += painter::measureKeyHint(canvas, hint.key, hint.label);
    }
    const int gap = (theme::kScreenWidth - total) / (kCount + 1);
    int x = gap;
    const int centerY = top + height / 2;
    for (const Hint& hint : hints) {
        x += painter::drawKeyHint(canvas, x, centerY, hint.key, hint.label) + gap;
    }
}

}  // namespace voicerec
