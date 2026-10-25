// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#include "KeyboardInput.h"
#include <M5Cardputer.h>
#include <algorithm>

namespace voicerec {

namespace {

enum class Modifier : uint8_t { None, Fn, Shift, Ctrl, Alt };

struct PositionKey {
    int x;
    int y;
    char code;
    Modifier modifier;
};

constexpr PositionKey kPositionKeys[] = {
    {13, 0, keycode::kBackspace, Modifier::None},
    {0, 1, keycode::kTab, Modifier::None},
    {0, 2, 0, Modifier::Fn},
    {1, 2, 0, Modifier::Shift},
    {13, 2, keycode::kEnter, Modifier::None},
    {0, 3, 0, Modifier::Ctrl},
    {1, 3, keycode::kOpt, Modifier::None},
    {2, 3, 0, Modifier::Alt},
};

const PositionKey* findPositionKey(const Point2D_t& pos) {
    for (const auto& key : kPositionKeys) {
        if (key.x == pos.x && key.y == pos.y) {
            return &key;
        }
    }
    return nullptr;
}

}  // namespace

void KeyboardInput::update() {
    const uint32_t now = millis();
    auto& keyboard = M5Cardputer.Keyboard;

    bool fn = false;
    bool shift = false;
    bool ctrl = false;
    bool alt = false;
    std::vector<std::pair<char, char>> pressed;

    for (const Point2D_t& pos : keyboard.keyList()) {
        if (pos.x < 0 || pos.x > 13 || pos.y < 0 || pos.y > 3) {
            continue;
        }
        if (const PositionKey* special = findPositionKey(pos)) {
            switch (special->modifier) {
                case Modifier::Fn: fn = true; break;
                case Modifier::Shift: shift = true; break;
                case Modifier::Ctrl: ctrl = true; break;
                case Modifier::Alt: alt = true; break;
                case Modifier::None: pressed.emplace_back(special->code, 0); break;
            }
            continue;
        }
        KeyValue_t value = keyboard.getKeyValue(pos);
        pressed.emplace_back(value.value_first, value.value_second);
    }

    fn_ = fn;
    shift_ = shift;
    ctrl_ = ctrl;
    alt_ = alt;

    // Drop released keys
    held_.erase(std::remove_if(held_.begin(), held_.end(),
                               [&](const HeldKey& key) {
                                   return std::none_of(pressed.begin(), pressed.end(),
                                                       [&](const auto& p) { return p.first == key.code; });
                               }),
                held_.end());

    // New presses and auto-repeat
    for (const auto& [code, shifted] : pressed) {
        auto it = std::find_if(held_.begin(), held_.end(), [&](const HeldKey& key) { return key.code == code; });
        if (it == held_.end()) {
            HeldKey key{code, shifted, now, now};
            held_.push_back(key);
            emit(key, false);
        } else if (now - it->pressedAt >= kRepeatDelayMs && now - it->lastRepeatAt >= kRepeatIntervalMs) {
            it->lastRepeatAt = now;
            emit(*it, true);
        }
    }

    // G0 Button
    auto& button = M5Cardputer.BtnA;
    if (button.wasPressed()) {
        HeldKey key{keycode::kButtonG0, 0, now, now};
        emit(key, false);
    }
}

void KeyboardInput::emit(const HeldKey& key, bool repeat) {
    KeyEvent event;
    event.code = key.code;
    event.fn = fn_;
    event.shift = shift_;
    event.ctrl = ctrl_;
    event.alt = alt_;
    event.repeat = repeat;
    if (key.text != 0) {
        event.text = shift_ ? key.text : key.code;
    } else if (key.code == ' ') {
        event.text = ' ';
    }
    events_.push_back(event);
}

KeyEvent KeyboardInput::nextEvent() {
    if (events_.empty()) {
        return {};
    }
    KeyEvent ev = events_.front();
    events_.erase(events_.begin());
    return ev;
}

}  // namespace voicerec
