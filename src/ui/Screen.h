// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#pragma once

#include <IP9Kit.h>

#include "ui/Strings.h"
#include "ui/Theme.h"

namespace voicerec {

// The shared IP9Kit core (https://github.com/raptyk/IP9Kit) is part of this app's vocabulary:
// KeyEvent, keycode::, painter::, ConfirmScreen, InfoScreen, ListItem, ...
using namespace ip9;

class App;

// A full-screen view of this app; `app_` gives access to the App's services.
class Screen : public ip9::AppScreen<App> {
public:
    using AppScreen::AppScreen;
};

// A list (settings, recordings) with access to the App.
class ListScreen : public ip9::AppScreen<App, ip9::ListScreen> {
public:
    using AppScreen::AppScreen;
};

}  // namespace voicerec
