// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#include "CalibrationScreen.h"
#include <esp_log.h>
#include "../../app/App.h"
#include "../../hw/Device.h"
#include "../../hw/AudioModeManager.h"
#include "../../storage/SdStorage.h"
#include "../../audio/AudioPipeline.h"
#include "../../ui/Painter.h"

namespace voicerec {

CalibrationScreen::CalibrationScreen(App& app)
    : Screen(app) {
    result_.valid = false;
}

CalibrationScreen::~CalibrationScreen() {
    AudioModeManager::instance().setMode(AudioMode::Idle);
}

void CalibrationScreen::tick() {
    if (step_ == CalibStep::Measuring) {
        auto& vad = AudioPipeline::instance().vad();
        if (!vad.isCalibrating()) {
            result_ = vad.finishCalibration();
            AudioModeManager::instance().setMode(AudioMode::Idle);
            step_ = CalibStep::Done;
        }
    }
}

void CalibrationScreen::draw(M5Canvas& canvas) {
    canvas.fillScreen(theme::kBg);

    const int bat = Device::instance().batteryLevel();
    const bool chg = Device::instance().isCharging();
    const bool sdOk = SdStorage::instance().isMounted();
    Painter::drawHeader(canvas, "Noise Calibration", bat, chg, sdOk);

    switch (step_) {
        case CalibStep::Ready: {
            Painter::drawCard(canvas, 10, 24, theme::kScreenWidth - 20, 75);
            canvas.setTextSize(1);
            canvas.setTextColor(theme::kPrimary, theme::kPanelBg);
            canvas.drawString("Ambient Noise Profiler", 18, 30);

            canvas.setTextColor(theme::kText, theme::kPanelBg);
            canvas.drawString("1. Keep the room and device quiet.", 18, 48);
            canvas.drawString("2. Profiler measures background noise", 18, 62);
            canvas.drawString("   and computes optimal VAS threshold.", 18, 76);

            Painter::drawFooter(canvas, "[ENTER] Start Measuring  [ESC] Cancel");
            break;
        }

        case CalibStep::Measuring: {
            Painter::drawCard(canvas, 10, 24, theme::kScreenWidth - 20, 75);
            canvas.setTextSize(1);
            canvas.setTextColor(theme::kWarning, theme::kPanelBg);
            canvas.drawString("Measuring Room Background...", 18, 30);

            const size_t collected = AudioPipeline::instance().vad().calibrationFramesCollected();
            const float progress = static_cast<float>(collected) / kCalibrationFrames;

            Painter::drawProgressBar(canvas, 18, 52, theme::kScreenWidth - 36, 12, progress, theme::kPrimary);

            // Live noise level
            const float liveRms = AudioPipeline::instance().currentRmsDbfs();
            char liveBuf[48];
            snprintf(liveBuf, sizeof(liveBuf), "Current: %.1f dBFS (%d%%)", liveRms, static_cast<int>(progress * 100));
            canvas.setTextColor(theme::kTextDim, theme::kPanelBg);
            canvas.drawString(liveBuf, 18, 74);

            Painter::drawFooter(canvas, "Keep quiet... Measuring noise floor");
            break;
        }

        case CalibStep::Done: {
            Painter::drawCard(canvas, 10, 24, theme::kScreenWidth - 20, 75);
            canvas.setTextSize(1);

            if (result_.valid) {
                canvas.setTextColor(theme::kSuccess, theme::kPanelBg);
                canvas.drawString("Calibration Successful!", 18, 30);

                char buf[48];
                snprintf(buf, sizeof(buf), "Noise Floor (P40):  %.1f dBFS", result_.noiseFloorDbfs);
                canvas.setTextColor(theme::kText, theme::kPanelBg);
                canvas.drawString(buf, 18, 46);

                snprintf(buf, sizeof(buf), "Noise Peak (P85):   %.1f dBFS", result_.peakNoiseDbfs);
                canvas.drawString(buf, 18, 60);

                snprintf(buf, sizeof(buf), "New Threshold:      %.1f dBFS", result_.recommendedAttackDbfs);
                canvas.setTextColor(theme::kPrimary, theme::kPanelBg);
                canvas.drawString(buf, 18, 74);

                Painter::drawFooter(canvas, "[ENTER] Apply & Save  [R] Retry  [ESC] Discard");
            } else {
                canvas.setTextColor(theme::kDanger, theme::kPanelBg);
                canvas.drawString("Calibration Failed", 18, 30);
                canvas.setTextColor(theme::kTextDim, theme::kPanelBg);
                canvas.drawString("Too much audio disturbance.", 18, 52);

                Painter::drawFooter(canvas, "[R] Retry  [ESC] Cancel");
            }
            break;
        }
    }
}

void CalibrationScreen::onKey(const KeyEvent& event) {
    if (step_ == CalibStep::Ready) {
        if (event.code == keycode::kEnter || event.code == keycode::kSpace) {
            AudioModeManager::instance().setMode(AudioMode::Record);
            AudioPipeline::instance().vad().startCalibration();
            step_ = CalibStep::Measuring;
        } else if (event.code == keycode::kOpt || event.code == 0x1B) {
            app_.pop();
        }
    } else if (step_ == CalibStep::Done) {
        if (event.code == keycode::kEnter || event.code == keycode::kSpace) {
            if (result_.valid) {
                app_.settings().vasAttackThresholdDbfs = result_.recommendedAttackDbfs;
                app_.settings().vasHysteresisMarginDb = 5.0f;
                app_.settings().save();
                app_.toast("Settings Saved");
            }
            app_.pop();
        } else if (event.code == 'r' || event.code == 'R') {
            step_ = CalibStep::Ready;
        } else if (event.code == keycode::kOpt || event.code == 0x1B) {
            app_.pop();
        }
    }
}

}  // namespace voicerec
