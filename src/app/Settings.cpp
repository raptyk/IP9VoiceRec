// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#include "Settings.h"
#include <ArduinoJson.h>
#include <esp_log.h>
#include "../storage/SdStorage.h"

namespace voicerec {

static const char* TAG = "Settings";

bool Settings::load() {
    if (!SdStorage::instance().ensureMounted()) {
        return false;
    }

    if (!SdStorage::instance().fileExists(kConfigPath)) {
        ESP_LOGI(TAG, "No config file found, using defaults");
        return false;
    }

    File file = SdStorage::instance().openFile(kConfigPath, FILE_READ);
    if (!file) {
        return false;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, file);
    file.close();

    if (err) {
        ESP_LOGW(TAG, "Failed to parse config: %s", err.c_str());
        return false;
    }

    vasEnabled = doc["vasEnabled"] | vasEnabled;
    vasAttackThresholdDbfs = doc["vasAttackThresholdDbfs"] | vasAttackThresholdDbfs;
    vasHysteresisMarginDb = doc["vasHysteresisMarginDb"] | vasHysteresisMarginDb;
    vasHangoverMs = doc["vasHangoverMs"] | vasHangoverMs;
    bitrateKbps = doc["bitrateKbps"] | bitrateKbps;
    screenBrightness = doc["screenBrightness"] | screenBrightness;
    statusLedEnabled = doc["statusLedEnabled"] | statusLedEnabled;
    speakerVolume = doc["speakerVolume"] | speakerVolume;

    ESP_LOGI(TAG, "Config loaded successfully");
    return true;
}

bool Settings::save() {
    if (!SdStorage::instance().ensureMounted()) {
        return false;
    }

    JsonDocument doc;
    doc["vasEnabled"] = vasEnabled;
    doc["vasAttackThresholdDbfs"] = vasAttackThresholdDbfs;
    doc["vasHysteresisMarginDb"] = vasHysteresisMarginDb;
    doc["vasHangoverMs"] = vasHangoverMs;
    doc["bitrateKbps"] = bitrateKbps;
    doc["screenBrightness"] = screenBrightness;
    doc["statusLedEnabled"] = statusLedEnabled;
    doc["speakerVolume"] = speakerVolume;

    File file = SdStorage::instance().openFile(kConfigPath, FILE_WRITE);
    if (!file) {
        ESP_LOGE(TAG, "Failed to open config file for write");
        return false;
    }

    serializeJsonPretty(doc, file);
    file.close();
    ESP_LOGI(TAG, "Config saved successfully");
    return true;
}

}  // namespace voicerec
