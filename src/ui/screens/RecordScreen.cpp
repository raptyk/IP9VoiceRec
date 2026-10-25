// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#include "RecordScreen.h"

#include "app/App.h"
#include "audio/Recorder.h"
#include "storage/SdStorage.h"
#include "ui/Icons.h"
#include "ui/Screen.h"
#include "ui/Strings.h"
#include "ui/Theme.h"
#include "ui/screens/HelpScreen.h"

namespace voicerec {

namespace {
constexpr int kBandHeight = 22;
constexpr int kMeterRowHeight = 16;
constexpr int kKeyRowHeight = 18;
constexpr uint32_t kPeakHoldMs = 600;
constexpr float kPeakFallDbPerFrame = 1.5f;

// "rec_0007" from "/recordings/rec_0007.mp3".
String displayName(const String& path) {
    String name = path.substring(path.lastIndexOf('/') + 1);
    const int dot = name.lastIndexOf('.');
    return dot > 0 ? name.substring(0, dot) : name;
}
}  // namespace

void RecordScreen::onKey(const KeyEvent& event) {
    if (stopping_) {
        return;
    }
    Recorder& recorder = Recorder::instance();
    switch (event.code) {
        case keycode::kEnter:
        case keycode::kButtonG0:
        case keycode::kEsc:
        case keycode::kBackspace:
            if (!event.repeat) {
                requestStop();
            }
            break;
        case ' ':
            if (!event.repeat && recorder.state() == RecorderState::Recording) {
                recorder.setPaused(!recorder.paused());
            }
            break;
        case 'v':
            if (!event.repeat) {
                Settings& settings = app_.settings();
                settings.voiceActivation = !settings.voiceActivation;
                settingsChanged_ = true;
                app_.applySettings();
                app_.toast(settings.voiceActivation ? str::kToastVoiceOn : str::kToastVoiceOff);
            }
            break;
        case '[':
        case ']':
            changeSensitivity(event.code == ']' ? 1 : -1);
            break;
        case '-':
        case '=':
            changeGain(event.code == '=' ? 1 : -1);
            break;
        case keycode::kTab:
            if (!event.repeat) {
                app_.push(std::make_unique<HelpScreen>(app_, HelpScreen::kPageRecording));
            }
            break;
        default:
            break;
    }
}

void RecordScreen::changeSensitivity(int delta) {
    Settings& settings = app_.settings();
    settings.sensitivity = constrain(settings.sensitivity + delta, kSensitivityMin, kSensitivityMax);
    settingsChanged_ = true;
    app_.applySettings();
    char text[48];
    snprintf(text, sizeof(text), str::kToastSensitivity, settings.sensitivity,
             str::kSensitivityNames[settings.sensitivity - kSensitivityMin]);
    app_.toast(text);
}

void RecordScreen::changeGain(int delta) {
    Settings& settings = app_.settings();
    settings.micGainDb = constrain(settings.micGainDb + delta * kMicGainStepDb, kMicGainMinDb, kMicGainMaxDb);
    settingsChanged_ = true;
    app_.applySettings();
    char text[32];
    snprintf(text, sizeof(text), str::kToastGain, settings.micGainDb);
    app_.toast(text);
}

void RecordScreen::requestStop(const char* error) {
    stopping_ = true;
    error_ = error;
    Recorder::instance().stop();
}

void RecordScreen::tick() {
    Recorder& recorder = Recorder::instance();
    if (!stopping_) {
        if (recorder.writeError()) {
            requestStop(str::kErrorCard);
        } else if (recorder.micFailed()) {
            requestStop(str::kErrorMic);
        } else if (recorder.state() == RecorderState::Idle) {
            stopping_ = true;  // ended by itself (e.g. the microphone failed before the first frame)
        }
    }
    if (stopping_ && recorder.state() == RecorderState::Idle) {
        finish();
        return;
    }
    app_.invalidate();  // live meter and timers
}

void RecordScreen::finish() {
    const RecordingSummary summary = Recorder::instance().lastSummary();
    SdStorage& storage = SdStorage::instance();
    if (settingsChanged_) {
        app_.saveSettings();
    }

    char text[96];
    const String name = displayName(summary.path);
    if (summary.audioMs == 0) {
        storage.remove(summary.path);  // an empty file would only clutter the list
        if (error_ != nullptr) {
            app_.warn(error_);
        } else {
            app_.toast(str::kNothingSaved);
        }
    } else if (error_ != nullptr) {
        storage.refreshSpace();
        app_.warn(error_);
    } else if (summary.droppedFrames > 0) {
        storage.refreshSpace();
        snprintf(text, sizeof(text), str::kLostAudio,
                 painter::formatDuration(summary.droppedFrames * kFrameMs + 999).c_str());
        app_.warn(text);
    } else {
        storage.refreshSpace();
        snprintf(text, sizeof(text), str::kSaved, name.c_str(), painter::formatDuration(summary.audioMs).c_str());
        app_.toast(text);
        app_.signalSuccess();
    }
    app_.pop();
}

const char* RecordScreen::footer() const {
    return str::kFooterRecord;
}

void RecordScreen::draw(M5Canvas& canvas, int top, int height) {
    canvas.fillRect(0, top, theme::kScreenWidth, height, theme::kTintRecord);
    drawBand(canvas, top);
    const int middleTop = top + kBandHeight;
    const int middleHeight = height - kBandHeight - kMeterRowHeight - kKeyRowHeight;
    drawMiddle(canvas, middleTop, middleHeight);
    drawMeter(canvas, middleTop + middleHeight);
    drawKeyRow(canvas, top + height - kKeyRowHeight, kKeyRowHeight);
}

void RecordScreen::drawBand(M5Canvas& canvas, int top) {
    using namespace theme;
    const Recorder& recorder = Recorder::instance();
    const RecorderState state = recorder.state();
    const bool listening = app_.settings().voiceActivation && recorder.voiceState() == VoiceState::Silence;

    const char* title = str::kBandRecording;
    uint16_t colour = kRecord;
    bool writing = false;
    if (stopping_ || state == RecorderState::Saving) {
        title = str::kBandSaving;
    } else if (state == RecorderState::Starting) {
        title = str::kBandStarting;
        colour = kRecordListening;
    } else if (recorder.paused()) {
        title = str::kBandPaused;
        colour = kPaused;
    } else if (listening) {
        title = str::kBandListening;
        colour = kRecordListening;
    } else {
        writing = true;
    }

    canvas.fillRect(0, top, kScreenWidth, kBandHeight, colour);
    const int centerY = top + kBandHeight / 2;
    icons::draw(canvas, icons::Icon::Mic, 16, centerY, kText, 0.42f, colour);
    canvas.setFont(&fonts::FreeSansBold9pt7b);
    canvas.setTextSize(1);
    canvas.setTextColor(kText);
    canvas.setTextDatum(middle_left);
    painter::drawClippedText(canvas, title, 32, centerY + 1, 140);

    // Total time since the start (including skipped silence), with a blinking dot while writing.
    canvas.setFont(&fonts::Font2);
    canvas.setTextDatum(middle_right);
    const String elapsed = painter::formatDuration(recorder.elapsedMs());
    canvas.drawString(elapsed, kScreenWidth - kPadding, centerY + 1);
    if (writing && (millis() / 500) % 2 == 0) {
        canvas.fillCircle(kScreenWidth - kPadding - canvas.textWidth(elapsed) - 8, centerY, 4, kText);
    }
}

void RecordScreen::drawMiddle(M5Canvas& canvas, int top, int height) {
    using namespace theme;
    const Recorder& recorder = Recorder::instance();
    const int centerY = top + height / 2;

    // Large timer: length of what is in the file.
    const String timer = painter::formatDuration(recorder.audioMs());
    canvas.setTextSize(1);
    canvas.setFont(timer.length() > 5 ? &fonts::FreeSansBold12pt7b : &fonts::FreeSansBold18pt7b);
    canvas.setTextColor(kText);
    canvas.setTextDatum(middle_left);
    canvas.drawString(timer, kPadding + 2, centerY + 1);

    // File details on the right.
    const int right = kScreenWidth - kPadding;
    const int maxWidth = 104;
    canvas.setFont(&fonts::Font0);
    canvas.setTextDatum(top_right);
    const int lineTop = centerY - 17;
    canvas.setTextColor(kText);
    painter::drawClippedText(canvas, displayName(recorder.path()), right, lineTop, maxWidth);

    canvas.setTextColor(kTextMuted);
    if (recorder.state() == RecorderState::Starting) {
        painter::drawWrappedText(canvas, str::kMicStarting, right, lineTop + 12, maxWidth, 11, 2);
        return;
    }
    painter::drawClippedText(canvas, painter::formatBytes(recorder.bytesWritten()), right, lineTop + 12, maxWidth);
    char text[40];
    if (app_.settings().voiceActivation) {
        const uint32_t skipped = recorder.elapsedMs() > recorder.audioMs() ? recorder.elapsedMs() - recorder.audioMs() : 0;
        snprintf(text, sizeof(text), str::kSkipped, painter::formatDuration(skipped).c_str());
    } else {
        snprintf(text, sizeof(text), "%s", str::kRecordsEverything);
    }
    painter::drawClippedText(canvas, text, right, lineTop + 24, maxWidth);
}

void RecordScreen::drawMeter(M5Canvas& canvas, int top) {
    using namespace theme;
    const Recorder& recorder = Recorder::instance();
    const float level = recorder.levelDbfs();

    // Peak hold: stays for a moment, then falls back.
    const uint32_t now = millis();
    if (recorder.peakDbfs() >= peakHold_) {
        peakHold_ = recorder.peakDbfs();
        peakHoldAt_ = now;
    } else if (now - peakHoldAt_ > kPeakHoldMs) {
        peakHold_ -= kPeakFallDbPerFrame;
    }

    uint16_t colour = kTextMuted;
    const bool voiceActivation = app_.settings().voiceActivation;
    if (recorder.clipping()) {
        colour = kError;
    } else if (recorder.paused()) {
        colour = kTextDisabled;
    } else if (!voiceActivation || recorder.voiceState() == VoiceState::Voice) {
        colour = kOk;
    } else if (recorder.voiceState() == VoiceState::Hold) {
        colour = kWarn;
    }
    const int labelWidth = 28;
    const int width = kScreenWidth - 2 * kPadding - labelWidth;
    const float noise = voiceActivation ? recorder.noiseFloorDbfs() : 1.0f;
    const float trigger = voiceActivation ? recorder.triggerDbfs() : 1.0f;
    painter::drawLevelMeter(canvas, kPadding, top + 6, width, 8, level, peakHold_, colour, noise, trigger);

    canvas.setFont(&fonts::Font0);
    canvas.setTextSize(1);
    canvas.setTextDatum(middle_right);
    canvas.setTextColor(recorder.clipping() ? kError : kTextMuted);
    char text[8];
    snprintf(text, sizeof(text), "%d", static_cast<int>(lroundf(fmaxf(level, -99.0f))));
    canvas.drawString(recorder.clipping() ? "CLIP" : text, kScreenWidth - kPadding, top + 10);
}

void RecordScreen::drawKeyRow(M5Canvas& canvas, int top, int height) {
    const Recorder& recorder = Recorder::instance();
    struct Hint {
        const char* key;
        const char* label;
    };
    const Hint hints[] = {
        {str::kKeyEnter, str::kLabelSave},
        {str::kKeySpace, recorder.paused() ? str::kLabelResume : str::kLabelPause},
        {str::kKeyV, app_.settings().voiceActivation ? str::kLabelVoiceOn : str::kLabelVoiceOff},
    };
    constexpr int kCount = sizeof(hints) / sizeof(hints[0]);

    // Spread the hints evenly over the row.
    int total = 0;
    for (const Hint& hint : hints) {
        total += painter::measureKeyHint(canvas, hint.key, hint.label);
    }
    const int gap = (theme::kScreenWidth - total) / (kCount + 1);
    int x = gap;
    const int centerY = top + height / 2;
    for (const Hint& hint : hints) {
        x += painter::drawKeyHint(canvas, x, centerY, hint.key, hint.label) + gap;
    }
}

}  // namespace voicerec
