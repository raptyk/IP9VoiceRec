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
constexpr uint8_t kEs8311Reset = 0x00;
constexpr uint8_t kEs8311ClockManager1 = 0x01;
constexpr uint8_t kEs8311ClockManager2 = 0x02;
constexpr uint8_t kEs8311AdcOsr = 0x03;
constexpr uint8_t kEs8311DacOsr = 0x04;
constexpr uint8_t kEs8311ClockDividers = 0x05;
constexpr uint8_t kEs8311SerialIn = 0x09;
constexpr uint8_t kEs8311System = 0x0D;
constexpr uint8_t kEs8311AnalogPower = 0x0E;
constexpr uint8_t kEs8311DacPower = 0x12;
constexpr uint8_t kEs8311OutputDrive = 0x13;
constexpr uint8_t kEs8311AdcPga = 0x14;
constexpr uint8_t kEs8311AdcVolume = 0x17;
constexpr uint8_t kEs8311AdcFilter = 0x1C;
constexpr uint8_t kEs8311DacMute = 0x31;
constexpr uint8_t kEs8311DacVolume = 0x32;
constexpr uint8_t kEs8311DacEqualizer = 0x37;
constexpr uint32_t kI2cFrequency = 100000;
constexpr uint32_t kCodecResetMs = 20;

// Silence that keeps the speaker's I2S clock running while the ES8311 is set up for playback,
// on a channel of its own so the player's channel is never touched.
constexpr size_t kClockSilenceSamples = 160;  // 10 ms
constexpr uint32_t kClockSilenceRepeat = 20;  // 200 ms, longer than the setup takes
constexpr uint8_t kClockChannel = 7;
constexpr uint32_t kClockStartMs = 20;

void writeEs8311(const uint8_t (*regs)[2], size_t count) {
    for (size_t i = 0; i < count; ++i) {
        M5.In_I2C.writeRegister8(kEs8311Address, regs[i][0], regs[i][1], kI2cFrequency);
    }
}

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
    if (adv_) {
        startAdvDac();
    }
    return true;
}

void AudioHardware::startAdvDac() {
    // M5Unified sends its short ES8311 playback setup before the speaker's I2S port runs, and
    // after a recording the codec is left with its state machine switched off by the microphone
    // side. Clocked from BCLK, the codec then only pops and its DAC stays silent. So, with the
    // clock running (a little silence on a channel of its own): reset the codec and set up the
    // whole playback path, as Espressif's ES8311 driver does - every register the DAC needs,
    // not only the ones a fresh power-up leaves wrong.
    static const int16_t kSilence[kClockSilenceSamples] = {};
    M5.Speaker.playRaw(kSilence, kClockSilenceSamples, kSampleRate, false, kClockSilenceRepeat, kClockChannel);
    vTaskDelay(pdMS_TO_TICKS(kClockStartMs));

    static constexpr uint8_t kReset[][2] = {
        {kEs8311Reset, 0x1F},  // reset every digital block
    };
    writeEs8311(kReset, 1);
    vTaskDelay(pdMS_TO_TICKS(kCodecResetMs));
    static constexpr uint8_t kSetup[][2] = {
        {kEs8311Reset, 0x00},           // out of reset
        {kEs8311Reset, 0x80},           // state machine on, slave
        {kEs8311ClockManager1, 0xBF},   // MCLK from BCLK, all clocks on
        {kEs8311ClockManager2, 0x18},   // pre-divider 1, pre-multiplier x8: BCLK 32 fs -> MCLK 256 fs
        {kEs8311AdcOsr, 0x10},          // single speed, ADC oversampling 16
        {kEs8311DacOsr, 0x10},          // DAC oversampling 16
        {kEs8311ClockDividers, 0x00},   // ADC / DAC clock dividers 1
        {kEs8311SerialIn, 0x0C},        // DAC input: I2S, 16-bit words (the slots M5Unified sends)
        {kEs8311System, 0x01},          // power up the analog circuitry
        {kEs8311AnalogPower, 0x02},     // analog references on
        {kEs8311DacPower, 0x00},        // power up the DAC
        {kEs8311OutputDrive, 0x10},     // enable the output driver
        {kEs8311AdcFilter, 0x6A},       // ADC equalizer bypass, DC offset removal
        {kEs8311DacEqualizer, 0x08},    // DAC equalizer bypass
        {kEs8311DacMute, 0x00},         // DAC not muted
        {kEs8311DacVolume, 0xBF},       // DAC digital volume 0 dB
    };
    writeEs8311(kSetup, sizeof(kSetup) / sizeof(kSetup[0]));
    // The analog power-up is taken over only from a write the codec sees with the clock
    // running (as on the capture side): repeat it while the reference voltages settle.
    static constexpr uint8_t kPowerUp[][2] = {
        {kEs8311System, 0x01},
    };
    for (int attempt = 0; attempt < 2; ++attempt) {
        vTaskDelay(pdMS_TO_TICKS(30));
        writeEs8311(kPowerUp, 1);
    }
}

void AudioHardware::stopAdvDac() {
    // Mute and power down the DAC while its clock still runs: once the I2S port stops, a DAC
    // left on without a clock hisses through the speaker.
    static constexpr uint8_t kPowerDown[][2] = {
        {kEs8311DacMute, 0x60},      // mute
        {kEs8311DacVolume, 0x00},    // digital volume off
        {kEs8311OutputDrive, 0x00},  // output driver off
        {kEs8311DacPower, 0x02},     // DAC off
    };
    writeEs8311(kPowerDown, sizeof(kPowerDown) / sizeof(kPowerDown[0]));
    vTaskDelay(pdMS_TO_TICKS(kCodecResetMs));
}

void AudioHardware::stopSpeaker() {
    if (owner_ != AudioOwner::Speaker) {
        return;
    }
    M5.Speaker.stop();
    if (adv_) {
        stopAdvDac();
    }
    M5.Speaker.end();
    release(AudioOwner::Speaker);
}

void AudioHardware::setSpeakerVolume(uint8_t volume) {
    M5.Speaker.setVolume(volume);
}

}  // namespace voicerec
