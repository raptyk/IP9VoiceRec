// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#include "App.h"

#include <M5Cardputer.h>

#include "audio/Player.h"
#include "audio/Recorder.h"
#include "hw/AudioHardware.h"
#include "storage/SdStorage.h"
#include "ui/Strings.h"
#include "ui/Theme.h"
#include "ui/screens/HomeScreen.h"
#include "ui/screens/RecordScreen.h"

namespace voicerec {

namespace {

constexpr uint32_t kStorageCheckMs = 10000;  // look for a newly inserted card (a failed mount takes a moment)
constexpr uint32_t kDimAfterMs = 30000;
constexpr uint64_t kMinFreeBytes = 1 << 20;  // refuse to start a recording below 1 MB

// Status LED colours (0xRRGGBB); success / error come from ip9::led.
constexpr uint32_t kLedRecord = 0xFF0010;
constexpr uint32_t kLedPaused = 0xFF8000;
constexpr uint32_t kLedSaving = 0xFFFFFF;

}  // namespace

void App::begin() {
    auto config = M5.config();
    config.internal_mic = true;  // pins (and the ES8311 setup on the ADV) come from M5Unified
    config.internal_spk = true;
    config.serial_baudrate = 0;  // UART0 must stay off GPIO43 (microphone / speaker clock)
    beginShell(config, theme::kAccent);

    const bool adv = device_.model() == DeviceModel::CardputerAdv;
    settings_.load(adv);

    AudioHardware::instance().begin(adv);
    statusLed_.setSteadyLevel(60);  // recording: clearly visible, not a torch
    statusLed_.begin();
    SdStorage::instance().ensureMounted();
    Recorder::instance().begin();
    Player::instance().begin();
    applySettings();
    if (settings_.statusLed) {
        statusLed_.blink(kLedRecord, 1, 120, 60);  // the LED works
    }

    log_i("%s %s on %s", str::kAppName, APP_VERSION, device_.modelName());
    push(std::make_unique<HomeScreen>(*this));
}

void App::loop() {
    Shell::loop();
    delay(1);
}

void App::onLoop() {
    refreshStorage();
}

bool App::startRecording() {
    SdStorage& storage = SdStorage::instance();
    Recorder& recorder = Recorder::instance();
    if (recorder.active()) {
        warn(str::kErrorBusy);
        return false;
    }
    if (!storage.ensureMounted()) {
        warn(str::kErrorNoCard);
        return false;
    }
    if (storage.freeBytes() < kMinFreeBytes) {
        warn(str::kErrorCardFull);
        return false;
    }
    Player::instance().stop();  // the speaker and the microphone share pins
    recorder.configure(settings_.recorderConfig());
    switch (recorder.start(storage.nextRecordingPath(), settings_.bitrateKbps)) {
        case StartResult::Ok:
            break;
        case StartResult::Busy:
            warn(str::kErrorBusy);
            return false;
        case StartResult::FileError:
        case StartResult::EncoderError:
            warn(str::kErrorFile);
            return false;
    }
    push(std::make_unique<RecordScreen>(*this));
    return true;
}

void App::saveSettings() {
    settings_.save();
}

void App::applySettings() {
    setDisplayPower(settings_.brightness, kDimAfterMs, settings_.screenOffSec * 1000UL, settings_.statusLed);
    Recorder::instance().configure(settings_.recorderConfig());
    Player::instance().setVolume(settings_.speakerVolume());
}

void App::updateStatusLed() {
    // Signals: steady red = a voice is being recorded (or voice activation is off),
    // slow red pulse = listening for a voice, steady amber = paused, white pulse = saving,
    // two green blinks = saved, red double blink = something went wrong,
    // short red blink every 10 s = battery at 10 % or less.
    const Recorder& recorder = Recorder::instance();
    switch (recorder.state()) {
        case RecorderState::Starting:
            statusLed_.setBackground(LedPattern::Breathing, kLedRecord);
            return;
        case RecorderState::Recording:
            if (recorder.paused()) {
                statusLed_.setBackground(LedPattern::Steady, kLedPaused);
            } else if (!settings_.voiceActivation || recorder.voiceState() != VoiceState::Silence) {
                statusLed_.setBackground(LedPattern::Steady, kLedRecord);
            } else {
                statusLed_.setBackground(LedPattern::Breathing, kLedRecord);
            }
            return;
        case RecorderState::Saving:
            statusLed_.setBackground(LedPattern::Breathing, kLedSaving);
            return;
        case RecorderState::Idle:
            break;
    }
    statusLed_.setBackground(LedPattern::Off, 0);
    blinkIfBatteryLow();
}

void App::refreshStorage() {
    // A card inserted after start-up is picked up here; never while audio uses the card.
    SdStorage& storage = SdStorage::instance();
    if (storage.mounted() || Recorder::instance().active() || Player::instance().active()) {
        return;
    }
    if (millis() - lastStorageCheckAt_ > kStorageCheckMs) {
        lastStorageCheckAt_ = millis();
        if (storage.ensureMounted()) {
            invalidate();
        }
    }
}

StatusLine App::statusLine(const ip9::Screen& top) {
    using namespace theme;
    // Section chip: which part of the app this is; then the card state: recordings and free space.
    StatusLine line{str::kChipApp, kAccent, kOk, String()};
    if (top.chipLabel() != nullptr) {
        line.chip = top.chipLabel();
        line.chipColour = top.chipColour();
    }
    SdStorage& storage = SdStorage::instance();
    char description[48];
    if (!storage.mounted()) {
        line.dotColour = kWarn;
        snprintf(description, sizeof(description), "%s", str::kStatusNoCard);
    } else {
        const size_t count = storage.recordingCount();
        const String free = painter::formatBytes(storage.freeBytes());
        if (count == 1) {
            snprintf(description, sizeof(description), str::kStatusOneRecording, free.c_str());
        } else {
            snprintf(description, sizeof(description), str::kStatusRecordings, static_cast<unsigned>(count),
                     free.c_str());
        }
        if (storage.freeBytes() < kMinFreeBytes) {
            line.dotColour = kError;
        }
    }
    line.text = description;
    return line;
}

}  // namespace voicerec
