// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#include "ListScreen.h"

#include "app/App.h"
#include "ui/Painter.h"
#include "ui/Strings.h"
#include "ui/Theme.h"

namespace voicerec {

namespace {
constexpr int kRowHeight = 18;
constexpr int kDescriptionLineHeight = 10;
constexpr int kDescriptionLines = 2;
constexpr int kDescriptionHeight = kDescriptionLines * kDescriptionLineHeight + 4;
}  // namespace

ListScreen::ListScreen(App& app, String title, std::vector<ListItem> items)
    : Screen(app), title_(std::move(title)), items_(std::move(items)) {}

void ListScreen::setItems(std::vector<ListItem> items) {
    items_ = std::move(items);
    selected_ = constrain(selected_, 0, max(0, static_cast<int>(items_.size()) - 1));
    firstVisible_ = min(firstVisible_, selected_);
    app_.invalidate();
}

void ListScreen::select(int index) {
    if (index >= 0 && index < static_cast<int>(items_.size())) {
        selected_ = index;
    }
}

void ListScreen::onBack() {
    app_.pop();
}

void ListScreen::onKey(const KeyEvent& event) {
    const int count = static_cast<int>(items_.size());
    switch (event.code) {
        case keycode::kUp:
        case keycode::kDown:
            if (count > 0) {
                selected_ = (selected_ + (event.code == keycode::kUp ? count - 1 : 1)) % count;
                if (!isEnabled(selected_)) {
                    app_.signalWarning();  // a locked row came into focus
                }
            }
            break;
        case keycode::kLeft:
        case keycode::kRight:
            if (count > 0 && items_[selected_].onAdjust) {
                items_[selected_].onAdjust(event.code == keycode::kRight ? 1 : -1);
            }
            break;
        case keycode::kEnter:
            if (!event.repeat && count > 0) {
                activate(selected_);  // may push/pop screens; `this` can be retired afterwards
            }
            return;
        case keycode::kBackspace:
        case keycode::kEsc:
            if (!event.repeat) {
                onBack();
            }
            return;
        default:
            // Digit shortcuts: 1..9 select and activate an item directly.
            if (!event.repeat && event.code >= '1' && event.code <= '9') {
                const int index = event.code - '1';
                if (index < count) {
                    selected_ = index;
                    activate(index);
                    return;
                }
            }
            break;
    }
}

bool ListScreen::isEnabled(int index) const {
    const ListItem& item = items_[index];
    return !item.enabled || item.enabled();
}

void ListScreen::activate(int index) {
    ListItem& item = items_[index];
    if (!isEnabled(index)) {
        if (item.lockedReason) {
            app_.warn(item.lockedReason());
        }
        return;
    }
    if (item.onSelect) {
        item.onSelect();
    } else if (item.onAdjust) {
        item.onAdjust(1);
    }
}

const char* ListScreen::footer() const {
    if (!items_.empty() && items_[selected_].onAdjust && !items_[selected_].onSelect) {
        return str::kFooterValue;
    }
    return str::kFooterList;
}

void ListScreen::draw(M5Canvas& canvas, int top, int height) {
    using namespace theme;
    const int listTop = painter::drawTitle(canvas, top, title_.c_str());
    const int bottom = top + height;
    if (items_.empty()) {
        drawEmpty(canvas, listTop, bottom - listTop);
        return;
    }

    const bool hasDescription = !items_[selected_].description.isEmpty();
    const int listHeight = bottom - listTop - (hasDescription ? kDescriptionHeight : 0);
    const int visible = max(1, listHeight / kRowHeight);
    if (selected_ < firstVisible_) {
        firstVisible_ = selected_;
    } else if (selected_ >= firstVisible_ + visible) {
        firstVisible_ = selected_ - visible + 1;
    }

    for (int row = 0; row < visible; ++row) {
        const int index = firstVisible_ + row;
        if (index >= static_cast<int>(items_.size())) {
            break;
        }
        const ListItem& item = items_[index];
        const bool enabled = isEnabled(index);
        // Locked rows show no state, only a greyed-out label.
        const String value = enabled && item.value ? item.value() : String();
        painter::drawListRow(canvas, listTop + row * kRowHeight, kRowHeight - 1, item.label, value,
                             index == selected_, enabled);
    }
    painter::drawScrollBar(canvas, listTop, visible * kRowHeight, firstVisible_, visible, items_.size());

    if (hasDescription) {
        const int descriptionTop = bottom - kDescriptionHeight + 2;
        canvas.drawFastHLine(kPadding, descriptionTop - 2, kScreenWidth - 2 * kPadding, kSurfaceHigh);
        canvas.setFont(&fonts::Font0);
        canvas.setTextSize(1);
        canvas.setTextColor(kTextMuted);
        canvas.setTextDatum(top_left);
        painter::drawWrappedText(canvas, items_[selected_].description, kPadding, descriptionTop + 1,
                                 kScreenWidth - 2 * kPadding, kDescriptionLineHeight, kDescriptionLines);
    }
}

void ListScreen::drawEmpty(M5Canvas& canvas, int top, int height) {
    painter::drawCenteredLines(canvas, top, height, "-");
}

}  // namespace voicerec
