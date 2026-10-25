// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#pragma once

#include <Arduino.h>
#include <FS.h>
#include <SPI.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <vector>

namespace voicerec {

struct RecordingFile {
    String name;  // file name without the folder
    String path;  // full path on the card
    uint32_t sizeBytes = 0;
    int number = 0;  // n of rec_nnnn.mp3, 0 for other MP3 files
};

// The SD card (SPI on GPIO 40/39/14/12, shared by Cardputer and Cardputer ADV).
// Free space is cached: computing it walks the FAT and can take a second on large cards,
// so it is refreshed only after mounting, recording and deleting. Calls are serialized
// (UI and player task both open files); an open file is then used by one task only.
class SdStorage {
public:
    static SdStorage& instance();

    // Mounts the card if needed (retried at most every few seconds); detects removal.
    bool ensureMounted();
    bool mounted() const { return mounted_; }

    uint64_t totalBytes() const { return totalBytes_; }
    uint64_t freeBytes() const { return freeBytes_; }
    void refreshSpace();

    // Recordings, newest (highest number) first.
    std::vector<RecordingFile> listRecordings();
    size_t recordingCount();
    String nextRecordingPath();
    bool remove(const String& path);

    File openForWrite(const String& path);
    File openForRead(const String& path);

    static constexpr const char* kRecordingsDir = "/recordings";

private:
    SdStorage();

    bool mount();

    class Guard {
    public:
        explicit Guard(SemaphoreHandle_t lock) : lock_(lock) { xSemaphoreTakeRecursive(lock_, portMAX_DELAY); }
        ~Guard() { xSemaphoreGiveRecursive(lock_); }

    private:
        SemaphoreHandle_t lock_;
    };

    SemaphoreHandle_t lock_;
    SPIClass spi_;
    bool mounted_ = false;
    uint32_t lastMountAttemptAt_ = 0;
    uint64_t totalBytes_ = 0;
    uint64_t freeBytes_ = 0;
    int highestNumber_ = -1;  // -1 = not scanned since mounting
    int cachedCount_ = -1;
};

}  // namespace voicerec
