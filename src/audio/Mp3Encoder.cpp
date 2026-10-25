// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#include "Mp3Encoder.h"

extern "C" {
#include <layer3.h>
}

#include <esp_log.h>

namespace voicerec {

static const char* TAG = "Mp3Encoder";

Mp3Encoder::Mp3Encoder() = default;

Mp3Encoder::~Mp3Encoder() {
    end();
}

bool Mp3Encoder::begin(uint32_t sampleRate, int bitrateKbps) {
    end();

    sampleRate_ = sampleRate;
    bitrateKbps_ = bitrateKbps;

    shine_config_t config;
    shine_set_config_mpeg_defaults(&config.mpeg);

    config.wave.samplerate = static_cast<int>(sampleRate_);
    config.wave.channels = PCM_MONO;

    config.mpeg.mode = MONO;
    config.mpeg.bitr = bitrateKbps_;
    config.mpeg.emph = NONE;
    config.mpeg.copyright = 0;
    config.mpeg.original = 1;

    const int mpegVersion = shine_check_config(config.wave.samplerate, config.mpeg.bitr);
    if (mpegVersion < 0) {
        ESP_LOGE(TAG, "Invalid shine config: rate=%u bitr=%d", sampleRate_, bitrateKbps_);
        return false;
    }

    encoder_ = shine_initialise(&config);
    if (!encoder_) {
        ESP_LOGE(TAG, "Failed to initialize shine encoder (insufficient memory)");
        return false;
    }

    samplesPerPass_ = shine_samples_per_pass(encoder_);
    ESP_LOGI(TAG, "Shine initialized: %d Hz, %d kbps mono, %d samples/pass (MPEG version %d)",
             sampleRate_, bitrateKbps_, samplesPerPass_, mpegVersion);

    return true;
}

void Mp3Encoder::end() {
    if (encoder_) {
        shine_close(encoder_);
        encoder_ = nullptr;
    }
}

const uint8_t* Mp3Encoder::encodeFrame(const int16_t* samples, int* writtenBytes) {
    if (!encoder_ || !samples || !writtenBytes) {
        if (writtenBytes) *writtenBytes = 0;
        return nullptr;
    }

    // shine_encode_buffer_interleaved handles mono arrays properly
    unsigned char* output = shine_encode_buffer_interleaved(
        encoder_,
        const_cast<int16_t*>(samples),
        writtenBytes
    );

    return reinterpret_cast<const uint8_t*>(output);
}

const uint8_t* Mp3Encoder::flush(int* writtenBytes) {
    if (!encoder_ || !writtenBytes) {
        if (writtenBytes) *writtenBytes = 0;
        return nullptr;
    }

    unsigned char* output = shine_flush(encoder_, writtenBytes);
    return reinterpret_cast<const uint8_t*>(output);
}

}  // namespace voicerec
