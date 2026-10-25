// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#pragma once

#include <Arduino.h>
#include <FS.h>
#include <MP3DecoderHelix.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include <atomic>

#include "audio/Mp3Info.h"

namespace voicerec {

enum class PlayerState : uint8_t { Stopped, Playing, Paused, Finished, Error };

// MP3 playback through the built-in speaker. One task (core 0) owns the file, the Helix
// decoder and the speaker; the UI only sends commands, so stopping or seeking can never
// pull the decoder away from under a running decode.
class Player {
public:
    static Player& instance();

    void begin();

    void play(const String& path, uint32_t startMs = 0);
    void togglePause();
    void stop();
    void seekBy(int32_t deltaMs);
    // 0..255
    void setVolume(uint8_t volume);

    PlayerState state() const { return state_.load(); }
    bool active() const { return state() == PlayerState::Playing || state() == PlayerState::Paused; }
    uint32_t positionMs() const { return positionMs_.load(); }
    uint32_t durationMs() const { return durationMs_.load(); }

private:
    enum class Op : uint8_t { Play, TogglePause, Stop, Seek };

    struct Command {
        Op op;
        int32_t value;
        char path[96];
    };

    static constexpr size_t kMaxPcmSamples = 1152 * 2;  // MPEG-1 stereo frame

    Player() = default;

    static void taskEntry(void* arg);
    static void onPcm(MP3FrameInfo& info, short* pcm, size_t samples, void* ref);
    void loop();
    void handle(const Command& command);
    void open(const char* path, uint32_t startMs);
    void seekTo(uint32_t ms);
    void pump();
    void output(const MP3FrameInfo& info, const short* pcm, size_t samples);
    void close(PlayerState endState);
    void send(Op op, int32_t value = 0, const char* path = nullptr);

    TaskHandle_t task_ = nullptr;
    QueueHandle_t commands_ = nullptr;

    std::atomic<PlayerState> state_{PlayerState::Stopped};
    std::atomic<bool> interrupt_{false};  // set by the UI to cut a waiting output short
    std::atomic<uint32_t> positionMs_{0};
    std::atomic<uint32_t> durationMs_{0};
    std::atomic<uint8_t> volume_{160};

    // Player task only.
    File file_;
    Mp3Info info_;
    libhelix::MP3DecoderHelix decoder_;
    uint32_t baseMs_ = 0;
    uint64_t decodedSamples_ = 0;  // per channel, since baseMs_
    int16_t out_[3][kMaxPcmSamples] = {};
    size_t outIndex_ = 0;
    uint8_t readBuffer_[512] = {};
};

}  // namespace voicerec
