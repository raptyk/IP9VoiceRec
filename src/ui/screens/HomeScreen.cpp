// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#include "HomeScreen.h"

#include "app/App.h"
#include "storage/SdStorage.h"
#include "ui/Icons.h"
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

HomeScreen::HomeScreen(App& app) : AppScreen(app, kCardCount) {}

bool HomeScreen::onExtraKey(const KeyEvent& event) {
    if (event.code != 'r') {
        return false;
    }
    if (!event.repeat) {
        app_.startRecording();
    }
    return true;
}

void HomeScreen::openHelp() {
    app_.push(std::make_unique<HelpScreen>(app_));
}

void HomeScreen::open(int index) {
    switch (kCards[index].kind) {
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
            openHelp();
            break;
    }
}

const char* HomeScreen::footer() const {
    return str::kFooterHome;
}

void HomeScreen::drawIcon(M5Canvas& canvas, int index, int cx, int cy, uint16_t colour, float scale,
                          uint16_t background) const {
    icons::draw(canvas, kCards[index].icon, cx, cy, colour, scale, background);
}

CarouselCard HomeScreen::card(int index) const {
    using namespace theme;
    const Card& card = kCards[index];
    const Settings& settings = app_.settings();
    SdStorage& storage = SdStorage::instance();

    // Status line: is this ready, or what is missing?
    CarouselCard result{card.title, nullptr, card.colour, String(), kTextMuted, 0.9f};
    char buffer[64];
    switch (card.kind) {
        case CardKind::Record:
            result.text = settings.voiceActivation ? str::kCardRecordText : str::kCardRecordTextAlways;
            if (!storage.mounted()) {
                result.status = str::kInsertCard;
                result.statusColour = kWarn;
            } else if (storage.freeBytes() < (1 << 20)) {
                result.status = str::kCardFull;
                result.statusColour = kError;
            } else {
                snprintf(buffer, sizeof(buffer), str::kReadySpace,
                         recordingTimeLeft(storage.freeBytes(), settings.bitrateKbps).c_str());
                result.status = buffer;
                result.statusColour = kOk;
            }
            break;
        case CardKind::Library: {
            result.text = str::kCardLibraryText;
            const size_t count = storage.recordingCount();
            if (!storage.mounted()) {
                result.status = str::kInsertCard;
                result.statusColour = kWarn;
            } else if (count == 0) {
                result.status = str::kRecordingsNone;
            } else {
                snprintf(buffer, sizeof(buffer), count == 1 ? str::kRecordingsOne : str::kRecordingsCount,
                         static_cast<unsigned>(count));
                result.status = buffer;
                result.statusColour = kOk;
            }
            break;
        }
        case CardKind::MicCheck:
            result.text = str::kCardMicCheckText;
            if (settings.calibrated) {
                snprintf(buffer, sizeof(buffer), str::kCalibrated, settings.micGainDb);
                result.status = buffer;
                result.statusColour = kOk;
            } else {
                result.status = str::kNotCalibrated;
                result.statusColour = kWarn;
            }
            break;
        case CardKind::Settings:
            result.text = str::kCardSettingsText;
            if (settings.voiceActivation) {
                snprintf(buffer, sizeof(buffer), str::kSettingsSummaryOn, settings.sensitivity);
                result.status = buffer;
            } else {
                result.status = str::kSettingsSummaryOff;
            }
            break;
        case CardKind::Help:
            result.text = str::kCardHelpText;
            break;
    }
    return result;
}

}  // namespace voicerec
