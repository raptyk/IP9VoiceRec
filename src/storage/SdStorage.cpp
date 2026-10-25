// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#include "SdStorage.h"
#include <esp_log.h>
#include <algorithm>

namespace voicerec {

[[maybe_unused]] static const char* TAG = "SdStorage";

namespace {
constexpr int8_t kSdSck = 40;
constexpr int8_t kSdMiso = 39;
constexpr int8_t kSdMosi = 14;
constexpr int8_t kSdCs = 12;
constexpr uint32_t kSdFrequency = 20000000; // 20 MHz
constexpr uint32_t kMountRetryIntervalMs = 5000;
}  // namespace

SdStorage::SdStorage()
    : sdSpi_(FSPI) {
    mutex_ = xSemaphoreCreateMutex();
}

SdStorage::~SdStorage() {
    if (mutex_) {
        vSemaphoreDelete(mutex_);
        mutex_ = nullptr;
    }
}

void SdStorage::lock() {
    if (mutex_) {
        xSemaphoreTake(mutex_, portMAX_DELAY);
    }
}

void SdStorage::unlock() {
    if (mutex_) {
        xSemaphoreGive(mutex_);
    }
}

bool SdStorage::begin() {
    lock();
    if (mounted_) {
        unlock();
        return true;
    }

    lastMountAttemptAt_ = millis();
    sdSpi_.begin(kSdSck, kSdMiso, kSdMosi, kSdCs);

    mounted_ = SD.begin(kSdCs, sdSpi_, kSdFrequency);
    if (!mounted_) {
        ESP_LOGW(TAG, "SD mount failed on CS %d", kSdCs);
        SD.end();
        unlock();
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

    unlock();
    return true;
}

bool SdStorage::ensureMounted() {
    lock();
    if (mounted_) {
        if (SD.cardType() == CARD_NONE) {
            ESP_LOGW(TAG, "SD card removed");
            SD.end();
            mounted_ = false;
        } else {
            unlock();
            return true;
        }
    }

    const uint32_t now = millis();
    if (lastMountAttemptAt_ != 0 && now - lastMountAttemptAt_ < kMountRetryIntervalMs) {
        unlock();
        return false;
    }
    unlock();

    return begin();
}

uint64_t SdStorage::totalBytes() {
    lock();
    if (!mounted_) {
        unlock();
        return 0;
    }
    uint64_t total = SD.totalBytes();
    unlock();
    return total;
}

uint64_t SdStorage::usedBytes() {
    lock();
    if (!mounted_) {
        unlock();
        return 0;
    }
    uint64_t used = SD.usedBytes();
    unlock();
    return used;
}

uint64_t SdStorage::freeBytes() {
    lock();
    if (!mounted_) {
        unlock();
        return 0;
    }
    const uint64_t total = SD.totalBytes();
    const uint64_t used = SD.usedBytes();
    unlock();
    return total > used ? (total - used) : 0;
}

std::vector<RecordingFile> SdStorage::listRecordings() {
    std::vector<RecordingFile> list;
    if (!ensureMounted()) {
        return list;
    }

    lock();
    File dir = SD.open(kRecordingsDir);
    if (!dir || !dir.isDirectory()) {
        unlock();
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
                    rec.estimatedDurationSec = static_cast<uint32_t>(rec.sizeBytes / 6000);
                    list.push_back(rec);
                }
            }
        }
        entry = dir.openNextFile();
    }
    dir.close();
    unlock();

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
    lock();
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
    unlock();

    char buf[64];
    snprintf(buf, sizeof(buf), "%s/rec_%04d.mp3", kRecordingsDir, highestIndex + 1);
    return std::string(buf);
}

bool SdStorage::deleteRecording(const std::string& path) {
    if (!ensureMounted()) {
        return false;
    }
    lock();
    bool res = SD.remove(path.c_str());
    unlock();
    return res;
}

File SdStorage::openFile(const char* path, const char* mode) {
    if (!ensureMounted()) {
        return File();
    }
    lock();
    File f = SD.open(path, mode);
    unlock();
    return f;
}

bool SdStorage::fileExists(const char* path) {
    if (!ensureMounted()) {
        return false;
    }
    lock();
    bool exists = SD.exists(path);
    unlock();
    return exists;
}

}  // namespace voicerec
