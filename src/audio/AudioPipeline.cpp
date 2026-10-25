// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#include "AudioPipeline.h"
#include <cstring>
#include <M5Cardputer.h>
#include <esp_log.h>
#include "../hw/AudioModeManager.h"
#include "../storage/SdStorage.h"

namespace voicerec {

static const char* TAG = "AudioPipeline";

AudioPipeline::AudioPipeline() = default;

AudioPipeline::~AudioPipeline() {
    stopRecording();
    if (streamBuffer_) {
        vStreamBufferDelete(streamBuffer_);
        streamBuffer_ = nullptr;
    }
}

bool AudioPipeline::begin() {
    if (!streamBuffer_) {
        streamBuffer_ = xStreamBufferCreate(kAudioStreamBufferBytes, kAudioStreamTriggerBytes);
        if (!streamBuffer_) {
            ESP_LOGE(TAG, "Failed to create FreeRTOS StreamBuffer");
            return false;
        }
    }

    if (!captureTaskHandle_) {
        xTaskCreatePinnedToCore(
            captureTaskTrampoline,
            "AudioCapture",
            4096,
            this,
            6, // High priority
            &captureTaskHandle_,
            1  // Pinned to Core 1
        );
    }

    if (!writerTaskHandle_) {
        xTaskCreatePinnedToCore(
            writerTaskTrampoline,
            "Mp3Writer",
            8192,
            this,
            3, // Medium priority
            &writerTaskHandle_,
            0  // Pinned to Core 0
        );
    }

    ESP_LOGI(TAG, "AudioPipeline initialized on Core 1 and Core 0");
    return true;
}

bool AudioPipeline::startRecording(const std::string& path, int bitrateKbps) {
    if (pipelineState_ != PipelineState::Stopped) {
        stopRecording();
    }

    if (!SdStorage::instance().ensureMounted()) {
        ESP_LOGE(TAG, "Cannot start recording: SD not mounted");
        return false;
    }

    // Switch hardware to Record mode (safely turns off speaker first)
    if (!AudioModeManager::instance().setMode(AudioMode::Record)) {
        ESP_LOGE(TAG, "Failed to switch hardware to Record mode");
        return false;
    }

    // Reset DSP states
    dcBlocker_.reset();
    vad_.reset();
    preRoll_.reset();

    // Initialize MP3 encoder
    if (!encoder_.begin(kSampleRate, bitrateKbps)) {
        ESP_LOGE(TAG, "Failed to initialize MP3 encoder");
        AudioModeManager::instance().setMode(AudioMode::Idle);
        return false;
    }

    // Open file on SD
    currentPath_ = path;
    activeFile_ = SdStorage::instance().openFile(currentPath_.c_str(), FILE_WRITE);
    if (!activeFile_) {
        ESP_LOGE(TAG, "Failed to open destination file %s", currentPath_.c_str());
        encoder_.end();
        AudioModeManager::instance().setMode(AudioMode::Idle);
        return false;
    }

    stagingCount_ = 0;
    totalWrittenBytes_ = 0;
    totalDurationMs_ = 0;
    speechDurationMs_ = 0;
    stopRequested_.store(false);
    flushRequested_.store(false);

    // Clear any residual data in stream buffer
    xStreamBufferReset(streamBuffer_);

    pipelineState_.store(PipelineState::Recording);
    ESP_LOGI(TAG, "Recording started -> %s (%d kbps)", currentPath_.c_str(), bitrateKbps);
    return true;
}

void AudioPipeline::pauseRecording() {
    if (pipelineState_ == PipelineState::Recording) {
        pipelineState_.store(PipelineState::Paused);
        flushRequested_.store(true);
        ESP_LOGI(TAG, "Recording paused");
    }
}

void AudioPipeline::resumeRecording() {
    if (pipelineState_ == PipelineState::Paused) {
        pipelineState_.store(PipelineState::Recording);
        ESP_LOGI(TAG, "Recording resumed");
    }
}

void AudioPipeline::stopRecording() {
    if (pipelineState_ == PipelineState::Stopped) {
        return;
    }

    ESP_LOGI(TAG, "Stopping recording...");
    pipelineState_.store(PipelineState::Stopped);
    stopRequested_.store(true);

    // Give writer task up to 1 second to drain queue and flush SD
    uint32_t waitStart = millis();
    while (stopRequested_.load() && (millis() - waitStart < 1200)) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    // Switch hardware back to Idle (saves battery)
    AudioModeManager::instance().setMode(AudioMode::Idle);
    ESP_LOGI(TAG, "Recording stopped. Total bytes: %u, duration: %u ms",
             totalWrittenBytes_, totalDurationMs_);
}

void AudioPipeline::captureTaskTrampoline(void* arg) {
    static_cast<AudioPipeline*>(arg)->captureTaskLoop();
}

void AudioPipeline::captureTaskLoop() {
    int16_t frame[kSamplesPerFrame];

    while (true) {
        // Record audio when in Record mode or Calibrating
        if (AudioModeManager::instance().currentMode() == AudioMode::Record) {
            if (M5.Mic.isEnabled() && M5.Mic.record(frame, kSamplesPerFrame, kSampleRate)) {
                // 1. High-Pass DC filter
                dcBlocker_.process(frame, kSamplesPerFrame);

                // 2. VAD processing
                VadFrameResult vadRes = vad_.processFrame(frame, kSamplesPerFrame);

                // Update atomic live meters for UI
                liveRmsDbfs_.store(vadRes.rmsDbfs);
                livePeakDbfs_.store(vadRes.peakDbfs);
                liveVadState_.store(vadRes.state);

                // Calibration check
                if (vad_.isCalibrating()) {
                    vad_.addCalibrationFrame(frame, kSamplesPerFrame);
                }

                // Audio recording pipeline
                if (pipelineState_.load() == PipelineState::Recording) {
                    totalDurationMs_ += static_cast<uint32_t>(kFrameDurationMs);

                    if (vadRes.isWriting()) {
                        speechDurationMs_ += static_cast<uint32_t>(kFrameDurationMs);

                        // If voice triggered from silence, first flush pre-roll history!
                        if (vadRes.speechTriggered) {
                            preRoll_.flush([this](const int16_t* hist, size_t count) {
                                if (streamBuffer_) {
                                    xStreamBufferSend(streamBuffer_, hist, count * sizeof(int16_t), 0);
                                }
                            });
                        }

                        // Send speech frame to stream buffer
                        if (streamBuffer_) {
                            xStreamBufferSend(streamBuffer_, frame, sizeof(frame), 0);
                        }
                    } else {
                        // In silence: store frame into pre-roll ring buffer
                        preRoll_.pushFrame(frame);

                        // If we just entered silence, request safe file flush
                        flushRequested_.store(true);
                    }
                }
            } else {
                vTaskDelay(pdMS_TO_TICKS(2));
            }
        } else {
            // Idle or playback mode
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }
}

void AudioPipeline::writerTaskTrampoline(void* arg) {
    static_cast<AudioPipeline*>(arg)->writerTaskLoop();
}

void AudioPipeline::writerTaskLoop() {
    int16_t pcmFrame[kSamplesPerFrame];

    while (true) {
        if (!streamBuffer_) {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        // Blocking read with 50 ms timeout
        size_t received = xStreamBufferReceive(
            streamBuffer_,
            pcmFrame,
            sizeof(pcmFrame),
            pdMS_TO_TICKS(50)
        );

        if (received == sizeof(pcmFrame) && encoder_.isOpen() && activeFile_) {
            // Encode frame to MP3
            int writtenBytes = 0;
            const uint8_t* mp3Data = encoder_.encodeFrame(pcmFrame, &writtenBytes);
            if (mp3Data && writtenBytes > 0) {
                writeToStaging(mp3Data, writtenBytes);
            }
        }

        // Check if SD flush requested during VAS pause
        if (flushRequested_.load()) {
            flushRequested_.store(false);
            flushStagingToSd(true);
        }

        // Check if stop requested
        if (stopRequested_.load()) {
            // Drain any remaining frames in stream buffer
            while (xStreamBufferReceive(streamBuffer_, pcmFrame, sizeof(pcmFrame), 0) == sizeof(pcmFrame)) {
                if (encoder_.isOpen()) {
                    int written = 0;
                    const uint8_t* mp3 = encoder_.encodeFrame(pcmFrame, &written);
                    if (mp3 && written > 0) {
                        writeToStaging(mp3, written);
                    }
                }
            }

            // Flush encoder end bytes
            if (encoder_.isOpen()) {
                int flushBytes = 0;
                const uint8_t* tail = encoder_.flush(&flushBytes);
                if (tail && flushBytes > 0) {
                    writeToStaging(tail, flushBytes);
                }
                encoder_.end();
            }

            // Flush all staging data to SD and close file
            flushStagingToSd(true);
            if (activeFile_) {
                activeFile_.close();
            }

            stopRequested_.store(false);
        }

        // Feed IDLE0 Task Watchdog
        vTaskDelay(1);
    }
}

void AudioPipeline::writeToStaging(const uint8_t* data, size_t len) {
    if (!data || len == 0) return;

    size_t offset = 0;
    while (offset < len) {
        size_t toCopy = std::min(len - offset, sizeof(stagingBuffer_) - stagingCount_);
        std::memcpy(stagingBuffer_ + stagingCount_, data + offset, toCopy);
        stagingCount_ += toCopy;
        offset += toCopy;

        if (stagingCount_ >= kSdWriteBlockThreshold) {
            flushStagingToSd(false);
        }
    }
}

void AudioPipeline::flushStagingToSd(bool forceSync) {
    if (!activeFile_ || stagingCount_ == 0) {
        if (forceSync && activeFile_) {
            activeFile_.flush();
        }
        return;
    }

    size_t toWrite = forceSync ? stagingCount_ : (stagingCount_ / 512) * 512;
    if (toWrite > 0) {
        size_t written = activeFile_.write(stagingBuffer_, toWrite);
        totalWrittenBytes_ += written;

        // Shift any remainder in staging buffer
        size_t remainder = stagingCount_ - toWrite;
        if (remainder > 0) {
            std::memmove(stagingBuffer_, stagingBuffer_ + toWrite, remainder);
        }
        stagingCount_ = remainder;
    }

    if (forceSync) {
        activeFile_.flush();
    }
}

}  // namespace voicerec
