// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#pragma once

#include <M5Cardputer.h>
#include "../hw/KeyboardInput.h"

namespace voicerec {

class App;

class Screen {
public:
    explicit Screen(App& app) : app_(app) {}
    virtual ~Screen() = default;

    virtual void draw(M5Canvas& canvas) = 0;
    virtual void onKey(const KeyEvent& event) = 0;
    virtual void tick() {}

protected:
    App& app_;
};

}  // namespace voicerec
