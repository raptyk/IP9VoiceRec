// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#include "SdStorage.h"
#include <esp_log.h>
#include <algorithm>

namespace voicerec {

static const char* TAG = "SdStorage";

namespace {
constexpr int8_t kSdSck = 40;
constexpr int8_t kSdMiso = 39;
constexpr int8_t kSdMosi = 14;
constexpr int8_t kSdCs = 12;
constexpr uint32_t kSdFrequency = 20000000; // 20 MHz
constexpr uint32_t kMountRetryIntervalMs = 5000;
}  // namespace

SdStorage::SdStorage()
    : sdSpi_(FSPI) {}

bool SdStorage::begin() {
    if (mounted_) {
        return true;
    }

    lastMountAttemptAt_ = millis();
    sdSpi_.begin(kSdSck, kSdMiso, kSdMosi, kSdCs);

    mounted_ = SD.begin(kSdCs, sdSpi_, kSdFrequency);
    if (!mounted_) {
        ESP_LOGW(TAG, "SD mount failed on CS %d", kSdCs);
        SD.end();
        return false;
    }

    ESP_LOGI(TAG, "SD mounted successfully. Type: %d, Size: %llu MB",
             SD.cardType(), SD.cardSize() / (1024 * 1024));

    if (!SD.exists(kRecordingsDir)) {
        SD.mkdir(kRecordingsDir);
    }
    if (!SD.exists(kConfigDir)) {
        SD.mkdir(kConfigDir);
    }

    return true;
}

bool SdStorage::ensureMounted() {
    if (mounted_) {
        if (SD.cardType() == CARD_NONE) {
            ESP_LOGW(TAG, "SD card removed");
            SD.end();
            mounted_ = false;
        } else {
            return true;
        }
    }

    const uint32_t now = millis();
    if (lastMountAttemptAt_ != 0 && now - lastMountAttemptAt_ < kMountRetryIntervalMs) {
        return false;
    }

    return begin();
}

uint64_t SdStorage::totalBytes() const {
    if (!mounted_) return 0;
    return SD.totalBytes();
}

uint64_t SdStorage::usedBytes() const {
    if (!mounted_) return 0;
    return SD.usedBytes();
}

uint64_t SdStorage::freeBytes() const {
    if (!mounted_) return 0;
    const uint64_t total = totalBytes();
    const uint64_t used = usedBytes();
    return total > used ? (total - used) : 0;
}

std::vector<RecordingFile> SdStorage::listRecordings() {
    std::vector<RecordingFile> list;
    if (!ensureMounted()) {
        return list;
    }

    File dir = SD.open(kRecordingsDir);
    if (!dir || !dir.isDirectory()) {
        return list;
    }

    File entry = dir.openNextFile();
    while (entry) {
        if (!entry.isDirectory()) {
            std::string name = entry.name();
            // Match .mp3 extension
            if (name.length() > 4) {
                std::string ext = name.substr(name.length() - 4);
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                if (ext == ".mp3") {
                    RecordingFile rec;
                    rec.name = name;
                    rec.path = std::string(kRecordingsDir) + "/" + name;
                    rec.sizeBytes = entry.size();
                    // At 48 kbps = 6000 bytes/sec
                    rec.estimatedDurationSec = static_cast<uint32_t>(rec.sizeBytes / 6000);
                    list.push_back(rec);
                }
            }
        }
        entry = dir.openNextFile();
    }
    dir.close();

    // Sort newest/highest index first
    std::sort(list.begin(), list.end(), [](const RecordingFile& a, const RecordingFile& b) {
        return a.name > b.name;
    });

    return list;
}

std::string SdStorage::generateNextRecordingPath() {
    if (!ensureMounted()) {
        return std::string(kRecordingsDir) + "/rec_0001.mp3";
    }

    int highestIndex = 0;
    File dir = SD.open(kRecordingsDir);
    if (dir && dir.isDirectory()) {
        File entry = dir.openNextFile();
        while (entry) {
            std::string name = entry.name();
            int idx = 0;
            if (sscanf(name.c_str(), "rec_%04d.mp3", &idx) == 1 ||
                sscanf(name.c_str(), "REC_%04d.MP3", &idx) == 1) {
                if (idx > highestIndex) {
                    highestIndex = idx;
                }
            }
            entry = dir.openNextFile();
        }
        dir.close();
    }

    char buf[64];
    snprintf(buf, sizeof(buf), "%s/rec_%04d.mp3", kRecordingsDir, highestIndex + 1);
    return std::string(buf);
}

bool SdStorage::deleteRecording(const std::string& path) {
    if (!ensureMounted()) {
        return false;
    }
    return SD.remove(path.c_str());
}

File SdStorage::openFile(const char* path, const char* mode) {
    if (!ensureMounted()) {
        return File();
    }
    return SD.open(path, mode);
}

bool SdStorage::fileExists(const char* path) {
    if (!ensureMounted()) {
        return false;
    }
    return SD.exists(path);
}

}  // namespace voicerec
