// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#include "Mp3Info.h"

namespace voicerec {

namespace {

// Layer III bitrates (kbit/s) by bitrate index.
constexpr uint16_t kBitratesV1[16] = {0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0};
constexpr uint16_t kBitratesV2[16] = {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0};
constexpr uint32_t kSampleRatesV1[3] = {44100, 48000, 32000};
constexpr size_t kScanBytes = 1024;  // a frame header must appear within this many bytes

bool parseHeader(const uint8_t* h, Mp3Info& info) {
    if (h[0] != 0xFF || (h[1] & 0xE0) != 0xE0) {
        return false;
    }
    const uint8_t version = (h[1] >> 3) & 0x03;  // 0 = MPEG 2.5, 2 = MPEG 2, 3 = MPEG 1
    const uint8_t layer = (h[1] >> 1) & 0x03;    // 1 = Layer III
    const uint8_t bitrateIndex = (h[2] >> 4) & 0x0F;
    const uint8_t rateIndex = (h[2] >> 2) & 0x03;
    if (version == 1 || layer != 1 || rateIndex == 3) {
        return false;
    }
    const uint16_t bitrate = (version == 3 ? kBitratesV1 : kBitratesV2)[bitrateIndex];
    if (bitrate == 0) {
        return false;
    }
    uint32_t rate = kSampleRatesV1[rateIndex];
    if (version == 2) {
        rate /= 2;
    } else if (version == 0) {
        rate /= 4;
    }
    info.bitrateKbps = bitrate;
    info.sampleRate = rate;
    return true;
}

}  // namespace

uint32_t Mp3Info::offsetForMs(uint32_t ms) const {
    return dataOffset + static_cast<uint32_t>(static_cast<uint64_t>(ms) * bitrateKbps / 8);
}

Mp3Info readMp3Info(File& file) {
    Mp3Info info;
    const size_t size = file.size();
    uint32_t start = 0;

    uint8_t id3[10];
    file.seek(0);
    if (file.read(id3, sizeof(id3)) == sizeof(id3) && id3[0] == 'I' && id3[1] == 'D' && id3[2] == '3') {
        // Syncsafe size: 4 x 7 bits.
        start = 10 + ((id3[6] & 0x7F) << 21 | (id3[7] & 0x7F) << 14 | (id3[8] & 0x7F) << 7 | (id3[9] & 0x7F));
    }

    uint8_t buffer[kScanBytes];
    file.seek(start);
    const size_t length = file.read(buffer, sizeof(buffer));
    for (size_t i = 0; i + 4 <= length; ++i) {
        if (parseHeader(buffer + i, info)) {
            info.valid = true;
            info.dataOffset = start + i;
            break;
        }
    }
    if (info.valid && size > info.dataOffset) {
        info.durationMs = static_cast<uint32_t>(static_cast<uint64_t>(size - info.dataOffset) * 8 / info.bitrateKbps);
    }
    return info;
}

}  // namespace voicerec
