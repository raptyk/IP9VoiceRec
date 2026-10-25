// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#pragma once

#include <Arduino.h>

#include <functional>
#include <vector>

#include "ui/Screen.h"

namespace voicerec {

struct ListItem {
    String label;
    std::function<String()> value;            // optional text on the right
    std::function<void()> onSelect;           // enter
    std::function<void(int delta)> onAdjust;  // left / right (for value settings)
    String description;                       // shown at the bottom while the row is selected
    std::function<bool()> enabled;            // optional: greyed out and locked while false
    std::function<String()> lockedReason;     // toast shown when a locked row is selected
};

// Vertical list navigated with ; . (up/down), enter to select, , / to adjust values,
// del or esc to go back. The selected row's description explains what it does,
// so every option is self-explanatory without a manual.
class ListScreen : public Screen {
public:
    ListScreen(App& app, String title, std::vector<ListItem> items = {});

    void onKey(const KeyEvent& event) override;
    void draw(M5Canvas& canvas, int top, int height) override;
    const char* footer() const override;

    std::vector<ListItem>& items() { return items_; }

protected:
    void setItems(std::vector<ListItem> items);
    void select(int index);
    bool isEnabled(int index) const;
    // Runs the row's action, or explains why a locked row cannot be used yet.
    void activate(int index);
    // Content drawn instead of the list when it is empty (e.g. "Scanning...").
    virtual void drawEmpty(M5Canvas& canvas, int top, int height);
    // Called when del/esc leaves the list (default: close the screen).
    virtual void onBack();

    String title_;
    std::vector<ListItem> items_;
    int selected_ = 0;
    int firstVisible_ = 0;
};

}  // namespace voicerec
