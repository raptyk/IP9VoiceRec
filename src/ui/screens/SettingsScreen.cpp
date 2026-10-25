// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#include "SettingsScreen.h"

#include <M5Cardputer.h>

#include <algorithm>

#include "app/App.h"
#include "storage/SdStorage.h"
#include "ui/Screen.h"
#include "ui/Strings.h"
#include "ui/screens/MicCheckScreen.h"

namespace voicerec {

namespace {

constexpr uint16_t kScreenOffChoices[] = {30, 60, 120, 300, 0};
constexpr int kBrightnessStep = 25;
constexpr int kBrightnessMin = 30;

template <typename T, size_t N>
T cycle(const T (&choices)[N], T current, int delta) {
    size_t index = 0;
    for (size_t i = 0; i < N; ++i) {
        if (choices[i] == current) {
            index = i;
        }
    }
    return choices[(index + N + (delta > 0 ? 1 : N - 1)) % N];
}

String onOff(bool value) {
    return value ? str::kOn : str::kOff;
}

String screenOffText(uint16_t seconds) {
    if (seconds == 0) {
        return str::kNever;
    }
    return seconds < 60 ? String(seconds) + " s" : String(seconds / 60) + " min";
}

}  // namespace

std::unique_ptr<ListScreen> makeSettingsScreen(App& app) {
    Settings& s = app.settings();
    // Every change is applied and stored at once.
    auto changed = [&app]() {
        app.applySettings();
        app.saveSettings();
    };

    std::vector<ListItem> items;
    items.push_back({str::kSetVoiceActivation, [&s]() { return onOff(s.voiceActivation); }, nullptr,
                     [&s, changed](int) {
                         s.voiceActivation = !s.voiceActivation;
                         changed();
                     },
                     str::kSetVoiceActivationText});
    items.push_back({str::kSetSensitivity,
                     [&s]() {
                         return String(s.sensitivity) + " - " + str::kSensitivityNames[s.sensitivity - kSensitivityMin];
                     },
                     nullptr,
                     [&s, changed](int delta) {
                         s.sensitivity = constrain(s.sensitivity + delta, kSensitivityMin, kSensitivityMax);
                         changed();
                     },
                     str::kSetSensitivityText});
    items.push_back({str::kSetHold, [&s]() { return String(s.holdMs / 1000.0f, 1) + " s"; }, nullptr,
                     [&s, changed](int delta) {
                         s.holdMs = constrain(s.holdMs + delta * kHangoverStepMs, kHangoverMinMs, kHangoverMaxMs);
                         changed();
                     },
                     str::kSetHoldText});
    items.push_back({str::kSetGain, [&s]() { return String("+") + s.micGainDb + " dB"; }, nullptr,
                     [&s, changed](int delta) {
                         s.micGainDb = constrain(s.micGainDb + delta * kMicGainStepDb, kMicGainMinDb, kMicGainMaxDb);
                         changed();
                     },
                     str::kSetGainText});
    items.push_back({str::kSetAutoLevel, [&s]() { return onOff(s.autoLevel); }, nullptr,
                     [&s, changed](int) {
                         s.autoLevel = !s.autoLevel;
                         changed();
                     },
                     str::kSetAutoLevelText});
    items.push_back({str::kSetQuality, [&s]() { return String(s.bitrateKbps) + " kbit/s"; }, nullptr,
                     [&s, changed](int delta) {
                         s.bitrateKbps = cycle(kBitrates, s.bitrateKbps, delta);
                         changed();
                     },
                     str::kSetQualityText});
    items.push_back({str::kSetVolume, [&s]() { return String(s.volume) + " / 10"; }, nullptr,
                     [&s, changed](int delta) {
                         s.volume = constrain(s.volume + delta, 0, 10);
                         changed();
                     },
                     str::kSetVolumeText});
    items.push_back({str::kSetBrightness,
                     [&s, &app]() {
                         return app.device().brightnessLocked() ? String("Max (LED)")
                                                                : String(s.brightness * 100 / 255) + "%";
                     },
                     nullptr,
                     [&s, changed](int delta) {
                         s.brightness = constrain(s.brightness + delta * kBrightnessStep, kBrightnessMin, 255);
                         changed();
                     },
                     str::kSetBrightnessText});
    items.push_back({str::kSetScreenOff, [&s]() { return screenOffText(s.screenOffSec); }, nullptr,
                     [&s, changed](int delta) {
                         s.screenOffSec = cycle(kScreenOffChoices, s.screenOffSec, delta);
                         changed();
                     },
                     str::kSetScreenOffText});
    items.push_back({str::kSetStatusLed, [&s]() { return onOff(s.statusLed); }, nullptr,
                     [&s, changed](int) {
                         s.statusLed = !s.statusLed;
                         changed();
                     },
                     str::kSetStatusLedText});
    items.push_back({str::kSetMicCheck, nullptr, [&app]() { app.push(std::make_unique<MicCheckScreen>(app)); },
                     nullptr, str::kSetMicCheckText});
    items.push_back({str::kSetStorage,
                     []() {
                         SdStorage& storage = SdStorage::instance();
                         return storage.mounted() ? painter::formatBytes(storage.freeBytes()) : String(str::kOff);
                     },
                     [&app]() {
                         SdStorage& storage = SdStorage::instance();
                         if (!storage.ensureMounted()) {
                             app.warn(str::kInsertCard);
                             return;
                         }
                         char body[48];
                         snprintf(body, sizeof(body), str::kStorageInfo, painter::formatBytes(storage.freeBytes()).c_str(),
                                  painter::formatBytes(storage.totalBytes()).c_str());
                         app.push(std::make_unique<InfoScreen>(app, str::kSetStorage, body, str::kSetStorageText));
                     },
                     nullptr, str::kSetStorageText});
    items.push_back({str::kSetAbout, nullptr,
                     [&app]() {
                         char title[40];
                         snprintf(title, sizeof(title), str::kAboutTitle, APP_VERSION);
                         char body[64];
                         snprintf(body, sizeof(body), str::kAboutBody, app.device().modelName(),
                                  static_cast<unsigned>(ESP.getFreeHeap() / 1024));
                         app.push(std::make_unique<InfoScreen>(app, title, body, str::kAboutDetail));
                     },
                     nullptr, str::kSetAboutText});

    return std::make_unique<ListScreen>(app, str::kSettingsTitle, std::move(items));
}

}  // namespace voicerec
