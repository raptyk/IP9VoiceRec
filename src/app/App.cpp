// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#include "App.h"

#include <M5Cardputer.h>

#include "audio/Player.h"
#include "audio/Recorder.h"
#include "hw/AudioHardware.h"
#include "storage/SdStorage.h"
#include "ui/Painter.h"
#include "ui/Strings.h"
#include "ui/Theme.h"
#include "ui/screens/HomeScreen.h"
#include "ui/screens/RecordScreen.h"

namespace voicerec {

namespace {

constexpr uint32_t kToastDurationMs = 2000;
constexpr uint32_t kStatusRefreshMs = 500;
constexpr uint32_t kStorageCheckMs = 10000;  // look for a newly inserted card (a failed mount takes a moment)
constexpr uint32_t kDimAfterMs = 30000;
constexpr uint8_t kDimBrightness = 24;
constexpr uint32_t kMinFrameIntervalMs = 33;  // ~30 fps is plenty and leaves CPU time for input
constexpr int kBatteryWidth = 52;             // battery icon + "100%" on the right of the status bar
constexpr uint64_t kMinFreeBytes = 1 << 20;   // refuse to start a recording below 1 MB

// Status LED colours (0xRRGGBB).
constexpr uint32_t kLedRecord = 0xFF0010;
constexpr uint32_t kLedPaused = 0xFF8000;
constexpr uint32_t kLedSaving = 0xFFFFFF;
constexpr uint32_t kLedSuccess = 0x00FF30;
constexpr uint32_t kLedError = 0xFF0000;

}  // namespace

App::App() : canvas_(&M5.Display) {}

void App::begin() {
    auto config = M5.config();
    config.internal_mic = true;  // pins (and the ES8311 setup on the ADV) come from M5Unified
    config.internal_spk = true;
    config.serial_baudrate = 0;  // UART0 must stay off GPIO43 (microphone / speaker clock)
    M5Cardputer.begin(config, true);
    M5.Display.setRotation(1);

    device_.begin();
    const bool adv = device_.model() == DeviceModel::CardputerAdv;
    settings_.load(adv);

    canvas_.setColorDepth(16);
    if (!canvas_.createSprite(theme::kScreenWidth, theme::kScreenHeight)) {
        // Not enough contiguous RAM for 16-bit: fall back to 8-bit colour.
        canvas_.setColorDepth(8);
        canvas_.createSprite(theme::kScreenWidth, theme::kScreenHeight);
    }

    AudioHardware::instance().begin(adv);
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
    lastInputAt_ = millis();
}

void App::loop() {
    M5Cardputer.update();
    device_.update();
    keyboard_.update();
    handleInput();
    if (!screens_.empty()) {
        screens_.back()->tick();
    }
    retired_.clear();
    updatePower();
    updateStatusLed();
    statusLed_.loop();
    refreshStorage();

    const uint32_t now = millis();
    if (now - lastStatusRefreshAt_ >= kStatusRefreshMs) {
        lastStatusRefreshAt_ = now;
        dirty_ = true;  // status bar (battery, card) may have changed
    }
    if (toastUntil_ != 0 && now > toastUntil_) {
        toastUntil_ = 0;
        dirty_ = true;
    }
    if (dirty_ && !screenOff_ && now - lastRenderAt_ >= kMinFrameIntervalMs) {
        render();
    }
    delay(1);
}

void App::push(std::unique_ptr<Screen> screen) {
    screens_.push_back(std::move(screen));
    screens_.back()->onEnter();
    dirty_ = true;
}

void App::pop() {
    if (screens_.size() <= 1) {
        return;  // the home screen is the root and never closes
    }
    retired_.push_back(std::move(screens_.back()));
    screens_.pop_back();
    screens_.back()->onEnter();
    dirty_ = true;
}

void App::popToRoot() {
    while (screens_.size() > 1) {
        retired_.push_back(std::move(screens_.back()));
        screens_.pop_back();
    }
    screens_.back()->onEnter();
    dirty_ = true;
}

void App::toast(const String& message) {
    toastText_ = message;
    toastUntil_ = millis() + kToastDurationMs;
    dirty_ = true;
}

void App::warn(const String& message) {
    toast(message);
    signalWarning();
}

void App::signalWarning() {
    statusLed_.blink(kLedError, 2, 70, 90, true);
}

void App::signalSuccess() {
    statusLed_.blink(kLedSuccess, 2);
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
    // ADV: the LED is powered from the backlight rail, so an enabled status light keeps the
    // backlight at full duty (see Device::setLedPowerHold).
    device_.setLedPowerHold(settings_.statusLed);
    if (!screenDimmed_ && !screenOff_) {
        device_.setBrightness(settings_.brightness);
    }
    statusLed_.setEnabled(settings_.statusLed);
    Recorder::instance().configure(settings_.recorderConfig());
    Player::instance().setVolume(settings_.speakerVolume());
}

void App::handleInput() {
    while (keyboard_.hasEvent()) {
        KeyEvent event = keyboard_.nextEvent();
        lastInputAt_ = millis();
        if (screenOff_) {
            // The key that wakes the screen is swallowed, so a recording is never stopped by accident.
            screenOff_ = false;
            screenDimmed_ = false;
            device_.setBrightness(settings_.brightness);
            keyboard_.suppressHeldKeys();
            dirty_ = true;
            continue;
        }
        if (screenDimmed_) {
            screenDimmed_ = false;
            device_.setBrightness(settings_.brightness);
        }
        if (!screens_.empty()) {
            screens_.back()->onKey(event);
        }
        dirty_ = true;
    }
}

void App::updatePower() {
    if (keyboard_.anyHeld()) {
        lastInputAt_ = millis();
        return;
    }
    const uint32_t idle = millis() - lastInputAt_;
    if (!screenOff_ && settings_.screenOffSec > 0 && idle > settings_.screenOffSec * 1000UL) {
        screenOff_ = true;
        device_.setBrightness(0);
    } else if (!screenDimmed_ && !screenOff_ && idle > kDimAfterMs) {
        screenDimmed_ = true;
        device_.setBrightness(min<uint8_t>(kDimBrightness, settings_.brightness));
    }
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
    if (millis() - lastBatteryBlinkAt_ > 10000) {
        lastBatteryBlinkAt_ = millis();
        const int battery = device_.batteryPercent();
        if (battery >= 0 && battery <= 10 && !device_.isCharging()) {
            statusLed_.flash(kLedError, 40);
        }
    }
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
            dirty_ = true;
        }
    }
}

void App::render() {
    dirty_ = false;
    lastRenderAt_ = millis();
    canvas_.fillScreen(theme::kBackground);

    const int contentTop = theme::kStatusBarHeight;
    const int contentHeight = theme::kScreenHeight - theme::kStatusBarHeight - theme::kFooterHeight;
    Screen& screen = *screens_.back();
    screen.draw(canvas_, contentTop, contentHeight);

    drawStatusBar();
    drawFooter(screen.footer());
    drawToast();
    canvas_.pushSprite(0, 0);
}

void App::drawStatusBar() {
    using namespace theme;
    canvas_.fillRect(0, 0, kScreenWidth, kStatusBarHeight, kSurface);

    // Section chip: which part of the app this is.
    const Screen& screen = *screens_.back();
    const char* chip = screen.chipLabel();
    canvas_.setFont(&fonts::Font0);
    canvas_.setTextSize(1);
    const int chipWidth = canvas_.textWidth(chip) + 12;
    canvas_.fillRoundRect(2, 2, chipWidth, kStatusBarHeight - 4, 4, screen.chipColour());
    canvas_.setTextColor(kText);
    canvas_.setTextDatum(middle_center);
    canvas_.drawString(chip, 2 + chipWidth / 2, kStatusBarHeight / 2);

    // Card state: coloured dot + recordings and free space.
    const SdStorage& storage = SdStorage::instance();
    uint16_t dotColour = kOk;
    char description[48];
    if (!storage.mounted()) {
        dotColour = kWarn;
        snprintf(description, sizeof(description), "%s", str::kStatusNoCard);
    } else {
        const size_t count = SdStorage::instance().recordingCount();
        const String free = painter::formatBytes(storage.freeBytes());
        if (count == 1) {
            snprintf(description, sizeof(description), str::kStatusOneRecording, free.c_str());
        } else {
            snprintf(description, sizeof(description), str::kStatusRecordings, static_cast<unsigned>(count),
                     free.c_str());
        }
        if (storage.freeBytes() < kMinFreeBytes) {
            dotColour = kError;
        }
    }
    const int dotX = 2 + chipWidth + 8;
    canvas_.fillCircle(dotX, kStatusBarHeight / 2, 3, dotColour);
    canvas_.setTextDatum(middle_left);
    canvas_.setTextColor(kText);
    painter::drawClippedText(canvas_, description, dotX + 7, kStatusBarHeight / 2,
                             kScreenWidth - kBatteryWidth - (dotX + 7));

    painter::drawBattery(canvas_, kScreenWidth - 4, kStatusBarHeight / 2, device_.batteryPercent(),
                         device_.isCharging());
}

void App::drawFooter(const char* text) {
    using namespace theme;
    const int top = kScreenHeight - kFooterHeight;
    canvas_.fillRect(0, top, kScreenWidth, kFooterHeight, kSurface);
    if (text == nullptr) {
        return;
    }
    canvas_.setFont(&fonts::Font0);
    canvas_.setTextSize(1);
    canvas_.setTextColor(kTextMuted);
    canvas_.setTextDatum(middle_center);
    canvas_.drawString(text, kScreenWidth / 2, top + kFooterHeight / 2);
}

void App::drawToast() {
    using namespace theme;
    if (toastUntil_ == 0) {
        return;
    }
    canvas_.setFont(&fonts::Font2);
    canvas_.setTextSize(1);
    const int maxTextWidth = kScreenWidth - 24;
    std::vector<String> lines = painter::wrapText(canvas_, toastText_, maxTextWidth);
    if (lines.size() > 2) {
        lines.resize(2);
    }
    int textWidth = 0;
    for (const String& line : lines) {
        textWidth = max<int>(textWidth, canvas_.textWidth(line));
    }
    const int lineHeight = 15;
    const int width = min<int>(textWidth + 16, kScreenWidth - 8);
    const int height = static_cast<int>(lines.size()) * lineHeight + 8;
    const int x = (kScreenWidth - width) / 2;
    const int y = kScreenHeight - kFooterHeight - height - 4;
    canvas_.fillRoundRect(x, y, width, height, 6, kSurfaceHigh);
    canvas_.drawRoundRect(x, y, width, height, 6, kAccent);
    canvas_.setTextColor(kText);
    canvas_.setTextDatum(top_center);
    for (size_t i = 0; i < lines.size(); ++i) {
        painter::drawClippedText(canvas_, lines[i], kScreenWidth / 2, y + 4 + i * lineHeight, maxTextWidth);
    }
}

}  // namespace voicerec
