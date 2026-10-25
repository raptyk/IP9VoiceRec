// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#include "AudioHardware.h"

#include <M5Cardputer.h>

#include "audio/AudioConfig.h"

namespace voicerec {

namespace {
constexpr uint32_t kClaimTimeoutMs = 600;
constexpr uint32_t kPinSettleMs = 15;  // let the shared clock line settle between the two drivers

// Cardputer ADV: ES8311 codec on the internal I2C bus.
constexpr uint8_t kEs8311Address = 0x18;
constexpr uint8_t kEs8311System = 0x0D;
constexpr uint8_t kEs8311AdcPga = 0x14;
constexpr uint8_t kEs8311AdcVolume = 0x17;
constexpr uint8_t kEs8311AdcFilter = 0x1C;
constexpr uint32_t kI2cFrequency = 100000;

// Base gain of each microphone path (M5Unified magnification: output = mean * magnification / 2).
constexpr uint8_t kPdmMagnification = 16;  // Cardputer SPM1423 PDM MEMS: x8
constexpr uint8_t kAdvMagnification = 2;   // ADV: x1, the gain is in the codec's analog PGA
}  // namespace

AudioHardware& AudioHardware::instance() {
    static AudioHardware hardware;
    return hardware;
}

void AudioHardware::begin(bool cardputerAdv) {
    adv_ = cardputerAdv;
    lock_ = xSemaphoreCreateMutex();
    M5.Speaker.end();
    M5.Mic.end();
    owner_ = AudioOwner::None;
}

bool AudioHardware::claim(AudioOwner owner) {
    const uint32_t started = millis();
    for (;;) {
        xSemaphoreTake(lock_, portMAX_DELAY);
        if (owner_ == AudioOwner::None) {
            owner_ = owner;
            xSemaphoreGive(lock_);
            return true;
        }
        xSemaphoreGive(lock_);
        if (millis() - started > kClaimTimeoutMs) {
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void AudioHardware::release(AudioOwner owner) {
    xSemaphoreTake(lock_, portMAX_DELAY);
    if (owner_ == owner) {
        owner_ = AudioOwner::None;
    }
    xSemaphoreGive(lock_);
}

bool AudioHardware::startMic() {
    if (!claim(AudioOwner::Mic)) {
        log_w("Microphone busy: speaker still running");
        return false;
    }
    vTaskDelay(pdMS_TO_TICKS(kPinSettleMs));

    auto config = M5.Mic.config();
    config.sample_rate = kSampleRate;
    config.magnification = adv_ ? kAdvMagnification : kPdmMagnification;
    config.noise_filter_level = 0;
    // The capture task of M5Unified must never starve: above our processing task, on core 1
    // (core 0 runs the MP3 encoder and the SD card writes), with 12 x 128 frames of DMA slack.
    config.task_priority = 6;
    config.task_pinned_core = 1;
    config.dma_buf_count = 12;
    M5.Mic.config(config);
    if (!M5.Mic.begin()) {
        log_e("M5.Mic.begin() failed");
        release(AudioOwner::Mic);
        return false;
    }
    if (adv_) {
        tuneAdvCodec();
    }
    return true;
}

void AudioHardware::tuneAdvCodec() {
    // M5Unified leaves the ES8311 analog PGA at its minimum (0 dB), which together with the
    // x1 magnification makes the ADV microphone far too quiet. +21 dB is where the codec's SNR
    // stops improving (M5Stack uses the same value for the StopWatch); the fine gain is done
    // in software. The SYSTEM write (re)arms the capture path, which the codec only accepts
    // while the I2S clock runs - it does now, M5.Mic.begin() has started the port.
    static constexpr uint8_t kSetup[][2] = {
        {kEs8311System, 0x01},     // power up the analog circuitry
        {kEs8311AdcPga, 0x17},     // Mic1p-Mic1n, PGA +21 dB (3 dB per step)
        {kEs8311AdcVolume, 0xBF},  // ADC digital volume 0 dB
        {kEs8311AdcFilter, 0x6A},  // equalizer bypass, digital DC offset removal
    };
    for (int attempt = 0; attempt < 3; ++attempt) {
        vTaskDelay(pdMS_TO_TICKS(30));
        for (const auto& reg : kSetup) {
            M5.In_I2C.writeRegister8(kEs8311Address, reg[0], reg[1], kI2cFrequency);
        }
    }
}

void AudioHardware::stopMic() {
    if (owner_ != AudioOwner::Mic) {
        return;
    }
    M5.Mic.end();
    release(AudioOwner::Mic);
}

bool AudioHardware::startSpeaker(uint8_t volume) {
    if (!claim(AudioOwner::Speaker)) {
        log_w("Speaker busy: microphone still running");
        return false;
    }
    vTaskDelay(pdMS_TO_TICKS(kPinSettleMs));
    if (!M5.Speaker.begin()) {
        log_e("M5.Speaker.begin() failed");
        release(AudioOwner::Speaker);
        return false;
    }
    M5.Speaker.setVolume(volume);
    return true;
}

void AudioHardware::stopSpeaker() {
    if (owner_ != AudioOwner::Speaker) {
        return;
    }
    M5.Speaker.stop();
    M5.Speaker.end();
    release(AudioOwner::Speaker);
}

void AudioHardware::setSpeakerVolume(uint8_t volume) {
    M5.Speaker.setVolume(volume);
}

}  // namespace voicerec
