// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#pragma once

#include <stdint.h>
#include <vector>

namespace voicerec {

namespace keycode {
constexpr char kEnter = '\n';
constexpr char kBackspace = '\b';
constexpr char kTab = '\t';
constexpr char kOpt = 0x1B;
constexpr char kButtonG0 = 0x01;
constexpr char kSpace = ' ';
}  // namespace keycode

struct KeyEvent {
    char code = 0;
    char text = 0;
    bool fn = false;
    bool shift = false;
    bool ctrl = false;
    bool alt = false;
    bool repeat = false;

    bool is(char c) const { return code == c; }
};

class KeyboardInput {
public:
    static KeyboardInput& instance() {
        static KeyboardInput inst;
        return inst;
    }

    void update();
    KeyEvent nextEvent();
    bool hasEvent() const { return !events_.empty(); }
    void clear() { events_.clear(); }

    bool fn() const { return fn_; }
    bool shift() const { return shift_; }
    bool ctrl() const { return ctrl_; }
    bool alt() const { return alt_; }

private:
    KeyboardInput() = default;

    struct HeldKey {
        char code;
        char text;
        uint32_t pressedAt;
        uint32_t lastRepeatAt;
    };

    static constexpr uint32_t kRepeatDelayMs = 400;
    static constexpr uint32_t kRepeatIntervalMs = 80;

    void emit(const HeldKey& key, bool repeat);

    bool fn_ = false;
    bool shift_ = false;
    bool ctrl_ = false;
    bool alt_ = false;
    bool g0Held_ = false;

    std::vector<HeldKey> held_;
    std::vector<KeyEvent> events_;
};

}  // namespace voicerec
