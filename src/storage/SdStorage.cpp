// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#include "SdStorage.h"

#include <SD.h>

#include <algorithm>

namespace voicerec {

namespace {
constexpr int8_t kSdSck = 40;
constexpr int8_t kSdMiso = 39;
constexpr int8_t kSdMosi = 14;
constexpr int8_t kSdCs = 12;
constexpr uint32_t kSdFrequency = 20000000;
// Without a card SD.begin() takes a moment; do not retry on every call.
constexpr uint32_t kMountRetryMs = 4000;

// "rec_0007.mp3" -> 7; other names -> 0.
int recordingNumber(const String& name) {
    String lower = name;
    lower.toLowerCase();
    if (!lower.startsWith("rec_") || !lower.endsWith(".mp3")) {
        return 0;
    }
    return lower.substring(4, lower.length() - 4).toInt();
}

bool isMp3(const String& name) {
    String lower = name;
    lower.toLowerCase();
    return lower.endsWith(".mp3") && !lower.startsWith(".");
}
}  // namespace

SdStorage& SdStorage::instance() {
    static SdStorage storage;
    return storage;
}

SdStorage::SdStorage() : lock_(xSemaphoreCreateRecursiveMutex()), spi_(FSPI) {}

bool SdStorage::mount() {
    lastMountAttemptAt_ = millis();
    spi_.begin(kSdSck, kSdMiso, kSdMosi, kSdCs);
    mounted_ = SD.begin(kSdCs, spi_, kSdFrequency);
    if (!mounted_) {
        SD.end();
        return false;
    }
    if (!SD.exists(kRecordingsDir)) {
        SD.mkdir(kRecordingsDir);
    }
    highestNumber_ = -1;
    cachedCount_ = -1;
    refreshSpace();
    log_i("SD card mounted: %llu MB, %llu MB free", totalBytes_ >> 20, freeBytes_ >> 20);
    return true;
}

bool SdStorage::ensureMounted() {
    Guard guard(lock_);
    if (mounted_ && SD.cardType() == CARD_NONE) {
        log_w("SD card removed");
        SD.end();
        mounted_ = false;
        totalBytes_ = 0;
        freeBytes_ = 0;
    }
    if (mounted_) {
        return true;
    }
    if (lastMountAttemptAt_ != 0 && millis() - lastMountAttemptAt_ < kMountRetryMs) {
        return false;
    }
    return mount();
}

void SdStorage::refreshSpace() {
    Guard guard(lock_);
    if (!mounted_) {
        return;
    }
    totalBytes_ = SD.totalBytes();
    const uint64_t used = SD.usedBytes();
    freeBytes_ = totalBytes_ > used ? totalBytes_ - used : 0;
}

std::vector<RecordingFile> SdStorage::listRecordings() {
    Guard guard(lock_);
    std::vector<RecordingFile> list;
    if (!ensureMounted()) {
        return list;
    }
    File dir = SD.open(kRecordingsDir);
    if (!dir || !dir.isDirectory()) {
        return list;
    }
    for (File entry = dir.openNextFile(); entry; entry = dir.openNextFile()) {
        if (entry.isDirectory()) {
            continue;
        }
        RecordingFile file;
        file.name = entry.name();
        const int slash = file.name.lastIndexOf('/');
        if (slash >= 0) {
            file.name = file.name.substring(slash + 1);
        }
        if (!isMp3(file.name)) {
            continue;
        }
        file.path = String(kRecordingsDir) + "/" + file.name;
        file.sizeBytes = entry.size();
        file.number = recordingNumber(file.name);
        list.push_back(file);
    }
    dir.close();

    std::sort(list.begin(), list.end(), [](const RecordingFile& a, const RecordingFile& b) {
        return a.number != b.number ? a.number > b.number : a.name > b.name;
    });
    highestNumber_ = list.empty() ? 0 : std::max(0, list.front().number);
    cachedCount_ = static_cast<int>(list.size());
    return list;
}

size_t SdStorage::recordingCount() {
    Guard guard(lock_);
    if (cachedCount_ < 0) {
        listRecordings();
    }
    return mounted_ ? static_cast<size_t>(std::max(0, cachedCount_)) : 0;
}

String SdStorage::nextRecordingPath() {
    Guard guard(lock_);
    if (highestNumber_ < 0) {
        listRecordings();
    }
    // Never overwrite: skip numbers that exist (e.g. a file copied onto the card meanwhile).
    for (int number = std::max(0, highestNumber_) + 1;; ++number) {
        char path[48];
        snprintf(path, sizeof(path), "%s/rec_%04d.mp3", kRecordingsDir, number);
        if (!SD.exists(path)) {
            highestNumber_ = number;
            if (cachedCount_ >= 0) {
                ++cachedCount_;
            }
            return path;
        }
    }
}

bool SdStorage::remove(const String& path) {
    Guard guard(lock_);
    if (!ensureMounted()) {
        return false;
    }
    const bool removed = SD.remove(path);
    if (removed) {
        cachedCount_ = -1;
        refreshSpace();
    }
    return removed;
}

File SdStorage::openForWrite(const String& path) {
    Guard guard(lock_);
    if (!ensureMounted()) {
        return File();
    }
    return SD.open(path, FILE_WRITE);
}

File SdStorage::openForRead(const String& path) {
    Guard guard(lock_);
    if (!ensureMounted()) {
        return File();
    }
    return SD.open(path, FILE_READ);
}

}  // namespace voicerec
