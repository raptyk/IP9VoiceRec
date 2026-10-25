// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#include "App.h"
#include <esp_log.h>
#include "../hw/Device.h"
#include "../hw/StatusLed.h"
#include "../hw/AudioModeManager.h"
#include "../hw/KeyboardInput.h"
#include "../storage/SdStorage.h"
#include "../audio/AudioPipeline.h"
#include "../ui/Theme.h"
#include "../ui/Painter.h"
#include "../ui/screens/HomeScreen.h"

namespace voicerec {

static const char* TAG = "App";

App::App()
    : canvas_(&M5.Display) {}

void App::begin() {
    auto cfg = M5.config();
    cfg.internal_mic = false; // Initialized manually via AudioModeManager
    cfg.internal_spk = false; // Initialized manually via AudioModeManager
    cfg.serial_baudrate = 0;  // Never use UART0 on GPIO 43/44
    M5Cardputer.begin(cfg, true);
    M5.Display.setRotation(1);

    Device::instance().begin();
    AudioModeManager::instance().begin();
    StatusLed::instance().begin();
    SdStorage::instance().begin();

    // Load persisted settings from SD
    settings_.load();
    Device::instance().setBrightness(settings_.screenBrightness);
    StatusLed::instance().setEnabled(settings_.statusLedEnabled);

    // Initialize audio pipeline (FreeRTOS tasks)
    AudioPipeline::instance().begin();

    // Initialize 16-bit double buffer canvas
    canvas_.setColorDepth(16);
    if (!canvas_.createSprite(theme::kScreenWidth, theme::kScreenHeight)) {
        ESP_LOGW(TAG, "Falling back to 8-bit canvas due to RAM");
        canvas_.setColorDepth(8);
        canvas_.createSprite(theme::kScreenWidth, theme::kScreenHeight);
    }

    ESP_LOGI(TAG, "CardputerVoiceRec started on %s", Device::instance().modelName());

    // Push initial HomeScreen
    push(std::make_unique<HomeScreen>(*this));
}

void App::push(std::unique_ptr<Screen> screen) {
    if (screen) {
        screens_.push_back(std::move(screen));
    }
}

void App::pop() {
    if (screens_.size() > 1) {
        screens_.pop_back();
    }
}

void App::replace(std::unique_ptr<Screen> screen) {
    if (!screens_.empty()) {
        screens_.pop_back();
    }
    push(std::move(screen));
}

void App::toast(const std::string& message, uint32_t durationMs) {
    toastMessage_ = message;
    toastUntil_ = millis() + durationMs;
}

void App::loop() {
    M5Cardputer.update();
    Device::instance().update();
    KeyboardInput::instance().update();

    // Dispatch keyboard events to current active screen
    while (KeyboardInput::instance().hasEvent()) {
        KeyEvent ev = KeyboardInput::instance().nextEvent();
        if (!screens_.empty()) {
            screens_.back()->onKey(ev);
        }
    }

    // Tick current active screen
    if (!screens_.empty()) {
        screens_.back()->tick();
    }

    // Update status LED
    StatusLed::instance().loop();

    // Render screen (~30 fps cap)
    const uint32_t now = millis();
    if (now - lastRenderAt_ >= 33) {
        lastRenderAt_ = now;
        render();
    }
}

void App::render() {
    if (screens_.empty()) {
        return;
    }

    // Draw active screen onto offscreen sprite
    screens_.back()->draw(canvas_);

    // Draw toast overlay if active
    const uint32_t now = millis();
    if (toastUntil_ > now && !toastMessage_.empty()) {
        const int msgW = canvas_.textWidth(toastMessage_.c_str());
        const int cardW = msgW + 24;
        const int cardH = 26;
        const int cardX = (theme::kScreenWidth - cardW) / 2;
        const int cardY = (theme::kScreenHeight - cardH) / 2;

        Painter::drawCard(canvas_, cardX, cardY, cardW, cardH, 0x0000, theme::kPrimary);
        canvas_.setTextColor(theme::kText, 0x0000);
        canvas_.setTextSize(1);
        canvas_.drawString(toastMessage_.c_str(), cardX + 12, cardY + 8);
    }

    // Push frame to ST7789 display
    canvas_.pushSprite(0, 0);
}

}  // namespace voicerec
