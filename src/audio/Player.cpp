// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#include "Player.h"

#include <M5Cardputer.h>
#include <string.h>

#include "hw/AudioHardware.h"
#include "storage/SdStorage.h"

namespace voicerec {

namespace {
constexpr uint32_t kTaskStack = 8192;
constexpr UBaseType_t kTaskPriority = 4;
constexpr size_t kQueueDepth = 6;
constexpr uint8_t kSpeakerChannel = 0;
constexpr uint32_t kSeekEndMarginMs = 500;
}  // namespace

Player& Player::instance() {
    static Player player;
    return player;
}

void Player::begin() {
    if (commands_ != nullptr) {
        return;
    }
    decoder_.setDataCallback(onPcm);
    commands_ = xQueueCreate(kQueueDepth, sizeof(Command));
    xTaskCreatePinnedToCore(taskEntry, "player", kTaskStack, this, kTaskPriority, &task_, 0);
}

// ---------------------------------------------------------------- UI side

void Player::send(Op op, int32_t value, const char* path) {
    Command command{};
    command.op = op;
    command.value = value;
    if (path != nullptr) {
        strlcpy(command.path, path, sizeof(command.path));
    }
    xQueueSend(commands_, &command, pdMS_TO_TICKS(100));
}

void Player::play(const String& path, uint32_t startMs) {
    interrupt_ = true;
    send(Op::Play, static_cast<int32_t>(startMs), path.c_str());
}

void Player::togglePause() {
    send(Op::TogglePause);
}

void Player::stop() {
    interrupt_ = true;
    send(Op::Stop);
}

void Player::seekBy(int32_t deltaMs) {
    interrupt_ = true;
    send(Op::Seek, deltaMs);
}

void Player::setVolume(uint8_t volume) {
    volume_ = volume;
    if (AudioHardware::instance().owner() == AudioOwner::Speaker) {
        AudioHardware::instance().setSpeakerVolume(volume);
    }
}

// ---------------------------------------------------------------- player task

void Player::taskEntry(void* arg) {
    static_cast<Player*>(arg)->loop();
}

void Player::loop() {
    for (;;) {
        Command command;
        // Block while there is nothing to play; poll between decode steps otherwise.
        TickType_t wait = state_ == PlayerState::Playing ? 0 : portMAX_DELAY;
        while (xQueueReceive(commands_, &command, wait) == pdTRUE) {
            handle(command);
            wait = 0;
        }
        if (state_ == PlayerState::Playing) {
            pump();
        }
    }
}

void Player::handle(const Command& command) {
    interrupt_ = false;
    switch (command.op) {
        case Op::Play:
            open(command.path, static_cast<uint32_t>(command.value));
            break;
        case Op::TogglePause:
            if (state_ == PlayerState::Playing) {
                state_ = PlayerState::Paused;
            } else if (state_ == PlayerState::Paused) {
                state_ = PlayerState::Playing;
            }
            break;
        case Op::Stop:
            close(PlayerState::Stopped);
            break;
        case Op::Seek:
            if (file_) {
                const int64_t target = static_cast<int64_t>(positionMs_.load()) + command.value;
                const int64_t last = durationMs_ > kSeekEndMarginMs ? durationMs_ - kSeekEndMarginMs : 0;
                seekTo(static_cast<uint32_t>(target < 0 ? 0 : (target > last ? last : target)));
            }
            break;
    }
}

void Player::open(const char* path, uint32_t startMs) {
    close(PlayerState::Stopped);
    file_ = SdStorage::instance().openForRead(path);
    if (!file_) {
        log_e("Cannot open %s", path);
        state_ = PlayerState::Error;
        return;
    }
    info_ = readMp3Info(file_);
    if (!info_.valid) {
        log_e("Not an MP3 file: %s", path);
        file_.close();
        state_ = PlayerState::Error;
        return;
    }
    durationMs_ = info_.durationMs;
    if (!AudioHardware::instance().startSpeaker(volume_)) {
        file_.close();
        state_ = PlayerState::Error;
        return;
    }
    seekTo(startMs < info_.durationMs ? startMs : 0);
    state_ = PlayerState::Playing;
}

void Player::seekTo(uint32_t ms) {
    M5.Speaker.stop(kSpeakerChannel);  // drop what is queued from the old position
    file_.seek(info_.offsetForMs(ms));
    decoder_.end();
    decoder_.begin();
    baseMs_ = ms;
    decodedSamples_ = 0;
    positionMs_ = ms;
}

void Player::pump() {
    const size_t length = file_.read(readBuffer_, sizeof(readBuffer_));
    if (length > 0) {
        decoder_.write(readBuffer_, length);
        return;
    }
    // End of file: let the queued audio play out.
    while (M5.Speaker.isPlaying(kSpeakerChannel) && !interrupt_) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    if (!interrupt_) {
        positionMs_ = durationMs_.load();
        close(PlayerState::Finished);
    }
}

void Player::onPcm(MP3FrameInfo& info, short* pcm, size_t samples, void* ref) {
    (void)ref;
    Player::instance().output(info, pcm, samples);
}

void Player::output(const MP3FrameInfo& info, const short* pcm, size_t samples) {
    if (samples == 0 || samples > kMaxPcmSamples || info.samprate <= 0) {
        return;
    }
    // The speaker keeps two buffers queued and plays from our memory, so the decoder's own
    // buffer (reused for the next frame) cannot be handed over: copy into the third one,
    // which is free as soon as fewer than two are queued.
    while (M5.Speaker.isPlaying(kSpeakerChannel) >= 2) {
        if (interrupt_ || state_ != PlayerState::Playing) {
            return;
        }
        vTaskDelay(1);
    }
    int16_t* buffer = out_[outIndex_];
    outIndex_ = (outIndex_ + 1) % 3;
    memcpy(buffer, pcm, samples * sizeof(int16_t));
    const bool stereo = info.nChans > 1;
    M5.Speaker.playRaw(buffer, samples, info.samprate, stereo, 1, kSpeakerChannel, false);

    decodedSamples_ += stereo ? samples / 2 : samples;
    positionMs_ = baseMs_ + static_cast<uint32_t>(decodedSamples_ * 1000 / info.samprate);
}

void Player::close(PlayerState endState) {
    if (file_) {
        file_.close();
    }
    decoder_.end();
    AudioHardware::instance().stopSpeaker();
    state_ = endState;
}

}  // namespace voicerec
