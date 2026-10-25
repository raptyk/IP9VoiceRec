// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
//
// IP9 VoiceRec - voice recorder with MP3 compression and voice activation
// for M5Stack Cardputer and Cardputer ADV (ESP32-S3). Part of the IP9 apps: https://cardputer.ip9.pl

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
