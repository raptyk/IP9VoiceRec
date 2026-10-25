// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
//
// CardputerVoiceRec - Smart Voice Recorder with MP3 compression and VAS
// for M5Stack Cardputer & Cardputer ADV (ESP32-S3)

#include <Arduino.h>
#include "app/App.h"

namespace {
voicerec::App app;
}

void setup() {
    app.begin();
}

void loop() {
    app.loop();
}
