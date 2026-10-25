// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#pragma once

#include <Arduino.h>

#include <functional>

#include "ui/Screen.h"

namespace voicerec {

// Yes / no question. Enter confirms, esc / del cancels.
class ConfirmScreen : public Screen {
public:
    ConfirmScreen(App& app, String question, String detail, std::function<void()> onConfirm,
                  std::function<void()> onCancel = nullptr);

    void onKey(const KeyEvent& event) override;
    void draw(M5Canvas& canvas, int top, int height) override;
    const char* footer() const override;

private:
    String question_;
    String detail_;
    std::function<void()> onConfirm_;
    std::function<void()> onCancel_;
};

// Small pop-up card with a title and up to two lines of text; any of enter/esc/del closes it.
class InfoScreen : public Screen {
public:
    // `autoCloseMs` > 0 closes the pop-up by itself (success messages).
    InfoScreen(App& app, String title, String body, String detail = String(), uint32_t autoCloseMs = 0);

    void onEnter() override;
    void onKey(const KeyEvent& event) override;
    void tick() override;
    void draw(M5Canvas& canvas, int top, int height) override;
    const char* footer() const override;

private:
    String title_;
    String body_;
    String detail_;
    uint32_t autoCloseMs_;
    uint32_t shownAt_ = 0;
};

}  // namespace voicerec
