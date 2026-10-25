// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <string>
#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/stream_buffer.h>
#include <SD.h>

#include "AudioConfig.h"
#include "DcBlocker.h"
#include "VadEngine.h"
#include "PreRollBuffer.h"
#include "Mp3Encoder.h"

namespace voicerec {

enum class PipelineState : uint8_t {
    Stopped = 0,
    Recording = 1,
    Paused = 2
};

class AudioPipeline {
public:
    static AudioPipeline& instance() {
        static AudioPipeline inst;
        return inst;
    }

    bool begin();

    bool startRecording(const std::string& path, int bitrateKbps = kDefaultBitrateKbps);
    void pauseRecording();
    void resumeRecording();
    void stopRecording();

    PipelineState state() const { return pipelineState_; }
    bool isRecording() const { return pipelineState_ == PipelineState::Recording; }
    bool isPaused() const { return pipelineState_ == PipelineState::Paused; }

    std::string currentFilePath() const { return currentPath_; }
    size_t currentFileSize() const { return totalWrittenBytes_; }
    uint32_t totalDurationMs() const { return totalDurationMs_; }
    uint32_t speechDurationMs() const { return speechDurationMs_; }

    // Live audio meters
    float currentRmsDbfs() const { return liveRmsDbfs_.load(); }
    float currentPeakDbfs() const { return livePeakDbfs_.load(); }
    VadState currentVadState() const { return liveVadState_.load(); }

    VadEngine& vad() { return vad_; }

private:
    AudioPipeline();
    ~AudioPipeline();

    static void captureTaskTrampoline(void* arg);
    static void writerTaskTrampoline(void* arg);

    void captureTaskLoop();
    void writerTaskLoop();

    void writeToStaging(const uint8_t* data, size_t len);
    void flushStagingToSd(bool forceSync);

    DcBlocker dcBlocker_;
    VadEngine vad_;
    PreRollBuffer preRoll_;
    Mp3Encoder encoder_;

    StreamBufferHandle_t streamBuffer_ = nullptr;
    TaskHandle_t captureTaskHandle_ = nullptr;
    TaskHandle_t writerTaskHandle_ = nullptr;

    std::atomic<PipelineState> pipelineState_{PipelineState::Stopped};
    std::atomic<bool> stopRequested_{false};
    std::atomic<bool> flushRequested_{false};

    std::atomic<float> liveRmsDbfs_{-90.0f};
    std::atomic<float> livePeakDbfs_{-90.0f};
    std::atomic<VadState> liveVadState_{VadState::Silence};

    std::string currentPath_;
    File activeFile_;
    size_t totalWrittenBytes_ = 0;
    uint32_t totalDurationMs_ = 0;
    uint32_t speechDurationMs_ = 0;

    // Staging buffer on Core 0
    uint8_t stagingBuffer_[kSdSectorBufferSize];
    size_t stagingCount_ = 0;
};

}  // namespace voicerec
