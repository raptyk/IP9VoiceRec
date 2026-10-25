// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#include "MicCheckScreen.h"

#include "app/App.h"
#include "audio/Player.h"
#include "audio/Recorder.h"
#include "ui/Icons.h"
#include "ui/Screen.h"
#include "ui/Strings.h"
#include "ui/Theme.h"
#include "ui/screens/HelpScreen.h"

namespace voicerec {

namespace {
constexpr int kBandHeight = 22;
constexpr int kKeyRowHeight = 18;
constexpr int kLabelWidth = 28;
constexpr uint32_t kPeakHoldMs = 600;
constexpr float kPeakFallDbPerFrame = 1.5f;
constexpr float kGoodSnrDb = 20.0f;
constexpr float kOkSnrDb = 12.0f;
constexpr uint32_t kQuietDelayMs = 600;  // the click of the enter key must not count as room noise

const char* roomName(calibration::RoomKind room) {
    switch (room) {
        case calibration::RoomKind::Quiet: return str::kRoomQuiet;
        case calibration::RoomKind::Normal: return str::kRoomNormal;
        default: return str::kRoomNoisy;
    }
}
}  // namespace

MicCheckScreen::~MicCheckScreen() {
    Recorder::instance().stopMonitor();
    if (settingsChanged_) {
        app_.saveSettings();
    }
}

void MicCheckScreen::onEnter() {
    Player::instance().stop();  // the speaker and the microphone share pins
    Recorder::instance().startMonitor();
}

void MicCheckScreen::onKey(const KeyEvent& event) {
    if (phase_ != Phase::Live) {
        if (!event.repeat && (event.code == keycode::kEsc || event.code == keycode::kBackspace)) {
            phase_ = Phase::Live;
            Recorder::instance().captureLevels(0);
        }
        return;
    }
    switch (event.code) {
        case keycode::kEnter:
        case keycode::kButtonG0:
        case 'c':
            if (!event.repeat) {
                startCalibration();
            }
            break;
        case '[':
        case ']':
        case '-':
        case '=':
        case 'v':
            if (event.code != 'v' || !event.repeat) {
                changeSetting(event.code);
            }
            break;
        case keycode::kTab:
            if (!event.repeat) {
                app_.push(std::make_unique<HelpScreen>(app_, HelpScreen::kPageRecording));
            }
            break;
        case keycode::kEsc:
        case keycode::kBackspace:
            if (!event.repeat) {
                app_.pop();
            }
            break;
        default:
            break;
    }
}

void MicCheckScreen::changeSetting(char key) {
    Settings& settings = app_.settings();
    char text[48];
    if (key == '[' || key == ']') {
        settings.sensitivity = constrain(settings.sensitivity + (key == ']' ? 1 : -1), kSensitivityMin, kSensitivityMax);
        snprintf(text, sizeof(text), str::kToastSensitivity, settings.sensitivity,
                 str::kSensitivityNames[settings.sensitivity - kSensitivityMin]);
    } else if (key == '-' || key == '=') {
        settings.micGainDb = constrain(settings.micGainDb + (key == '=' ? kMicGainStepDb : -kMicGainStepDb),
                                       kMicGainMinDb, kMicGainMaxDb);
        snprintf(text, sizeof(text), str::kToastGain, settings.micGainDb);
    } else {
        settings.voiceActivation = !settings.voiceActivation;
        snprintf(text, sizeof(text), "%s", settings.voiceActivation ? str::kToastVoiceOn : str::kToastVoiceOff);
    }
    settingsChanged_ = true;
    app_.applySettings();
    app_.toast(text);
}

void MicCheckScreen::startCalibration() {
    phase_ = Phase::Quiet;
    captureStarted_ = false;
    phaseStartedAt_ = millis();
}

void MicCheckScreen::tick() {
    Recorder& recorder = Recorder::instance();
    if (recorder.micFailed() && !failureShown_) {
        failureShown_ = true;
        app_.warn(str::kErrorMic);
        app_.pop();
        return;
    }
    app_.invalidate();  // live meter
    if (phase_ == Phase::Live || recorder.micWarmingUp() || !recorder.micRunning()) {
        return;
    }
    if (!captureStarted_) {
        if (phase_ == Phase::Quiet && millis() - phaseStartedAt_ < kQuietDelayMs) {
            return;
        }
        captureStarted_ = true;
        recorder.captureLevels(phase_ == Phase::Quiet ? calibration::kQuietFrames : calibration::kSpeechFrames);
        return;
    }
    if (!recorder.levelsReady()) {
        return;
    }
    float levels[Recorder::kMaxLevelFrames];
    const size_t count = recorder.copyLevels(levels, Recorder::kMaxLevelFrames);
    if (phase_ == Phase::Quiet) {
        noise_ = calibration::analyzeNoise(levels, count);
        phase_ = Phase::Speech;
        captureStarted_ = false;
        return;
    }
    const calibration::SpeechResult speech =
        calibration::analyzeSpeech(levels, count, noise_, app_.settings().micGainDb);
    phase_ = Phase::Live;

    // Apply: gain from the voice level, the noise floor moves along with the gain.
    Settings& settings = app_.settings();
    const int oldGain = settings.micGainDb;
    const int newGain = speech.heard ? speech.recommendedGainDb : oldGain;
    settings.micGainDb = newGain;
    settings.noiseFloorDbfs = noise_.floorDbfs + (newGain - oldGain);
    settings.calibratedGainDb = newGain;
    settings.calibrated = true;
    if (speech.heard) {
        settings.sensitivity = calibration::sensitivityFor(noise_, speech);
    }
    settingsChanged_ = true;
    app_.applySettings();
    recorder.restartNoiseTracking();

    char body[64];
    char detail[64];
    if (speech.heard) {
        snprintf(body, sizeof(body), str::kCalResult, roomName(noise_.room), newGain, settings.sensitivity);
        const char* format = speech.snrDb >= kGoodSnrDb ? str::kCalSnrGood
                             : speech.snrDb >= kOkSnrDb ? str::kCalSnrOk
                                                        : str::kCalSnrLow;
        snprintf(detail, sizeof(detail), format, static_cast<int>(lroundf(speech.snrDb)));
        app_.signalSuccess();
        app_.push(std::make_unique<InfoScreen>(app_, str::kCalDoneTitle, body, detail));
    } else {
        snprintf(body, sizeof(body), str::kCalNoVoiceBody, roomName(noise_.room));
        app_.signalWarning();
        app_.push(std::make_unique<InfoScreen>(app_, str::kCalNoVoiceTitle, body, str::kCalNoVoiceDetail));
    }
}

const char* MicCheckScreen::footer() const {
    return phase_ == Phase::Live ? str::kFooterMicCheck : str::kFooterCalibrating;
}

void MicCheckScreen::draw(M5Canvas& canvas, int top, int height) {
    canvas.fillRect(0, top, theme::kScreenWidth, height, theme::kTintMicCheck);
    drawBand(canvas, top);
    const int middleTop = top + kBandHeight;
    if (phase_ == Phase::Live) {
        drawLive(canvas, middleTop, height - kBandHeight - kKeyRowHeight);
        drawKeyRow(canvas, top + height - kKeyRowHeight, kKeyRowHeight);
    } else {
        drawCalibration(canvas, middleTop, height - kBandHeight);
    }
}

void MicCheckScreen::drawBand(M5Canvas& canvas, int top) {
    using namespace theme;
    const char* title = phase_ == Phase::Quiet    ? str::kBandCalQuiet
                        : phase_ == Phase::Speech ? str::kBandCalSpeak
                                                  : str::kBandMicCheck;
    canvas.fillRect(0, top, kScreenWidth, kBandHeight, kMicCheck);
    const int centerY = top + kBandHeight / 2;
    icons::draw(canvas, icons::Icon::Levels, 16, centerY, kText, 0.42f, kMicCheck);
    canvas.setFont(&fonts::FreeSansBold9pt7b);
    canvas.setTextSize(1);
    canvas.setTextColor(kText);
    canvas.setTextDatum(middle_left);
    painter::drawClippedText(canvas, title, 32, centerY + 1, 150);

    char gain[24];
    snprintf(gain, sizeof(gain), str::kGainShort, app_.settings().micGainDb);
    canvas.setFont(&fonts::Font0);
    canvas.setTextDatum(middle_right);
    canvas.drawString(gain, kScreenWidth - kPadding, centerY);
}

void MicCheckScreen::drawMeter(M5Canvas& canvas, int y, int height, bool markers) {
    using namespace theme;
    const Recorder& recorder = Recorder::instance();
    const uint32_t now = millis();
    if (recorder.peakDbfs() >= peakHold_) {
        peakHold_ = recorder.peakDbfs();
        peakHoldAt_ = now;
    } else if (now - peakHoldAt_ > kPeakHoldMs) {
        peakHold_ -= kPeakFallDbPerFrame;
    }
    uint16_t colour = kTextMuted;
    if (recorder.clipping()) {
        colour = kError;
    } else if (recorder.voiceState() == VoiceState::Voice) {
        colour = kOk;
    } else if (recorder.voiceState() == VoiceState::Hold) {
        colour = kWarn;
    }
    const int width = kScreenWidth - 2 * kPadding - kLabelWidth;
    painter::drawLevelMeter(canvas, kPadding, y, width, height, recorder.levelDbfs(), peakHold_, colour,
                            markers ? recorder.noiseFloorDbfs() : 1.0f, markers ? recorder.triggerDbfs() : 1.0f);

    canvas.setFont(&fonts::Font0);
    canvas.setTextSize(1);
    canvas.setTextDatum(middle_right);
    canvas.setTextColor(recorder.clipping() ? kError : kText);
    char text[8];
    snprintf(text, sizeof(text), "%d", static_cast<int>(lroundf(fmaxf(recorder.levelDbfs(), -99.0f))));
    canvas.drawString(recorder.clipping() ? "CLIP" : text, kScreenWidth - kPadding, y + height / 2);
}

void MicCheckScreen::drawLive(M5Canvas& canvas, int top, int height) {
    using namespace theme;
    const Recorder& recorder = Recorder::instance();
    const Settings& settings = app_.settings();
    (void)height;

    // Detector verdict.
    const char* verdict = str::kNoVoice;
    uint16_t dot = kTextMuted;
    if (!recorder.micRunning() || recorder.micWarmingUp()) {
        verdict = str::kMicStarting;
    } else if (recorder.voiceState() == VoiceState::Voice) {
        verdict = str::kVoiceDetected;
        dot = kOk;
    } else if (recorder.voiceState() == VoiceState::Hold) {
        verdict = str::kVoiceHold;
        dot = kWarn;
    }
    canvas.fillCircle(kPadding + 5, top + 12, 4, dot);
    canvas.setFont(&fonts::Font2);
    canvas.setTextSize(1);
    canvas.setTextColor(kText);
    canvas.setTextDatum(middle_left);
    canvas.drawString(verdict, kPadding + 14, top + 12);

    // Meter with the scale below.
    const int meterY = top + 27;
    const int meterHeight = 10;
    drawMeter(canvas, meterY, meterHeight, settings.voiceActivation);
    const int width = kScreenWidth - 2 * kPadding - kLabelWidth;
    canvas.setFont(&fonts::Font0);
    canvas.setTextColor(kTextMuted);
    canvas.setTextDatum(top_center);
    for (int db = -60; db <= 0; db += 20) {
        canvas.drawString(String(db), painter::meterX(kPadding, width, db), meterY + meterHeight + 2);
    }

    // Numbers behind the markers.
    char info[48];
    if (settings.voiceActivation) {
        snprintf(info, sizeof(info), str::kNoiseInfo, static_cast<int>(lroundf(recorder.noiseFloorDbfs())),
                 static_cast<int>(lroundf(recorder.triggerDbfs())));
    } else {
        snprintf(info, sizeof(info), "%s", str::kVoiceActivationOffInfo);
    }
    canvas.setTextDatum(top_left);
    canvas.drawString(info, kPadding, meterY + meterHeight + 13);
}

void MicCheckScreen::drawCalibration(M5Canvas& canvas, int top, int height) {
    using namespace theme;
    const Recorder& recorder = Recorder::instance();
    const bool quiet = phase_ == Phase::Quiet;
    const bool waiting = !captureStarted_ || recorder.micWarmingUp();
    const char* title = quiet ? str::kCalQuietTitle : str::kCalSpeakTitle;
    const char* body = waiting && recorder.micWarmingUp() ? str::kMicStarting
                       : quiet                            ? str::kCalQuietText
                                                          : str::kCalSpeakText;

    canvas.setTextSize(1);
    canvas.setFont(&fonts::FreeSansBold9pt7b);
    canvas.setTextColor(kText);
    canvas.setTextDatum(top_left);
    painter::drawClippedText(canvas, title, kPadding + 2, top + 5, kScreenWidth - 2 * kPadding);
    canvas.setFont(&fonts::Font2);
    canvas.setTextColor(kTextMuted);
    painter::drawWrappedText(canvas, body, kPadding + 2, top + 25, kScreenWidth - 2 * kPadding - 4, 14, 2);

    const float progress = waiting ? 0.0f : recorder.levelProgress();
    painter::drawProgress(canvas, kPadding + 4, top + height - 26, kScreenWidth - 2 * kPadding - 8, progress,
                          kMicCheck);
    drawMeter(canvas, top + height - 14, 8, false);
}

void MicCheckScreen::drawKeyRow(M5Canvas& canvas, int top, int height) {
    struct Hint {
        const char* key;
        const char* label;
    };
    char sensitivity[16];
    snprintf(sensitivity, sizeof(sensitivity), str::kLabelSens, app_.settings().sensitivity);
    const Hint hints[] = {
        {str::kKeyEnter, str::kLabelCalibrate},
        {str::kKeyBrackets, sensitivity},
        {str::kKeyMinusPlus, str::kLabelGain},
    };
    constexpr int kCount = sizeof(hints) / sizeof(hints[0]);
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
