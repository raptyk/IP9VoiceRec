// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <stdint.h>

#include <atomic>

namespace voicerec {

enum class AudioOwner : uint8_t { None, Mic, Speaker };

// Microphone and speaker share the I2S clock pin GPIO43 (and GPIO41 on the ADV), so only
// one of them may run at a time. Each side is started and stopped by the task that uses it
// (capture task: microphone, player task: speaker); a start waits briefly for the other
// side to be released and fails instead of fighting over the pins.
class AudioHardware {
public:
    static AudioHardware& instance();

    // Called once at boot, before the audio tasks run: both sides off.
    void begin(bool cardputerAdv);

    bool startMic();
    void stopMic();

    bool startSpeaker(uint8_t volume);
    void stopSpeaker();
    void setSpeakerVolume(uint8_t volume);

    AudioOwner owner() const { return owner_.load(); }

private:
    AudioHardware() = default;

    bool claim(AudioOwner owner);
    void release(AudioOwner owner);
    void tuneAdvCodec();
    void startAdvDac();
    void stopAdvDac();

    SemaphoreHandle_t lock_ = nullptr;
    std::atomic<AudioOwner> owner_{AudioOwner::None};
    bool adv_ = false;
};

}  // namespace voicerec
