// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#include "Painter.h"

#include "Theme.h"

namespace voicerec::painter {

using namespace theme;

void drawClippedText(M5Canvas& canvas, const String& text, int x, int y, int maxWidth) {
    if (canvas.textWidth(text) <= maxWidth) {
        canvas.drawString(text, x, y);
        return;
    }
    String clipped = text;
    while (clipped.length() > 0 && canvas.textWidth(clipped + "...") > maxWidth) {
        clipped.remove(clipped.length() - 1);
    }
    canvas.drawString(clipped + "...", x, y);
}

void drawBattery(M5Canvas& canvas, int right, int centerY, int percent, bool charging) {
    const int bodyWidth = 18;
    const int bodyHeight = 9;
    const int x = right - bodyWidth - 2;
    const int y = centerY - bodyHeight / 2;
    const uint16_t colour = percent < 0 ? kTextMuted : (percent <= 15 ? kError : (percent <= 35 ? kWarn : kText));

    canvas.drawRect(x, y, bodyWidth, bodyHeight, colour);
    canvas.fillRect(x + bodyWidth, y + 2, 2, bodyHeight - 4, colour);
    if (percent >= 0) {
        const int fill = (bodyWidth - 4) * constrain(percent, 0, 100) / 100;
        canvas.fillRect(x + 2, y + 2, fill, bodyHeight - 4, colour);
    }
    if (charging) {
        // Lightning bolt drawn over the body.
        const int cx = x + bodyWidth / 2;
        canvas.fillTriangle(cx + 1, y - 1, cx - 3, centerY + 1, cx, centerY + 1, kWarn);
        canvas.fillTriangle(cx - 1, y + bodyHeight + 1, cx + 3, centerY - 1, cx, centerY - 1, kWarn);
    }

    canvas.setFont(&fonts::Font0);
    canvas.setTextSize(1);
    canvas.setTextColor(kTextMuted);
    canvas.setTextDatum(middle_right);
    if (percent >= 0) {
        canvas.drawString(String(percent) + "%", x - 3, centerY);
    }
}

int drawTitle(M5Canvas& canvas, int top, const char* title) {
    canvas.setFont(&fonts::FreeSansBold9pt7b);
    canvas.setTextSize(1);
    canvas.setTextColor(kText);
    canvas.setTextDatum(top_left);
    drawClippedText(canvas, title, kPadding, top + 2, kScreenWidth - 2 * kPadding);
    canvas.drawFastHLine(kPadding, top + 18, kScreenWidth - 2 * kPadding, kSurfaceHigh);
    return top + 20;
}

void drawListRow(M5Canvas& canvas, int y, int height, const String& label, const String& value, bool selected,
                 bool enabled) {
    if (selected) {
        canvas.fillRoundRect(3, y, kScreenWidth - 10, height, 4, enabled ? kAccent : kSurfaceHigh);
    }
    const uint16_t labelColour = enabled ? kText : (selected ? kTextMuted : kTextDisabled);
    canvas.setFont(&fonts::FreeSans9pt7b);
    canvas.setTextSize(1);
    canvas.setTextColor(labelColour);
    const int centerY = y + height / 2 + 1;

    int labelWidth = kScreenWidth - 2 * kPadding - 8;
    if (value.length() > 0) {
        canvas.setFont(&fonts::Font0);
        const int valueWidth = min<int>(canvas.textWidth(value), 110);
        canvas.setTextColor(selected ? kText : kTextMuted);
        canvas.setTextDatum(middle_right);
        drawClippedText(canvas, value, kScreenWidth - kPadding - 6, centerY, valueWidth);
        labelWidth -= valueWidth + 8;
        canvas.setFont(&fonts::FreeSans9pt7b);
        canvas.setTextColor(labelColour);
    }
    canvas.setTextDatum(middle_left);
    drawClippedText(canvas, label, kPadding + 2, centerY, labelWidth);
}

std::vector<String> wrapText(M5Canvas& canvas, const String& text, int maxWidth) {
    std::vector<String> lines;
    String line;
    int start = 0;
    const int length = text.length();
    while (start <= length) {
        int end = text.indexOf(' ', start);
        if (end < 0) {
            end = length;
        }
        const String word = text.substring(start, end);
        const String candidate = line.isEmpty() ? word : line + " " + word;
        if (canvas.textWidth(candidate) <= maxWidth || line.isEmpty()) {
            line = candidate;
        } else {
            lines.push_back(line);
            line = word;
        }
        start = end + 1;
    }
    if (!line.isEmpty()) {
        lines.push_back(line);
    }
    return lines;
}

int drawWrappedText(M5Canvas& canvas, const String& text, int x, int y, int maxWidth, int lineHeight, int maxLines) {
    std::vector<String> lines = wrapText(canvas, text, maxWidth);
    const int count = min<int>(lines.size(), maxLines);
    for (int i = 0; i < count; ++i) {
        String line = lines[i];
        if (i == count - 1 && static_cast<int>(lines.size()) > count) {
            line += " ...";  // more text than room: show that something is cut
        }
        drawClippedText(canvas, line, x, y + i * lineHeight, maxWidth);
    }
    return count * lineHeight;
}

void drawCenteredLines(M5Canvas& canvas, int top, int height, const char* title, const char* body,
                       const char* detail) {
    constexpr int kTitleHeight = 20;
    constexpr int kBodyLineHeight = 15;
    const int maxWidth = kScreenWidth - 2 * kPadding;

    // Measure first so the block can be centred vertically.
    canvas.setTextSize(1);
    canvas.setFont(&fonts::Font2);
    std::vector<String> bodyLines;
    for (const char* part : {body, detail}) {
        if (part != nullptr && part[0] != '\0') {
            for (const String& line : wrapText(canvas, part, maxWidth)) {
                bodyLines.push_back(line);
            }
        }
    }
    const int maxBodyLines = max(0, (height - kTitleHeight) / kBodyLineHeight);
    const int bodyCount = min<int>(bodyLines.size(), maxBodyLines);
    const int blockHeight = (title != nullptr ? kTitleHeight : 0) + bodyCount * kBodyLineHeight;
    int y = top + (height - blockHeight) / 2;

    canvas.setTextDatum(top_center);
    if (title != nullptr) {
        canvas.setFont(&fonts::FreeSansBold9pt7b);
        canvas.setTextColor(kText);
        drawClippedText(canvas, title, kScreenWidth / 2, y + 2, maxWidth);
        y += kTitleHeight;
    }
    canvas.setFont(&fonts::Font2);
    canvas.setTextColor(kTextMuted);
    for (int i = 0; i < bodyCount; ++i) {
        drawClippedText(canvas, bodyLines[i], kScreenWidth / 2, y, maxWidth);
        y += kBodyLineHeight;
    }
}

int measureKeyHint(M5Canvas& canvas, const char* key, const char* label) {
    canvas.setFont(&fonts::Font0);
    canvas.setTextSize(1);
    const int capWidth = max(16, static_cast<int>(canvas.textWidth(key)) + 8);
    return capWidth + 4 + canvas.textWidth(label);
}

int drawKeyHint(M5Canvas& canvas, int x, int centerY, const char* key, const char* label) {
    canvas.setFont(&fonts::Font0);
    canvas.setTextSize(1);
    const int capWidth = max(16, static_cast<int>(canvas.textWidth(key)) + 8);
    const int capHeight = 15;
    // Key cap: light top face with a darker bottom edge, like the Cardputer keys.
    canvas.fillRoundRect(x, centerY - capHeight / 2 + 1, capWidth, capHeight, 3, kSurface);
    canvas.fillRoundRect(x, centerY - capHeight / 2, capWidth, capHeight - 2, 3, kText);
    canvas.setTextColor(kBackground);
    canvas.setTextDatum(middle_center);
    canvas.drawString(key, x + capWidth / 2, centerY - 1);
    canvas.setTextColor(kText);
    canvas.setTextDatum(middle_left);
    canvas.drawString(label, x + capWidth + 4, centerY);
    return capWidth + 4 + canvas.textWidth(label);
}

void drawScrollBar(M5Canvas& canvas, int top, int height, int first, int visible, int total) {
    if (total <= visible) {
        return;
    }
    const int x = kScreenWidth - 4;
    canvas.fillRect(x, top, 2, height, kSurface);
    const int thumbHeight = max(8, height * visible / total);
    const int thumbTop = top + (height - thumbHeight) * first / max(1, total - visible);
    canvas.fillRect(x, thumbTop, 2, thumbHeight, kTextMuted);
}

void drawSpinner(M5Canvas& canvas, int x, int y) {
    const int phase = (millis() / 250) % 3;
    for (int i = 0; i < 3; ++i) {
        canvas.fillCircle(x + (i - 1) * 12, y, 3, i == phase ? kAccent : kSurfaceHigh);
    }
}

int meterX(int x, int width, float db) {
    const float clamped = constrain(db, kMeterMinDb, kMeterMaxDb);
    return x + static_cast<int>((clamped - kMeterMinDb) / (kMeterMaxDb - kMeterMinDb) * (width - 1));
}

void drawLevelMeter(M5Canvas& canvas, int x, int y, int width, int height, float levelDb, float peakDb,
                    uint16_t colour, float noiseDb, float triggerDb) {
    canvas.fillRoundRect(x, y, width, height, 3, kSurface);
    const int levelX = meterX(x, width, levelDb);
    if (levelDb > kMeterMinDb && levelX > x) {
        canvas.fillRoundRect(x, y, levelX - x + 1, height, 3, colour);
    }
    // Scale ticks every 10 dB.
    for (int db = static_cast<int>(kMeterMinDb) + 10; db < 0; db += 10) {
        canvas.drawFastVLine(meterX(x, width, db), y + height - 3, 3, kBackground);
    }
    if (noiseDb <= 0.0f) {
        const int noiseX = meterX(x, width, noiseDb);
        canvas.fillRect(noiseX - 1, y, 2, height, kTextMuted);
    }
    if (peakDb > kMeterMinDb) {
        canvas.drawFastVLine(meterX(x, width, peakDb), y, height, kText);
    }
    if (triggerDb <= 0.0f) {
        const int triggerX = meterX(x, width, triggerDb);
        canvas.fillTriangle(triggerX - 4, y - 5, triggerX + 4, y - 5, triggerX, y - 1, kText);
        canvas.drawFastVLine(triggerX, y, height, kText);
    }
}

void drawProgress(M5Canvas& canvas, int x, int y, int width, float progress, uint16_t colour) {
    progress = constrain(progress, 0.0f, 1.0f);
    canvas.fillRoundRect(x, y - 2, width, 4, 2, kSurfaceHigh);
    const int filled = static_cast<int>(progress * width);
    if (filled > 0) {
        canvas.fillRoundRect(x, y - 2, filled, 4, 2, colour);
    }
    canvas.fillCircle(x + filled, y, 5, kText);
}

String formatDuration(uint32_t ms) {
    const uint32_t seconds = ms / 1000;
    char text[16];
    if (seconds >= 3600) {
        snprintf(text, sizeof(text), "%u:%02u:%02u", seconds / 3600, (seconds / 60) % 60, seconds % 60);
    } else {
        snprintf(text, sizeof(text), "%u:%02u", seconds / 60, seconds % 60);
    }
    return text;
}

String formatBytes(uint64_t bytes) {
    char text[16];
    if (bytes >= (1ULL << 30)) {
        snprintf(text, sizeof(text), "%.1f GB", bytes / 1073741824.0);
    } else if (bytes >= (1ULL << 20)) {
        snprintf(text, sizeof(text), "%.1f MB", bytes / 1048576.0);
    } else {
        snprintf(text, sizeof(text), "%u KB", static_cast<unsigned>((bytes + 1023) / 1024));
    }
    return text;
}

}  // namespace voicerec::painter
