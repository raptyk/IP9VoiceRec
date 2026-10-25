// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#include "AudioPlayer.h"
#include <M5Cardputer.h>
#include <esp_log.h>
#include "../hw/AudioModeManager.h"
#include "../storage/SdStorage.h"

namespace voicerec {

[[maybe_unused]] static const char* TAG = "AudioPlayer";

AudioPlayer::AudioPlayer() {
    decoder_.setDataCallback(onDecodedPcm);
}

AudioPlayer::~AudioPlayer() {
    stop();
}

bool AudioPlayer::begin() {
    if (!playerTaskHandle_) {
        xTaskCreatePinnedToCore(
            playbackTaskTrampoline,
            "AudioPlayer",
            6144,
            this,
            2,
            &playerTaskHandle_,
            0
        );
    }
    return true;
}

bool AudioPlayer::play(const std::string& filePath) {
    stop();

    if (!SdStorage::instance().ensureMounted()) {
        ESP_LOGE(TAG, "Cannot play: SD not mounted");
        return false;
    }

    currentPath_ = filePath;
    audioFile_ = SdStorage::instance().openFile(currentPath_.c_str(), FILE_READ);
    if (!audioFile_) {
        ESP_LOGE(TAG, "Failed to open file for playback: %s", currentPath_.c_str());
        return false;
    }

    fileSizeBytes_ = audioFile_.size();
    // 48 kbps = 6000 bytes/sec
    totalDurationSec_.store(static_cast<uint32_t>(fileSizeBytes_ / 6000));
    currentPositionSec_.store(0);

    // Switch hardware to playback mode (closes mic first)
    if (!AudioModeManager::instance().setMode(AudioMode::Playback)) {
        ESP_LOGE(TAG, "Failed to switch hardware to Playback mode");
        audioFile_.close();
        return false;
    }
    AudioModeManager::instance().setSpeakerVolume(volume_);

    decoder_.begin();
    stopRequested_.store(false);
    playerState_.store(PlayerState::Playing);

    ESP_LOGI(TAG, "Playback started: %s (%u bytes, ~%u sec)",
             currentPath_.c_str(), fileSizeBytes_, totalDurationSec_.load());
    return true;
}

void AudioPlayer::pause() {
    if (playerState_.load() == PlayerState::Playing) {
        playerState_.store(PlayerState::Paused);
        ESP_LOGI(TAG, "Playback paused");
    }
}

void AudioPlayer::resume() {
    if (playerState_.load() == PlayerState::Paused) {
        playerState_.store(PlayerState::Playing);
        ESP_LOGI(TAG, "Playback resumed");
    }
}

void AudioPlayer::stop() {
    if (playerState_.load() == PlayerState::Stopped) {
        return;
    }

    ESP_LOGI(TAG, "Stopping playback...");
    playerState_.store(PlayerState::Stopped);
    stopRequested_.store(true);

    if (M5.Speaker.isEnabled()) {
        M5.Speaker.stop();
    }
    decoder_.end();

    if (audioFile_) {
        audioFile_.close();
    }

    AudioModeManager::instance().setMode(AudioMode::Idle);
    currentPositionSec_.store(0);
}

void AudioPlayer::setVolume(uint8_t vol) {
    volume_ = vol;
    AudioModeManager::instance().setSpeakerVolume(volume_);
}

void AudioPlayer::playbackTaskTrampoline(void* arg) {
    static_cast<AudioPlayer*>(arg)->playbackTaskLoop();
}

void AudioPlayer::playbackTaskLoop() {
    uint8_t readBuf[512];

    while (true) {
        if (playerState_.load() == PlayerState::Playing && audioFile_) {
            if (audioFile_.available()) {
                const size_t bytesRead = audioFile_.read(readBuf, sizeof(readBuf));
                if (bytesRead > 0) {
                    decoder_.write(readBuf, bytesRead);

                    // Update approximate position
                    if (fileSizeBytes_ > 0) {
                        const size_t pos = audioFile_.position();
                        const uint32_t totalSec = totalDurationSec_.load();
                        if (totalSec > 0) {
                            currentPositionSec_.store(static_cast<uint32_t>((pos * totalSec) / fileSizeBytes_));
                        }
                    }
                }
            } else {
                // End of file reached: wait until speaker finishes playing remaining buffer
                while (M5.Speaker.isPlaying() && playerState_.load() == PlayerState::Playing) {
                    vTaskDelay(pdMS_TO_TICKS(10));
                }
                ESP_LOGI(TAG, "Reached end of audio file");
                stop();
            }
        } else {
            vTaskDelay(pdMS_TO_TICKS(20));
        }

        vTaskDelay(1);
    }
}

void AudioPlayer::onDecodedPcm(MP3FrameInfo& info, short* pcmBuffer, size_t len, void* ref) {
    (void)ref;
    if (!pcmBuffer || len == 0 || !M5.Speaker.isEnabled()) {
        return;
    }

    // Real-time pacing: wait until speaker finishes reading previous buffer
    while (M5.Speaker.isPlaying(0) != 0 && AudioPlayer::instance().isPlaying()) {
        vTaskDelay(pdMS_TO_TICKS(2));
    }

    if (!AudioPlayer::instance().isPlaying()) {
        return;
    }

    // len is number of samples (bytes / 2)
    // info.nChans: 1 = mono, 2 = stereo
    M5.Speaker.playRaw(
        pcmBuffer,
        len,
        info.samprate,
        info.nChans > 1,
        1,  // repeat count
        0   // channel 0
    );
}

}  // namespace voicerec
