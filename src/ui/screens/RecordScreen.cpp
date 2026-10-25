// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#include "RecordScreen.h"
#include <esp_log.h>
#include "../../app/App.h"
#include "../../hw/Device.h"
#include "../../hw/StatusLed.h"
#include "../../storage/SdStorage.h"
#include "../../audio/AudioPipeline.h"
#include "../../ui/Painter.h"
#include "CalibrationScreen.h"

namespace voicerec {

[[maybe_unused]] static const char* TAG = "RecordScreen";

RecordScreen::RecordScreen(App& app)
    : Screen(app) {
    auto& pipe = AudioPipeline::instance();
    auto& settings = app_.settings();

    pipe.vad().setVasEnabled(settings.vasEnabled);
    pipe.vad().setAttackThreshold(settings.vasAttackThresholdDbfs);
    pipe.vad().setHysteresisMargin(settings.vasHysteresisMarginDb);
    pipe.vad().setHangoverMs(settings.vasHangoverMs);

    filePath_ = SdStorage::instance().generateNextRecordingPath();
    const bool started = pipe.startRecording(filePath_, settings.bitrateKbps);
    if (!started) {
        ESP_LOGE(TAG, "Failed to start recording");
        app_.toast("Rec Failed (SD?)");
        stopped_ = true;
    }
}

RecordScreen::~RecordScreen() {
    AudioPipeline::instance().stopRecording();
    StatusLed::instance().setState(LedState::Idle);
}

void RecordScreen::tick() {
    if (stopped_) {
        app_.pop();
        return;
    }
    updateStatusLed();
}

void RecordScreen::updateStatusLed() {
    if (!app_.settings().statusLedEnabled) {
        StatusLed::instance().setState(LedState::Off);
        return;
    }

    auto& pipe = AudioPipeline::instance();
    if (pipe.isPaused()) {
        StatusLed::instance().setState(LedState::Idle);
    } else if (pipe.isRecording()) {
        switch (pipe.currentVadState()) {
            case VadState::Speech:
                StatusLed::instance().setState(LedState::Recording);
                break;
            case VadState::Hangover:
                StatusLed::instance().setState(LedState::Hangover);
                break;
            case VadState::Silence:
                StatusLed::instance().setState(LedState::WaitingSilence);
                break;
        }
    }
}

void RecordScreen::draw(M5Canvas& canvas) {
    canvas.fillScreen(theme::kBg);

    auto& pipe = AudioPipeline::instance();
    const int bat = Device::instance().batteryLevel();
    const bool chg = Device::instance().isCharging();
    const bool sdOk = SdStorage::instance().isMounted();

    // Header
    Painter::drawHeader(canvas, "Recording Audio", bat, chg, sdOk);

    // Extract filename from path
    std::string filename = filePath_;
    const size_t slashPos = filename.find_last_of('/');
    if (slashPos != std::string::npos) {
        filename = filename.substr(slashPos + 1);
    }

    // Top Card: Target file & Bitrate
    Painter::drawCard(canvas, 6, 22, theme::kScreenWidth - 12, 17, theme::kPanelBg, theme::kPanelBorder);
    canvas.setTextSize(1);
    canvas.setTextColor(theme::kText, theme::kPanelBg);
    canvas.drawString(filename.c_str(), 12, 26);

    char brBuf[32];
    snprintf(brBuf, sizeof(brBuf), "MP3 %d kbps", app_.settings().bitrateKbps);
    canvas.setTextColor(theme::kPrimary, theme::kPanelBg);
    const int brW = canvas.textWidth(brBuf);
    canvas.drawString(brBuf, theme::kScreenWidth - 12 - brW, 26);

    // VU Meter Section
    const float rms = pipe.currentRmsDbfs();
    const float peak = pipe.currentPeakDbfs();
    const float threshold = pipe.vad().attackThreshold();
    const VadState vadState = pipe.currentVadState();

    // Big VU Meter
    constexpr int kMeterX = 6;
    constexpr int kMeterY = 43;
    constexpr int kMeterW = theme::kScreenWidth - 12;
    constexpr int kMeterH = 14;
    Painter::drawVuMeter(canvas, kMeterX, kMeterY, kMeterW, kMeterH, rms, peak, threshold, vadState);

    // VU numeric label
    char dbBuf[32];
    snprintf(dbBuf, sizeof(dbBuf), "%.1f dBFS (Trg: %.0f dB)", rms, threshold);
    canvas.setTextColor(theme::kTextDim, theme::kBg);
    canvas.drawString(dbBuf, 8, kMeterY + kMeterH + 3);

    // VAD Status Badge
    const int badgeY = 74;
    if (pipe.isPaused()) {
        Painter::drawBadge(canvas, 8, badgeY, "PAUSED", theme::kPanelBorder, theme::kText);
    } else if (!app_.settings().vasEnabled) {
        Painter::drawBadge(canvas, 8, badgeY, "CONTINUOUS REC", theme::kDanger, theme::kText);
    } else {
        switch (vadState) {
            case VadState::Speech:
                Painter::drawBadge(canvas, 8, badgeY, "REC: SPEECH DETECTED", theme::kDanger, theme::kText);
                break;
            case VadState::Hangover: {
                char holdBuf[32];
                snprintf(holdBuf, sizeof(holdBuf), "HOLD (%.1fs)", pipe.vad().remainingHangoverMs() / 1000.0f);
                Painter::drawBadge(canvas, 8, badgeY, holdBuf, theme::kWarning, 0x0000);
                break;
            }
            case VadState::Silence:
                Painter::drawBadge(canvas, 8, badgeY, "SILENCE (PAUSED)", 0x0273, theme::kPrimary);
                break;
        }
    }

    // Counters Card
    Painter::drawCard(canvas, 6, 91, theme::kScreenWidth - 12, 25, theme::kPanelBg, theme::kPanelBorder);

    // Speech time
    const uint32_t spSec = pipe.speechDurationMs() / 1000;
    const uint32_t spMin = spSec / 60;
    const uint32_t spRemSec = spSec % 60;

    // Total elapsed time
    const uint32_t totSec = pipe.totalDurationMs() / 1000;
    const uint32_t totMin = totSec / 60;
    const uint32_t totRemSec = totSec % 60;

    char timeBuf[48];
    snprintf(timeBuf, sizeof(timeBuf), "Audio: %02u:%02u  Total: %02u:%02u", spMin, spRemSec, totMin, totRemSec);
    canvas.setTextColor(theme::kText, theme::kPanelBg);
    canvas.drawString(timeBuf, 12, 97);

    // Size & Saved %
    char sizeBuf[32];
    const size_t szKb = pipe.currentFileSize() / 1024;
    int savedPct = 0;
    if (totSec > 0 && spSec <= totSec) {
        savedPct = static_cast<int>(((totSec - spSec) * 100) / totSec);
    }
    snprintf(sizeBuf, sizeof(sizeBuf), "%u KB (-%d%%)", szKb, savedPct);
    const int szW = canvas.textWidth(sizeBuf);
    canvas.setTextColor(theme::kSuccess, theme::kPanelBg);
    canvas.drawString(sizeBuf, theme::kScreenWidth - 12 - szW, 97);

    // Footer
    Painter::drawFooter(canvas, "[ENTER] Stop  [P] Pause  [+/-] Sens  [C] Calib");
}

void RecordScreen::onKey(const KeyEvent& event) {
    auto& pipe = AudioPipeline::instance();

    if (event.code == keycode::kEnter || event.code == keycode::kSpace ||
        event.code == keycode::kOpt || event.code == 0x1B) {
        pipe.stopRecording();
        app_.toast("Saved to SD");
        stopped_ = true;
    } else if (event.code == 'p' || event.code == 'P') {
        if (pipe.isPaused()) {
            pipe.resumeRecording();
        } else {
            pipe.pauseRecording();
        }
    } else if (event.code == '+' || event.code == '=') {
        // Increase sensitivity (lower threshold)
        float t = pipe.vad().attackThreshold() - 1.0f;
        pipe.vad().setAttackThreshold(t);
        app_.settings().vasAttackThresholdDbfs = t;
    } else if (event.code == '-' || event.code == '_') {
        // Decrease sensitivity (higher threshold)
        float t = pipe.vad().attackThreshold() + 1.0f;
        pipe.vad().setAttackThreshold(t);
        app_.settings().vasAttackThresholdDbfs = t;
    } else if (event.code == 'c' || event.code == 'C') {
        app_.push(std::make_unique<CalibrationScreen>(app_));
    }
}

}  // namespace voicerec
