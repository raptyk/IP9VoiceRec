// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#pragma once

#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <vector>
#include <string>

namespace voicerec {

struct RecordingFile {
    std::string name;
    std::string path;
    size_t sizeBytes = 0;
    uint32_t estimatedDurationSec = 0;
};

class SdStorage {
public:
    static SdStorage& instance() {
        static SdStorage inst;
        return inst;
    }

    bool begin();
    bool isMounted() const { return mounted_; }
    bool ensureMounted();

    uint64_t totalBytes() const;
    uint64_t usedBytes() const;
    uint64_t freeBytes() const;

    std::vector<RecordingFile> listRecordings();
    std::string generateNextRecordingPath();
    bool deleteRecording(const std::string& path);

    File openFile(const char* path, const char* mode);
    bool fileExists(const char* path);

    static constexpr const char* kRecordingsDir = "/recordings";
    static constexpr const char* kConfigDir = "/config";

private:
    SdStorage();

    SPIClass sdSpi_;
    bool mounted_ = false;
    uint32_t lastMountAttemptAt_ = 0;
};

}  // namespace voicerec
