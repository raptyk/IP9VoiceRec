// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#include "HomeScreen.h"

#include "app/App.h"
#include "storage/SdStorage.h"
#include "ui/Icons.h"
#include "ui/Painter.h"
#include "ui/Strings.h"
#include "ui/Theme.h"
#include "ui/screens/HelpScreen.h"
#include "ui/screens/LibraryScreen.h"
#include "ui/screens/MicCheckScreen.h"
#include "ui/screens/SettingsScreen.h"

namespace voicerec {

namespace {

enum class CardKind : uint8_t { Record, Library, MicCheck, Settings, Help };

struct Card {
    icons::Icon icon;
    const char* title;
    CardKind kind;
    uint16_t colour;
};

constexpr Card kCards[] = {
    {icons::Icon::Mic, str::kCardRecordTitle, CardKind::Record, theme::kRecord},
    {icons::Icon::Library, str::kCardLibraryTitle, CardKind::Library, theme::kLibrary},
    {icons::Icon::Levels, str::kCardMicCheckTitle, CardKind::MicCheck, theme::kMicCheck},
    {icons::Icon::Settings, str::kCardSettingsTitle, CardKind::Settings, theme::kSurface},
    {icons::Icon::Help, str::kCardHelpTitle, CardKind::Help, theme::kSurface},
};
constexpr int kCardCount = sizeof(kCards) / sizeof(kCards[0]);
constexpr uint32_t kSlideMs = 140;

struct CardStatus {
    String text;
    uint16_t colour;
};

// "190 h" / "45 min" of recording that fits on the card.
String recordingTimeLeft(uint64_t freeBytes, uint8_t kbps) {
    const uint64_t seconds = freeBytes / (kbps * 125ULL);
    char text[16];
    if (seconds >= 3600) {
        snprintf(text, sizeof(text), str::kHours, static_cast<unsigned>(seconds / 3600));
    } else {
        snprintf(text, sizeof(text), str::kMinutes, static_cast<unsigned>(seconds / 60));
    }
    return text;
}

}  // namespace

void HomeScreen::move(int delta) {
    selected_ = (selected_ + delta + kCardCount) % kCardCount;
    slideDirection_ = delta;
    slideStartedAt_ = millis();
    app_.invalidate();
}

void HomeScreen::onKey(const KeyEvent& event) {
    switch (event.code) {
        case keycode::kLeft:
        case keycode::kUp:
            move(-1);
            return;
        case keycode::kRight:
            if (event.shift) {
                app_.push(std::make_unique<HelpScreen>(app_));  // "?"
                return;
            }
            move(1);
            return;
        case keycode::kDown:
            move(1);
            return;
        case keycode::kTab:
            if (!event.repeat) {
                app_.push(std::make_unique<HelpScreen>(app_));
            }
            return;
        case keycode::kEnter:
        case keycode::kButtonG0:
            if (!event.repeat) {
                open();
            }
            return;
        case 'r':
            if (!event.repeat) {
                app_.startRecording();
            }
            return;
        default:
            if (!event.repeat && event.code >= '1' && event.code < '1' + kCardCount) {
                selected_ = event.code - '1';
                open();
            }
            return;
    }
}

void HomeScreen::open() {
    switch (kCards[selected_].kind) {
        case CardKind::Record:
            app_.startRecording();
            break;
        case CardKind::Library:
            app_.push(std::make_unique<LibraryScreen>(app_));
            break;
        case CardKind::MicCheck:
            app_.push(std::make_unique<MicCheckScreen>(app_));
            break;
        case CardKind::Settings:
            app_.push(makeSettingsScreen(app_));
            break;
        case CardKind::Help:
            app_.push(std::make_unique<HelpScreen>(app_));
            break;
    }
}

const char* HomeScreen::footer() const {
    return str::kFooterHome;
}

void HomeScreen::draw(M5Canvas& canvas, int top, int height) {
    using namespace theme;
    const Card& card = kCards[selected_];
    const int centerX = kScreenWidth / 2;
    const Settings& settings = app_.settings();
    SdStorage& storage = SdStorage::instance();

    // Short slide-in of the selected icon.
    int offset = 0;
    const uint32_t age = millis() - slideStartedAt_;
    if (slideDirection_ != 0 && age < kSlideMs) {
        offset = slideDirection_ * static_cast<int>(30 * (kSlideMs - age) / kSlideMs);
        app_.invalidate();
    }

    // Neighbour previews and chevrons hint that the list scrolls sideways.
    const int iconY = top + 24;
    const Card& previous = kCards[(selected_ + kCardCount - 1) % kCardCount];
    const Card& next = kCards[(selected_ + 1) % kCardCount];
    icons::draw(canvas, previous.icon, 34, iconY, kSurfaceHigh, 0.55f, kBackground);
    icons::draw(canvas, next.icon, kScreenWidth - 34, iconY, kSurfaceHigh, 0.55f, kBackground);
    canvas.fillTriangle(6, iconY, 12, iconY - 6, 12, iconY + 6, kTextMuted);
    canvas.fillTriangle(kScreenWidth - 6, iconY, kScreenWidth - 12, iconY - 6, kScreenWidth - 12, iconY + 6,
                        kTextMuted);

    canvas.fillRoundRect(centerX - 26 + offset, iconY - 22, 52, 44, 10, card.colour);
    icons::draw(canvas, card.icon, centerX + offset, iconY, kText, 0.9f, card.colour);

    canvas.setTextSize(1);
    canvas.setFont(&fonts::FreeSansBold9pt7b);
    canvas.setTextColor(kText);
    canvas.setTextDatum(top_center);
    painter::drawClippedText(canvas, card.title, centerX, top + 49, kScreenWidth - 2 * kPadding);

    // Status line: is this ready, or what is missing?
    CardStatus status{String(), kTextMuted};
    char buffer[64];
    const char* text = nullptr;
    switch (card.kind) {
        case CardKind::Record:
            text = settings.voiceActivation ? str::kCardRecordText : str::kCardRecordTextAlways;
            if (!storage.mounted()) {
                status = {str::kInsertCard, kWarn};
            } else if (storage.freeBytes() < (1 << 20)) {
                status = {str::kCardFull, kError};
            } else {
                snprintf(buffer, sizeof(buffer), str::kReadySpace,
                         recordingTimeLeft(storage.freeBytes(), settings.bitrateKbps).c_str());
                status = {buffer, kOk};
            }
            break;
        case CardKind::Library: {
            text = str::kCardLibraryText;
            const size_t count = storage.recordingCount();
            if (!storage.mounted()) {
                status = {str::kInsertCard, kWarn};
            } else if (count == 0) {
                status = {str::kRecordingsNone, kTextMuted};
            } else {
                snprintf(buffer, sizeof(buffer), count == 1 ? str::kRecordingsOne : str::kRecordingsCount,
                         static_cast<unsigned>(count));
                status = {buffer, kOk};
            }
            break;
        }
        case CardKind::MicCheck:
            text = str::kCardMicCheckText;
            if (settings.calibrated) {
                snprintf(buffer, sizeof(buffer), str::kCalibrated, settings.micGainDb);
                status = {buffer, kOk};
            } else {
                status = {str::kNotCalibrated, kWarn};
            }
            break;
        case CardKind::Settings:
            text = str::kCardSettingsText;
            if (settings.voiceActivation) {
                snprintf(buffer, sizeof(buffer), str::kSettingsSummaryOn, settings.sensitivity);
                status = {buffer, kTextMuted};
            } else {
                status = {str::kSettingsSummaryOff, kTextMuted};
            }
            break;
        case CardKind::Help:
            text = str::kCardHelpText;
            break;
    }

    canvas.setFont(&fonts::Font0);
    int textTop = top + 66;
    if (!status.text.isEmpty()) {
        const int width = min<int>(canvas.textWidth(status.text), kScreenWidth - 40);
        const int x = centerX - width / 2;
        canvas.fillCircle(x - 7, textTop + 3, 3, status.colour);
        canvas.setTextColor(status.colour);
        canvas.setTextDatum(top_left);
        painter::drawClippedText(canvas, status.text, x, textTop, kScreenWidth - 40);
        textTop += 12;
    }
    canvas.setTextColor(kTextMuted);
    canvas.setTextDatum(top_center);
    const int lines = (top + height - textTop) / 10;
    painter::drawWrappedText(canvas, text, centerX, textTop, kScreenWidth - 2 * kPadding - 8, 10, lines);
}

}  // namespace voicerec
