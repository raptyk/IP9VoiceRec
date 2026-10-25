# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#
# Repairs recordings made by firmware 1.0.0 / 1.1.0: their frame headers claim a padding byte
# that the frames do not have (shine computed 215 + 1 slots instead of 216), so players lose the
# frame sync after the first frame. The audio data itself is intact; only the padding bit of each
# such header is cleared.
#
# Usage: python tools/repair_mp3.py rec_0001.mp3 [more.mp3 ...]   -> writes rec_0001_fixed.mp3
#        python tools/repair_mp3.py --in-place rec_0001.mp3
import sys
from pathlib import Path

BITRATES_V1 = [0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0]
BITRATES_V2 = [0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0]
SAMPLE_RATES_V1 = [44100, 48000, 32000, 0]


def is_sync(data, i):
    return i + 4 <= len(data) and data[i] == 0xFF and (data[i + 1] & 0xE0) == 0xE0


def frame_length(header):
    """Layer III frame length in bytes including the padding byte, or 0 if not a valid header."""
    version = (header[1] >> 3) & 0x03  # 3 = MPEG-1, 2 = MPEG-2, 0 = MPEG-2.5
    layer = (header[1] >> 1) & 0x03
    if version == 1 or layer != 1:
        return 0
    bitrate = (BITRATES_V1 if version == 3 else BITRATES_V2)[header[2] >> 4]
    rate = SAMPLE_RATES_V1[(header[2] >> 2) & 0x03]
    if bitrate == 0 or rate == 0:
        return 0
    rate //= {3: 1, 2: 2, 0: 4}[version]
    padding = (header[2] >> 1) & 0x01
    slots = (144 if version == 3 else 72) * bitrate * 1000 // rate
    return slots + padding


def repair(data):
    data = bytearray(data)
    i = 0
    frames = 0
    fixed = 0
    while is_sync(data, i):
        length = frame_length(data[i:i + 4])
        if length == 0:
            break
        padded = (data[i + 2] >> 1) & 0x01
        end = i + length
        # A padded header whose frame really ends one byte earlier: clear the padding bit.
        if padded and not is_sync(data, end) and (is_sync(data, end - 1) or end - 1 == len(data)):
            data[i + 2] &= 0xFD
            length -= 1
            fixed += 1
        frames += 1
        i += length
    return bytes(data), frames, fixed, i


def main(argv):
    in_place = "--in-place" in argv
    paths = [Path(a) for a in argv if a != "--in-place"]
    if not paths:
        print("usage: python tools/repair_mp3.py [--in-place] file.mp3 ...")
        return 1
    for path in paths:
        original = path.read_bytes()
        repaired, frames, fixed, end = repair(original)
        target = path if in_place else path.with_name(path.stem + "_fixed" + path.suffix)
        status = "ok" if end == len(original) else f"stopped at byte {end} of {len(original)}"
        if fixed == 0:
            print(f"{path}: {frames} frames, nothing to repair ({status})")
            continue
        target.write_bytes(repaired)
        print(f"{path}: {frames} frames, {fixed} headers repaired ({status}) -> {target}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
