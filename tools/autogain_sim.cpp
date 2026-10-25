// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
//
// Host simulation of the recording chain with the firmware's own Dsp, VoiceDetector and AutoGain:
// synthetic speech / noise / knocks -> microphone gain + limiter -> level -> voice detector ->
// pre-roll (voice activation) -> AutoGain lookahead -> gain ramp + limiter, as in Recorder.
// Every scenario checks the levels of the written audio. Any C++17 compiler, from the repo root:
//   g++ -std=c++17 -O2 -Isrc tools/autogain_sim.cpp src/audio/AutoGain.cpp src/audio/Dsp.cpp \
//       src/audio/VoiceDetector.cpp -o autogain_sim && ./autogain_sim
#include <math.h>
#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <functional>
#include <random>
#include <string>
#include <vector>

#include "audio/AudioConfig.h"
#include "audio/AutoGain.h"
#include "audio/Dsp.h"
#include "audio/VoiceDetector.h"

using namespace voicerec;

namespace {

constexpr size_t N = kSamplesPerFrame;
constexpr float kPi = 3.14159265f;
constexpr float kFrameSeconds = static_cast<float>(N) / kSampleRate;

float dbToAmp(float dbfs) {
    return powf(10.0f, dbfs / 20.0f);
}

// ---------------------------------------------------------------- signal generator

// Sound before the microphone gain, as full-scale floats, plus which samples are speech.
struct Signal {
    std::vector<float> samples;
    std::vector<bool> speech;
    std::mt19937 rng{1234};

    void add(float value, bool isSpeech) {
        samples.push_back(value);
        speech.push_back(isSpeech);
    }
    float gauss() { return std::normal_distribution<float>(0.0f, 1.0f)(rng); }
    float uniform(float a, float b) { return std::uniform_real_distribution<float>(a, b)(rng); }

    // Steady background noise.
    void silence(float seconds, float noiseDbfs) {
        const size_t count = static_cast<size_t>(seconds * kSampleRate);
        for (size_t i = 0; i < count; ++i) {
            add(gauss() * dbToAmp(noiseDbfs), false);
        }
    }

    // Speech-like sound: syllables (raised-cosine bursts of a vowel-ish tone mix plus noise)
    // whose loud frames sit around `loudDbfs` RMS, short gaps between syllables and words.
    void speak(float seconds, float loudDbfs, float noiseDbfs) {
        const size_t end = samples.size() + static_cast<size_t>(seconds * kSampleRate);
        int syllable = 0;
        while (samples.size() < end) {
            const float length = uniform(0.12f, 0.26f);
            const float level = loudDbfs + uniform(-7.0f, 1.5f);
            const float pitch = uniform(110.0f, 240.0f);
            const size_t count = static_cast<size_t>(length * kSampleRate);
            // A crest of ~12 dB: peak amplitude 4x the RMS, as in speech.
            const float amp = dbToAmp(level) * 1.41f;
            for (size_t i = 0; i < count && samples.size() < end; ++i) {
                const float t = static_cast<float>(i) / kSampleRate;
                const float envelope = 0.5f - 0.5f * cosf(2.0f * kPi * i / count);
                const float tone = 0.55f * sinf(2 * kPi * pitch * t) + 0.3f * sinf(2 * kPi * pitch * 2.03f * t) +
                                   0.2f * sinf(2 * kPi * pitch * 3.1f * t) + 0.25f * gauss();
                add(amp * envelope * tone + gauss() * dbToAmp(noiseDbfs), envelope > 0.25f);
            }
            ++syllable;
            const float gap = syllable % 4 == 0 ? uniform(0.18f, 0.35f) : uniform(0.03f, 0.09f);
            silence(gap, noiseDbfs);
        }
    }

    // A knock: 4 ms decaying click.
    void knock(float peakDbfs, float noiseDbfs) {
        const size_t count = static_cast<size_t>(0.004f * kSampleRate);
        for (size_t i = 0; i < count; ++i) {
            add(dbToAmp(peakDbfs) * expf(-6.0f * i / count) * (i % 2 ? 1.0f : -1.0f), false);
        }
        silence(0.05f, noiseDbfs);
    }
};

float percentile(std::vector<float> values, int percent);

// ---------------------------------------------------------------- recording chain

struct OutFrame {
    float rmsDbfs;
    float peakDbfs;
    float gainDb;
    bool speech;    // ground truth: mostly speech samples
    bool limited;   // the output limiter worked on this frame
};

struct Chain {
    // Configuration.
    bool voiceActivation = true;
    bool autoLevel = true;
    uint8_t sensitivity = 3;
    int micGainDb = 0;
    std::function<int(size_t)> gainAt;  // optional: microphone gain per frame index

    VoiceDetector detector;
    AutoGain autoGain;
    std::vector<int16_t> preRoll[kPreRollFrames];
    FrameInfo preRollInfo[kPreRollFrames];
    bool preRollTruth[kPreRollFrames] = {};
    size_t preRollCount = 0;
    size_t preRollHead = 0;

    std::vector<int16_t> lookahead[AutoGain::kLookaheadFrames];
    bool lookaheadTruth[AutoGain::kLookaheadFrames] = {};
    size_t lookaheadHead = 0;
    float lastGain = 1.0f;

    std::vector<OutFrame> out;
    std::vector<float> inputSpeechLevels;  // RMS of truth-speech frames before AutoGain

    float inputP80() const { return percentile(inputSpeechLevels, 80); }
    uint32_t framesWritten = 0;

    void begin(float startNoise) {
        detector.configure(sensitivity, 1500);
        detector.reset(startNoise);
        autoGain.beginSession();
        lookaheadHead = 0;
        lastGain = dsp::dbToGain(autoGain.currentGainDb());
        preRollCount = 0;
        preRollHead = 0;
        out.clear();
        inputSpeechLevels.clear();
    }

    void emit(std::vector<int16_t>& pcm, bool truth, float gainDb) {
        const dsp::FrameLevel level = dsp::measure(pcm.data(), N);
        out.push_back({level.rmsDbfs, level.peakDbfs, gainDb, truth, level.peakDbfs > -2.6f});
    }

    void writeOldest() {
        std::vector<int16_t>& pcm = lookahead[lookaheadHead];
        const float gainDb = autoGain.takeGainDb();
        const float gain = dsp::dbToGain(gainDb);
        dsp::applyGainRamp(pcm.data(), N, lastGain, gain);
        lastGain = gain;
        emit(pcm, lookaheadTruth[lookaheadHead], gainDb);
        lookaheadHead = (lookaheadHead + 1) % AutoGain::kLookaheadFrames;
    }

    void write(const std::vector<int16_t>& pcm, const FrameInfo& info, bool truth) {
        ++framesWritten;
        if (truth) {
            inputSpeechLevels.push_back(info.rmsDbfs);
        }
        if (!autoLevel) {
            std::vector<int16_t> copy = pcm;
            emit(copy, truth, 0.0f);
            return;
        }
        if (autoGain.full()) {
            writeOldest();
        }
        const size_t slot = (lookaheadHead + autoGain.pending()) % AutoGain::kLookaheadFrames;
        lookahead[slot] = pcm;
        lookaheadTruth[slot] = truth;
        autoGain.observe(info);
    }

    void flush() {
        while (autoGain.pending() > 0) {
            writeOldest();
        }
    }

    void run(const Signal& signal) {
        const size_t frames = signal.samples.size() / N;
        int appliedGain = micGainDb;
        for (size_t f = 0; f < frames; ++f) {
            const int gainDb = gainAt ? gainAt(f) : micGainDb;
            if (gainDb != appliedGain) {
                detector.shiftNoise(static_cast<float>(gainDb - appliedGain));
                appliedGain = gainDb;
            }
            std::vector<int16_t> pcm(N);
            size_t speechSamples = 0;
            for (size_t i = 0; i < N; ++i) {
                const float value = signal.samples[f * N + i] * 32767.0f;
                pcm[i] = static_cast<int16_t>(std::max(-32768.0f, std::min(32767.0f, value)));
                speechSamples += signal.speech[f * N + i] ? 1 : 0;
            }
            const bool truth = speechSamples > N / 2;
            dsp::applyGain(pcm.data(), N, dsp::dbToGain(static_cast<float>(gainDb)));
            const dsp::FrameLevel level = dsp::measure(pcm.data(), N);
            const VoiceDecision decision = detector.process(level.rmsDbfs);
            FrameInfo info;
            info.rmsDbfs = level.rmsDbfs;
            info.peakDbfs = level.peakDbfs;
            info.noiseDbfs = detector.noiseFloor();
            info.voiced = decision.state == VoiceState::Voice;
            info.inputGainDb = static_cast<int8_t>(gainDb);

            if (!voiceActivation) {
                write(pcm, info, truth);
                continue;
            }
            if (decision.writing) {
                if (decision.onset) {
                    const size_t oldest = preRollCount < kPreRollFrames ? 0 : preRollHead;
                    for (size_t i = 0; i < preRollCount; ++i) {
                        const size_t slot = (oldest + i) % kPreRollFrames;
                        write(preRoll[slot], preRollInfo[slot], preRollTruth[slot]);
                    }
                    preRollCount = 0;
                    preRollHead = 0;
                }
                write(pcm, info, truth);
            } else {
                preRoll[preRollHead] = pcm;
                preRollInfo[preRollHead] = info;
                preRollTruth[preRollHead] = truth;
                preRollHead = (preRollHead + 1) % kPreRollFrames;
                preRollCount = std::min(preRollCount + 1, kPreRollFrames);
                if (decision.released) {
                    flush();  // WriterOp::Sync
                }
            }
        }
        flush();  // WriterOp::Close
    }
};

// ---------------------------------------------------------------- statistics

float percentile(std::vector<float> values, int percent) {
    if (values.empty()) {
        return kSilenceDbfs;
    }
    std::sort(values.begin(), values.end());
    return values[std::min(values.size() - 1, values.size() * percent / 100)];
}

struct Stats {
    float speechP80 = kSilenceDbfs;  // loud speech frames
    float noiseMedian = kSilenceDbfs;
    float maxPeak = kSilenceDbfs;
    float limitedShare = 0.0f;  // of the speech frames
    float maxGainStep = 0.0f;   // largest frame-to-frame gain change (dB)
    float finalGain = 0.0f;
    float minGain = 99.0f;
    float maxGain = -99.0f;
};

Stats stats(const std::vector<OutFrame>& frames, size_t from = 0, size_t to = SIZE_MAX) {
    Stats s;
    std::vector<float> speech;
    std::vector<float> noise;
    size_t limited = 0;
    to = std::min(to, frames.size());
    for (size_t i = from; i < to; ++i) {
        const OutFrame& f = frames[i];
        (f.speech ? speech : noise).push_back(f.rmsDbfs);
        s.maxPeak = std::max(s.maxPeak, f.peakDbfs);
        limited += f.speech && f.limited ? 1 : 0;
        if (i > from) {
            s.maxGainStep = std::max(s.maxGainStep, fabsf(f.gainDb - frames[i - 1].gainDb));
        }
        s.minGain = std::min(s.minGain, f.gainDb);
        s.maxGain = std::max(s.maxGain, f.gainDb);
    }
    s.speechP80 = percentile(speech, 80);
    s.noiseMedian = percentile(noise, 50);
    s.limitedShare = speech.empty() ? 0.0f : static_cast<float>(limited) / speech.size();
    s.finalGain = to > from ? frames[to - 1].gainDb : 0.0f;
    return s;
}

int failures = 0;

void check(bool ok, const char* what) {
    printf("    %s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) {
        ++failures;
    }
}

void report(const char* name, const Stats& s) {
    printf("\n%s\n", name);
    printf("    speech P80 %6.1f dBFS | noise %6.1f | max peak %5.1f | limited %4.1f %% | gain %5.1f..%5.1f "
           "(end %5.1f, max step %.2f)\n",
           s.speechP80, s.noiseMedian, s.maxPeak, 100.0f * s.limitedShare, s.minGain, s.maxGain, s.finalGain,
           s.maxGainStep);
}

size_t framesFor(float seconds) {
    return static_cast<size_t>(seconds / kFrameSeconds);
}

bool near(float value, float target, float tolerance) {
    return fabsf(value - target) <= tolerance;
}

}  // namespace

int main() {
    const float target = AutoGain::kTargetSpeechDbfs;

    {
        Signal sig;
        sig.silence(2, -66);
        sig.speak(20, -38, -66);
        sig.silence(2, -66);
        Chain c;
        c.autoGain.resetLearning();
        c.begin(-66);
        c.run(sig);
        const Stats s = stats(c.out);
        report("1. Quiet speech (-38 dBFS), quiet room, voice activation", s);
        check(near(s.speechP80, target, 3.5f), "speech raised to the target level");
        check(s.limitedShare < 0.02f, "limiter (almost) never needed");
        check(s.maxPeak < -1.0f, "no clipping");
        const Stats steady = stats(c.out, framesFor(6), framesFor(20));
        printf("    after learning: gain %.1f..%.1f, max step %.2f dB\n", steady.minGain, steady.maxGain,
               steady.maxGainStep);
        check(steady.maxGain - steady.minGain < 4.0f, "steady gain during steady speech (no pumping)");
        check(steady.maxGainStep <= 1.0f, "no gain jumps after learning");
    }
    {
        Signal sig;
        sig.silence(1, -60);
        sig.speak(15, -20, -60);
        Chain c;
        c.autoGain.resetLearning();
        c.begin(-60);
        c.run(sig);
        const Stats s = stats(c.out);
        report("2. Normal speech (-20 dBFS): Auto level stays out of the way", s);
        printf("    input speech P80 %.1f dBFS\n", c.inputP80());
        check(near(s.finalGain, target - c.inputP80(), 3.0f), "only the small correction to the target");
        check(near(s.speechP80, target, 3.0f), "level at the target");
    }
    {
        Signal sig;
        sig.silence(1, -55);
        sig.speak(15, -6, -55);
        Chain c;
        c.autoGain.resetLearning();
        c.begin(-55);
        c.run(sig);
        const Stats s = stats(c.out);
        report("3. Shouting close to the mic (-6 dBFS)", s);
        check(s.finalGain < -6.0f, "gain lowered");
        check(s.maxPeak < -1.0f, "no clipping");
        check(near(s.speechP80, target, 4.0f), "speech brought down towards the target");
    }
    {
        Signal sig;
        sig.silence(2, -44);
        sig.speak(20, -34, -44);
        Chain c;
        c.sensitivity = 5;
        c.autoGain.resetLearning();
        c.begin(-44);
        c.run(sig);
        const Stats s = stats(c.out);
        report("4. Quiet speech in a noisy room (noise -44, speech -34)", s);
        check(s.noiseMedian < AutoGain::kNoiseCeilingDbfs + 1.5f, "noise not raised above the ceiling");
        check(s.finalGain >= 0.0f, "never attenuated because of noise");
        check(s.speechP80 >= c.inputP80() - 0.5f, "speech not made quieter");
    }
    {
        Signal sig;
        sig.silence(30, -64);
        Chain c;
        c.voiceActivation = false;
        c.autoGain.resetLearning();
        c.begin(-64);
        c.run(sig);
        const Stats s = stats(c.out);
        report("5. Only silence, voice activation off", s);
        check(s.maxGain - s.minGain < 0.01f && fabsf(s.finalGain) < 0.01f, "gain untouched (0 dB)");
        check(s.noiseMedian < -60.0f, "background stays quiet");
    }
    {
        Signal sig;
        sig.silence(1, -66);
        sig.speak(8, -40, -66);
        sig.silence(1, -66);
        sig.knock(-4, -66);  // a knock on the table in the pause
        sig.silence(1, -66);
        sig.speak(4, -40, -66);
        sig.knock(-6, -66);  // and one in the middle of speech
        sig.speak(4, -40, -66);
        Chain c;
        c.voiceActivation = false;
        c.autoGain.resetLearning();
        c.begin(-66);
        c.run(sig);
        const Stats s = stats(c.out);
        report("6. Knocks while the gain is high (+20 dB)", s);
        check(s.maxGain > 15.0f, "quiet speech got a high gain");
        check(s.maxPeak < -1.0f, "knocks not amplified into clipping (peak guard)");
    }
    {
        Signal sig;
        sig.silence(1, -64);
        sig.speak(10, -37, -64);
        sig.speak(6, -9, -64);  // suddenly much louder
        sig.speak(10, -37, -64);
        Chain c;
        c.autoGain.resetLearning();
        c.begin(-64);
        c.run(sig);
        report("7. Quiet speech, then a sudden shout, then quiet again", stats(c.out));
        const size_t shoutStart = framesFor(11);
        const Stats shout = stats(c.out, shoutStart, shoutStart + framesFor(6));
        const Stats after = stats(c.out, c.out.size() - framesFor(4));
        check(shout.maxPeak < -1.0f, "the shout is not clipped");
        check(shout.limitedShare < 0.1f, "limiter only on a few frames of the shout");
        printf("    last 4 s: speech P80 %.1f dBFS, gain %.1f dB\n", after.speechP80, after.finalGain);
        check(near(after.speechP80, target, 4.5f), "quiet speech raised again afterwards");
    }
    {
        Signal sig;
        sig.silence(1, -63);
        sig.speak(6, -36, -63);
        sig.silence(6, -63);  // long pause, recorded (voice activation off)
        sig.speak(6, -36, -63);
        Chain c;
        c.voiceActivation = false;
        c.autoGain.resetLearning();
        c.begin(-63);
        c.run(sig);
        const Stats s = stats(c.out);
        report("8. Long pause with voice activation off: no pumping", s);
        const size_t pauseFrom = framesFor(8);
        const Stats pause = stats(c.out, pauseFrom, pauseFrom + framesFor(4));
        check(pause.maxGain - pause.minGain < 0.3f, "gain frozen in the pause");
        check(pause.noiseMedian < AutoGain::kNoiseCeilingDbfs + 1.0f, "pause noise stays low");
    }
    {
        Signal sig;
        sig.silence(1, -66);
        sig.speak(20, -34, -66);
        Chain c;
        c.autoGain.resetLearning();
        const size_t change = framesFor(11);
        c.gainAt = [change](size_t f) { return f < change ? 0 : 9; };
        c.begin(-66);
        c.run(sig);
        report("9. Mic gain raised by 9 dB in the middle of a recording", stats(c.out));
        const Stats before = stats(c.out, framesFor(5), framesFor(9));
        const Stats after = stats(c.out, c.out.size() - framesFor(5));
        check(near(before.speechP80, after.speechP80, 3.0f), "output level the same before and after");
    }
    {
        Chain c;
        c.autoGain.resetLearning();
        Signal first;
        first.silence(1, -65);
        first.speak(10, -42, -65);
        c.begin(-65);
        c.run(first);
        const float learned = stats(c.out).finalGain;
        Signal second;
        second.rng.seed(99);
        second.silence(1, -65);
        second.speak(10, -26, -65);  // another speaker, much louder
        c.begin(-65);
        c.run(second);
        const Stats s = stats(c.out);
        report("10. Next recording, different speaker (learned gain carried over)", s);
        printf("    gain learned in the first recording: %.1f dB\n", learned);
        const Stats late = stats(c.out, framesFor(2.5f));
        check(s.maxPeak < -1.0f, "no clipping although the carried gain was high");
        check(near(late.speechP80, target, 3.5f), "settled on the new speaker within ~1.5 s");
    }
    {
        Signal sig;
        sig.silence(2, -72);
        sig.speak(15, -52, -72);
        Chain c;
        c.sensitivity = 5;
        c.autoGain.resetLearning();
        c.begin(-72);
        c.run(sig);
        const Stats s = stats(c.out);
        report("11. Whisper far away (-52 dBFS)", s);
        check(s.maxGain <= AutoGain::kMaxGainDb + 0.01f, "gain capped at the maximum");
        printf("    input speech P80 %.1f dBFS\n", c.inputP80());
        check(s.speechP80 > c.inputP80() + 20.0f, "whisper made clearly louder");
        check(s.noiseMedian < AutoGain::kNoiseCeilingDbfs + 1.0f, "noise below the ceiling");
    }
    {
        Signal sig;
        sig.silence(1, -64);
        sig.speak(8, -36, -64);
        sig.speak(12, -36, -48);  // a fan starts
        Chain c;
        c.voiceActivation = false;
        c.autoGain.resetLearning();
        c.begin(-64);
        c.run(sig);
        report("12. A fan starts during the recording", stats(c.out));
        const Stats late = stats(c.out, c.out.size() - framesFor(6));
        check(late.noiseMedian < AutoGain::kNoiseCeilingDbfs + 2.0f, "fan noise kept near the ceiling");
        check(late.speechP80 > -36.0f, "speech still above its original level");
    }
    {
        Signal sig;
        sig.silence(1, -60);
        sig.speak(10, -40, -60);
        Chain c;
        c.autoLevel = false;
        c.begin(-60);
        c.run(sig);
        const Stats s = stats(c.out);
        report("13. Auto level off: the file is written as before", s);
        check(near(s.speechP80, c.inputP80(), 0.5f) && s.maxGain == 0.0f, "no gain applied");
    }

    {
        Chain c;
        c.voiceActivation = false;
        c.autoGain.resetLearning();
        Signal first;
        first.silence(1, -66);
        first.speak(10, -40, -66);
        c.begin(-66);
        c.run(first);
        const float firstLevel = stats(c.out, c.out.size() - framesFor(5)).speechP80;
        Signal second;
        second.rng.seed(7);
        second.silence(1, -66);
        second.speak(6, -40, -66);
        c.micGainDb = 12;  // the user raised the mic gain between the recordings
        c.begin(-54);
        c.run(second);
        const Stats s = stats(c.out);
        report("14. Mic gain raised by 12 dB between two recordings of the same voice", s);
        const Stats start = stats(c.out, 0, framesFor(3));
        check(start.maxPeak < -1.0f, "no clipping at the start of the second recording");
        printf("    first recording ended at %.1f dBFS, second starts at %.1f dBFS\n", firstLevel, start.speechP80);
        // The louder mic also lifts the noise, so the noise ceiling may leave a few dB less headroom.
        check(near(start.speechP80, firstLevel, 4.0f), "about the same level from the first words");
    }

    printf("\n%s: %d failure(s)\n", failures == 0 ? "PASSED" : "FAILED", failures);
    return failures == 0 ? 0 : 1;
}
