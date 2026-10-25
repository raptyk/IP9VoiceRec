// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#pragma once

#include <stdint.h>
#include <string>
#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <SD.h>
#include <MP3DecoderHelix.h>

namespace voicerec {

enum class PlayerState : uint8_t {
    Stopped = 0,
    Playing = 1,
    Paused = 2
};

class AudioPlayer {
public:
    static AudioPlayer& instance() {
        static AudioPlayer inst;
        return inst;
    }

    bool begin();

    bool play(const std::string& filePath);
    void pause();
    void resume();
    void stop();

    PlayerState state() const { return playerState_.load(); }
    bool isPlaying() const { return playerState_.load() == PlayerState::Playing; }
    bool isPaused() const { return playerState_.load() == PlayerState::Paused; }

    std::string currentPath() const { return currentPath_; }
    uint32_t currentPositionSec() const { return currentPositionSec_.load(); }
    uint32_t totalDurationSec() const { return totalDurationSec_.load(); }

    void setVolume(uint8_t vol);
    uint8_t volume() const { return volume_; }

private:
    AudioPlayer();
    ~AudioPlayer();

    static void playbackTaskTrampoline(void* arg);
    void playbackTaskLoop();

    static void onDecodedPcm(MP3FrameInfo& info, short* pcmBuffer, size_t len, void* ref);

    TaskHandle_t playerTaskHandle_ = nullptr;
    std::atomic<PlayerState> playerState_{PlayerState::Stopped};
    std::atomic<bool> stopRequested_{false};

    std::string currentPath_;
    File audioFile_;
    size_t fileSizeBytes_ = 0;
    std::atomic<uint32_t> currentPositionSec_{0};
    std::atomic<uint32_t> totalDurationSec_{0};

    uint8_t volume_ = 180;
    libhelix::MP3DecoderHelix decoder_;
};

}  // namespace voicerec
