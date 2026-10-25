// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#pragma once

#include "../Screen.h"
#include "../../audio/VadEngine.h"

namespace voicerec {

enum class CalibStep : uint8_t {
    Ready,
    Measuring,
    Done
};

class CalibrationScreen : public Screen {
public:
    explicit CalibrationScreen(App& app);
    ~CalibrationScreen() override;

    void draw(M5Canvas& canvas) override;
    void onKey(const KeyEvent& event) override;
    void tick() override;

private:
    CalibStep step_ = CalibStep::Ready;
    CalibrationResult result_;
    uint32_t startMs_ = 0;
};

}  // namespace voicerec
