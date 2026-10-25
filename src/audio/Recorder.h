// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#pragma once

#include <Arduino.h>
#include <FS.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include <atomic>

#include "audio/AudioConfig.h"
#include "audio/DcBlocker.h"
#include "audio/Mp3Encoder.h"
#include "audio/PreRollBuffer.h"
#include "audio/VoiceDetector.h"

namespace voicerec {

enum class RecorderState : uint8_t {
    Idle,       // no recording (the microphone may still run for the level meter)
    Starting,   // file open, microphone warming up
    Recording,  // frames are processed (written while a voice is heard or voice activation is off)
    Saving      // stopped, the writer finishes the MP3 file
};

enum class StartResult : uint8_t { Ok, Busy, FileError, EncoderError };

struct RecorderConfig {
    int gainDb = 12;
    bool voiceActivation = true;
    uint8_t sensitivity = 3;
    uint16_t holdMs = 1500;
    float noiseFloorDbfs = -60.0f;  // start value of the adaptive noise floor
};

// Result of the last finished recording.
struct RecordingSummary {
    String path;
    uint32_t audioMs = 0;  // length of the MP3 file
    uint32_t bytes = 0;
    uint32_t droppedFrames = 0;
    bool writeError = false;
};

// Microphone capture, voice activation and MP3 writing, in two FreeRTOS tasks:
//
//  - capture task (core 1): the only user of M5.Mic. It keeps two buffers queued in the
//    microphone driver and processes the third, so capture is gapless and nothing ever waits
//    on another task. Per 36 ms frame: DC blocker -> gain + limiter -> level -> voice
//    detector -> pre-roll / writer queue.
//  - writer task (core 0): encodes the queued frames to MP3 and writes 4 KB blocks to the card.
//
// The UI never blocks: start() opens the file, stop() only asks for the end; the capture
// task hands a Close to the writer, which finishes the file and returns to Idle.
class Recorder {
public:
    static Recorder& instance();

    void begin();

    // Live level meter without recording (Mic check). Ignored while recording.
    void startMonitor();
    void stopMonitor();

    StartResult start(const String& path, uint8_t bitrateKbps);
    void stop();
    void setPaused(bool paused);
    bool paused() const { return paused_.load(); }

    // Settings can change at any time; they apply from the next frame.
    void configure(const RecorderConfig& config);
    // Restarts the adaptive noise floor from the configured value (after a calibration).
    void restartNoiseTracking() { noiseRestart_ = true; }

    RecorderState state() const { return state_.load(); }
    bool active() const { return state() != RecorderState::Idle; }
    bool micRunning() const { return micRunning_.load(); }
    bool micWarmingUp() const { return warmup_.load(); }
    bool micFailed() const { return micFailed_.load(); }
    bool writeError() const { return writeError_.load(); }

    // Live values of the last frame.
    float levelDbfs() const { return level_.load(); }
    float peakDbfs() const { return peak_.load(); }
    bool clipping() const { return millis() - clippedAt_.load() < 300; }
    VoiceState voiceState() const { return voice_.load(); }
    float noiseFloorDbfs() const { return noise_.load(); }
    float triggerDbfs() const { return trigger_.load(); }

    // Current recording.
    uint32_t elapsedMs() const { return framesSeen_.load() * kFrameMs; }
    uint32_t audioMs() const { return framesWritten_.load() * kFrameMs; }
    uint32_t bytesWritten() const { return bytesWritten_.load(); }
    uint32_t droppedFrames() const { return droppedFrames_.load(); }
    const String& path() const { return path_; }
    RecordingSummary lastSummary() const { return summary_; }

    // Calibration: records the levels of the next `frames` frames (max kMaxLevelFrames).
    static constexpr size_t kMaxLevelFrames = 128;
    void captureLevels(size_t frames);
    bool levelsReady() const { return levelCount_.load() >= levelTarget_.load(); }
    // 0..1 of the requested frames captured so far.
    float levelProgress() const {
        const size_t target = levelTarget_.load();
        return target == 0 ? 1.0f : static_cast<float>(levelCount_.load()) / target;
    }
    // Copies the captured levels; returns how many.
    size_t copyLevels(float* out, size_t capacity) const;

private:
    enum class Mode : uint8_t { Off, Monitor, Record };
    enum class WriterOp : uint8_t { Frame, Sync, Close };

    struct WriterMessage {
        WriterOp op;
        int16_t pcm[kSamplesPerFrame];
    };

    Recorder() = default;

    static void captureTaskEntry(void* arg);
    static void writerTaskEntry(void* arg);
    void captureLoop();
    void writerLoop();

    bool startMic();
    void stopMic();
    void applyConfig();
    void processFrame(int16_t* frame);
    void handleRecording(const int16_t* frame, const VoiceDecision& decision);
    void sendFrame(const int16_t* frame);
    void sendControl(WriterOp op);
    void closeSession();

    void writeEncoded(const uint8_t* data, size_t length);
    void writeStaging(bool all);
    void finishFile();

    TaskHandle_t captureTask_ = nullptr;
    TaskHandle_t writerTask_ = nullptr;
    QueueHandle_t writerQueue_ = nullptr;

    std::atomic<Mode> wanted_{Mode::Off};
    std::atomic<RecorderState> state_{RecorderState::Idle};
    std::atomic<bool> sessionOpen_{false};
    std::atomic<bool> paused_{false};
    std::atomic<bool> micRunning_{false};
    std::atomic<bool> warmup_{false};
    std::atomic<bool> micFailed_{false};
    std::atomic<bool> writeError_{false};
    std::atomic<bool> syncRequested_{false};

    // Configuration (written by the UI, applied by the capture task).
    std::atomic<int> gainDb_{12};
    std::atomic<bool> voiceActivation_{true};
    std::atomic<uint8_t> sensitivity_{3};
    std::atomic<uint16_t> holdMs_{1500};
    std::atomic<float> startNoise_{-60.0f};
    std::atomic<bool> configChanged_{true};
    std::atomic<bool> noiseRestart_{false};

    // Live values (written by the capture task).
    std::atomic<float> level_{kSilenceDbfs};
    std::atomic<float> peak_{kSilenceDbfs};
    std::atomic<uint32_t> clippedAt_{0};
    std::atomic<VoiceState> voice_{VoiceState::Silence};
    std::atomic<float> noise_{-60.0f};
    std::atomic<float> trigger_{-50.0f};
    std::atomic<uint32_t> framesSeen_{0};
    std::atomic<uint32_t> framesWritten_{0};
    std::atomic<uint32_t> droppedFrames_{0};
    std::atomic<uint32_t> bytesWritten_{0};

    // Capture task only.
    int16_t buffers_[3][kSamplesPerFrame] = {};
    uint32_t frameIndex_ = 0;
    uint32_t settleFrames_ = 0;
    uint32_t silentStartFrames_ = 0;
    bool sessionStarted_ = false;
    static constexpr int kNoGain = -1000;
    float gain_ = 1.0f;
    int appliedGainDb_ = kNoGain;
    DcBlocker dcBlocker_;
    VoiceDetector detector_;
    PreRollBuffer preRoll_;
    WriterMessage outgoing_;

    // Calibration levels.
    float levels_[kMaxLevelFrames] = {};
    std::atomic<size_t> levelTarget_{0};
    std::atomic<size_t> levelCount_{0};

    // Session (set up by start() before the hand-over to the tasks, finished by the writer).
    String path_;
    File file_;
    Mp3Encoder encoder_;
    RecordingSummary summary_;

    // Writer task only.
    WriterMessage incoming_;
    uint8_t staging_[kSdWriteBlock * 2] = {};
    size_t stagingCount_ = 0;
    uint32_t lastSyncAt_ = 0;
};

}  // namespace voicerec
