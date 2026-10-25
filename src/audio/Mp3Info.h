// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#pragma once

#include <FS.h>
#include <stddef.h>
#include <stdint.h>

namespace voicerec {

// Format of a constant-bitrate MP3 file, read from its first frame header.
struct Mp3Info {
    bool valid = false;
    uint16_t bitrateKbps = 0;
    uint32_t sampleRate = 0;
    uint32_t dataOffset = 0;  // first frame (after an ID3v2 tag, if any)
    uint32_t durationMs = 0;

    // Byte offset of a playback position (CBR: proportional).
    uint32_t offsetForMs(uint32_t ms) const;
};

// Reads the header of an open file; leaves the file position undefined.
Mp3Info readMp3Info(File& file);

}  // namespace voicerec
