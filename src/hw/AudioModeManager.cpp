// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#include "AudioModeManager.h"
#include <M5Cardputer.h>
#include <esp_log.h>

namespace voicerec {

[[maybe_unused]] static const char* TAG = "AudioModeMgr";

void AudioModeManager::begin() {
    // Both peripherals start in closed/idle state
    if (M5.Speaker.isEnabled()) {
        M5.Speaker.end();
    }
    if (M5.Mic.isEnabled()) {
        M5.Mic.end();
    }
    currentMode_ = AudioMode::Idle;
    ESP_LOGI(TAG, "AudioModeManager started in IDLE mode");
}

bool AudioModeManager::setMode(AudioMode mode) {
    if (mode == currentMode_) {
        return true;
    }

    ESP_LOGI(TAG, "Switching audio mode: %d -> %d", static_cast<int>(currentMode_), static_cast<int>(mode));

    // First strictly terminate the active peripheral
    switch (currentMode_) {
        case AudioMode::Record:
            if (M5.Mic.isEnabled()) {
                while (M5.Mic.isRecording()) {
                    vTaskDelay(pdMS_TO_TICKS(2));
                }
                M5.Mic.end();
            }
            break;

        case AudioMode::Playback:
            if (M5.Speaker.isEnabled()) {
                M5.Speaker.stop();
                M5.Speaker.end();
            }
            break;

        case AudioMode::Idle:
            break;
    }

    // Hardware quiet delay to settle I2S/PDM clock and power rails
    vTaskDelay(pdMS_TO_TICKS(15));

    // Now safely start the requested mode
    bool success = true;
    switch (mode) {
        case AudioMode::Idle:
            // Both remained closed
            break;

        case AudioMode::Record: {
            auto micCfg = M5.Mic.config();
            micCfg.sample_rate = 16000;
            micCfg.over_sampling = 2;
            // Cardputer ADV uses ES8311 codec with built-in analog PGA; standard Cardputer uses PDM MEMS
            if (M5.getBoard() == m5::board_t::board_M5CardputerADV) {
                micCfg.magnification = 2;
            } else {
                micCfg.magnification = 16;
            }
            M5.Mic.config(micCfg);
            success = M5.Mic.begin();
            if (!success) {
                ESP_LOGE(TAG, "Failed to start M5.Mic");
            }
            break;
        }

        case AudioMode::Playback: {
            auto spkCfg = M5.Speaker.config();
            M5.Speaker.config(spkCfg);
            success = M5.Speaker.begin();
            if (success) {
                M5.Speaker.setVolume(speakerVolume_);
            } else {
                ESP_LOGE(TAG, "Failed to start M5.Speaker");
            }
            break;
        }
    }

    currentMode_ = mode;
    return success;
}

void AudioModeManager::setSpeakerVolume(uint8_t volume) {
    speakerVolume_ = volume;
    if (currentMode_ == AudioMode::Playback && M5.Speaker.isEnabled()) {
        M5.Speaker.setVolume(speakerVolume_);
    }
}

}  // namespace voicerec
