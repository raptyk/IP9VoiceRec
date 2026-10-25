// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#include "Recorder.h"

#include <M5Cardputer.h>
#include <string.h>

#include "audio/Dsp.h"
#include "hw/AudioHardware.h"
#include "storage/SdStorage.h"

namespace voicerec {

namespace {
constexpr uint32_t kCaptureStack = 6144;
constexpr uint32_t kWriterStack = 8192;
constexpr UBaseType_t kCapturePriority = 5;  // below the M5Unified mic task (6), above the UI (1)
constexpr UBaseType_t kWriterPriority = 3;
}  // namespace

Recorder& Recorder::instance() {
    static Recorder recorder;
    return recorder;
}

void Recorder::begin() {
    if (writerQueue_ != nullptr) {
        return;
    }
    writerQueue_ = xQueueCreate(kWriterQueueFrames, sizeof(WriterMessage));
    xTaskCreatePinnedToCore(captureTaskEntry, "capture", kCaptureStack, this, kCapturePriority, &captureTask_, 1);
    xTaskCreatePinnedToCore(writerTaskEntry, "mp3writer", kWriterStack, this, kWriterPriority, &writerTask_, 0);
}

// ---------------------------------------------------------------- UI side

void Recorder::configure(const RecorderConfig& config) {
    gainDb_ = config.gainDb;
    voiceActivation_ = config.voiceActivation;
    sensitivity_ = config.sensitivity;
    holdMs_ = config.holdMs;
    startNoise_ = config.noiseFloorDbfs;
    autoLevel_ = config.autoLevel;
    configChanged_ = true;
}

void Recorder::startMonitor() {
    Mode expected = Mode::Off;
    if (wanted_.compare_exchange_strong(expected, Mode::Monitor)) {
        micFailed_ = false;
        xTaskNotifyGive(captureTask_);
    }
}

void Recorder::stopMonitor() {
    Mode expected = Mode::Monitor;
    wanted_.compare_exchange_strong(expected, Mode::Off);
}

StartResult Recorder::start(const String& path, uint8_t bitrateKbps) {
    if (state_ != RecorderState::Idle) {
        return StartResult::Busy;
    }
    if (!encoder_.begin(kSampleRate, bitrateKbps)) {
        return StartResult::EncoderError;
    }
    file_ = SdStorage::instance().openForWrite(path);
    if (!file_) {
        encoder_.end();
        return StartResult::FileError;
    }

    path_ = path;
    summary_ = RecordingSummary();
    framesSeen_ = 0;
    framesWritten_ = 0;
    droppedFrames_ = 0;
    bytesWritten_ = 0;
    paused_ = false;
    writeError_ = false;
    micFailed_ = false;
    xQueueReset(writerQueue_);
    // The writer is idle (state Idle), so its Auto level state can be prepared from here.
    autoLevelSession_ = autoLevel_.load();
    autoGain_.beginSession();
    lookaheadHead_ = 0;
    lastAutoGain_ = dsp::dbToGain(autoGain_.currentGainDb());
    autoGainDb_ = autoGain_.currentGainDb();

    // Hand-over: everything above is visible to the tasks once they see the open session.
    state_ = RecorderState::Starting;
    sessionOpen_ = true;
    wanted_ = Mode::Record;
    xTaskNotifyGive(captureTask_);
    log_i("Recording to %s at %u kbps", path.c_str(), bitrateKbps);
    return StartResult::Ok;
}

void Recorder::stop() {
    if (sessionOpen_) {
        wanted_ = Mode::Off;
        xTaskNotifyGive(captureTask_);
    }
}

void Recorder::setPaused(bool paused) {
    if (paused_.exchange(paused) != paused && paused) {
        syncRequested_ = true;  // secure what was recorded so far while nothing new arrives
    }
}

void Recorder::captureLevels(size_t frames) {
    levelTarget_ = 0;
    levelCount_ = 0;
    levelTarget_ = frames < kMaxLevelFrames ? frames : kMaxLevelFrames;
}

size_t Recorder::copyLevels(float* out, size_t capacity) const {
    const size_t count = levelCount_.load() < capacity ? levelCount_.load() : capacity;
    memcpy(out, levels_, count * sizeof(float));
    return count;
}

// ---------------------------------------------------------------- capture task

void Recorder::captureTaskEntry(void* arg) {
    static_cast<Recorder*>(arg)->captureLoop();
}

void Recorder::captureLoop() {
    for (;;) {
        const Mode want = wanted_.load();
        if (sessionOpen_ && want != Mode::Record) {
            closeSession();
        }
        if (want == Mode::Off) {
            if (micRunning_) {
                stopMic();
            }
            ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(250));
            continue;
        }
        if (!micRunning_ && !startMic()) {
            micFailed_ = true;
            wanted_ = Mode::Off;
            continue;
        }
        if (configChanged_.exchange(false)) {
            applyConfig();
        }
        if (noiseRestart_.exchange(false)) {
            detector_.reset(startNoise_);
        }

        // Two buffers are always queued in the driver; queuing the third returns as soon as
        // the oldest one is full, which is then processed while the other two keep filling.
        int16_t* next = buffers_[(frameIndex_ + 2) % 3];
        if (!M5.Mic.record(next, kSamplesPerFrame, kSampleRate)) {
            log_e("Microphone stopped delivering data");
            micFailed_ = true;
            wanted_ = Mode::Off;
            continue;
        }
        processFrame(buffers_[frameIndex_ % 3]);
        ++frameIndex_;
    }
}

bool Recorder::startMic() {
    if (!AudioHardware::instance().startMic()) {
        return false;
    }
    frameIndex_ = 0;
    settleFrames_ = 0;
    silentStartFrames_ = 0;
    sessionStarted_ = false;
    dcBlocker_.reset();
    appliedGainDb_ = kNoGain;
    applyConfig();
    detector_.reset(startNoise_);
    M5.Mic.record(buffers_[0], kSamplesPerFrame, kSampleRate);
    M5.Mic.record(buffers_[1], kSamplesPerFrame, kSampleRate);
    micRunning_ = true;
    warmup_ = true;
    return true;
}

void Recorder::stopMic() {
    AudioHardware::instance().stopMic();
    micRunning_ = false;
    warmup_ = false;
    level_ = kSilenceDbfs;
    peak_ = kSilenceDbfs;
    voice_ = VoiceState::Silence;
}

void Recorder::applyConfig() {
    const int gainDb = gainDb_.load();
    if (appliedGainDb_ != kNoGain && gainDb != appliedGainDb_) {
        detector_.shiftNoise(static_cast<float>(gainDb - appliedGainDb_));
    }
    appliedGainDb_ = gainDb;
    gain_ = dsp::dbToGain(static_cast<float>(gainDb));
    detector_.configure(sensitivity_, holdMs_);
}

void Recorder::processFrame(int16_t* frame) {
    // Drop the start-up transient, and on the ADV the codec's silent warm-up as well.
    if (warmup_) {
        const dsp::FrameLevel raw = dsp::measure(frame, kSamplesPerFrame);
        if (raw.peakDbfs <= kSilenceDbfs && silentStartFrames_ < kMicMaxSilentStartFrames) {
            ++silentStartFrames_;
            return;
        }
        dcBlocker_.process(frame, kSamplesPerFrame);  // let the filter settle on real data
        if (++settleFrames_ < kMicSettleFrames) {
            return;
        }
        warmup_ = false;
        detector_.reset(startNoise_);
        return;
    }

    dcBlocker_.process(frame, kSamplesPerFrame);
    if (dsp::applyGain(frame, kSamplesPerFrame, gain_)) {
        clippedAt_ = millis();
    }
    const dsp::FrameLevel level = dsp::measure(frame, kSamplesPerFrame);
    const VoiceDecision decision = detector_.process(level.rmsDbfs);
    FrameInfo info;
    info.rmsDbfs = level.rmsDbfs;
    info.peakDbfs = level.peakDbfs;
    info.noiseDbfs = detector_.noiseFloor();
    info.voiced = decision.state == VoiceState::Voice;
    info.inputGainDb = static_cast<int8_t>(appliedGainDb_);

    level_ = level.rmsDbfs;
    peak_ = level.peakDbfs;
    voice_ = decision.state;
    noise_ = detector_.noiseFloor();
    trigger_ = detector_.triggerLevel();

    const size_t target = levelTarget_.load();
    const size_t count = levelCount_.load();
    if (count < target) {
        levels_[count] = level.rmsDbfs;
        levelCount_ = count + 1;
    }

    if (sessionOpen_) {
        handleRecording(frame, decision, info);
    }
}

void Recorder::handleRecording(const int16_t* frame, const VoiceDecision& decision, const FrameInfo& info) {
    if (!sessionStarted_) {
        // First frame after the warm-up: the recording really begins now.
        sessionStarted_ = true;
        preRoll_.reset();
        state_ = RecorderState::Recording;
    }
    if (syncRequested_.exchange(false)) {
        sendControl(WriterOp::Sync);
    }
    if (paused_ || writeError_) {
        return;
    }
    ++framesSeen_;

    if (!voiceActivation_) {
        sendFrame(frame, info);
        return;
    }
    if (decision.writing) {
        if (decision.onset) {
            for (size_t i = 0; i < preRoll_.count(); ++i) {
                sendFrame(preRoll_.frame(i), preRoll_.info(i));
            }
            preRoll_.reset();
        }
        sendFrame(frame, info);
    } else {
        preRoll_.push(frame, info);
        if (decision.released) {
            sendControl(WriterOp::Sync);  // a voice segment ended: make it safe on the card
        }
    }
}

void Recorder::sendFrame(const int16_t* frame, const FrameInfo& info) {
    outgoing_.op = WriterOp::Frame;
    outgoing_.info = info;
    memcpy(outgoing_.pcm, frame, kBytesPerFrame);
    if (xQueueSend(writerQueue_, &outgoing_, 0) == pdTRUE) {
        ++framesWritten_;
    } else {
        ++droppedFrames_;  // the card is too slow right now
    }
}

void Recorder::sendControl(WriterOp op) {
    // Control messages carry no audio; only the op is read by the writer. A Close must arrive
    // (the file would stay open otherwise), so it waits for room behind the queued frames.
    outgoing_.op = op;
    xQueueSend(writerQueue_, &outgoing_, op == WriterOp::Close ? portMAX_DELAY : 0);
}

void Recorder::closeSession() {
    sessionOpen_ = false;
    sessionStarted_ = false;
    state_ = RecorderState::Saving;
    sendControl(WriterOp::Close);
}

// ---------------------------------------------------------------- writer task

void Recorder::writerTaskEntry(void* arg) {
    static_cast<Recorder*>(arg)->writerLoop();
}

void Recorder::writerLoop() {
    for (;;) {
        if (xQueueReceive(writerQueue_, &incoming_, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        switch (incoming_.op) {
            case WriterOp::Frame: {
                if (!encoder_.isOpen() || writeError_) {
                    break;
                }
                writeFrame(incoming_.pcm, incoming_.info);
                if (millis() - lastSyncAt_ > kSyncIntervalMs) {
                    writeStaging(true);
                    file_.flush();
                    lastSyncAt_ = millis();
                }
                break;
            }
            case WriterOp::Sync:
                // End of a voice segment or a pause: everything heard so far goes to the card.
                flushLookahead();
                if (file_) {
                    writeStaging(true);
                    file_.flush();
                    lastSyncAt_ = millis();
                }
                break;
            case WriterOp::Close:
                flushLookahead();
                finishFile();
                break;
        }
    }
}

void Recorder::writeFrame(const int16_t* pcm, const FrameInfo& info) {
    if (!autoLevelSession_) {
        encodePcm(pcm);
        return;
    }
    // The frame waits in the lookahead window; the oldest one leaves with its gain.
    if (autoGain_.full()) {
        writeOldestLookahead();
    }
    const size_t slot = (lookaheadHead_ + autoGain_.pending()) % AutoGain::kLookaheadFrames;
    memcpy(lookahead_[slot], pcm, kBytesPerFrame);
    autoGain_.observe(info);
}

void Recorder::writeOldestLookahead() {
    int16_t* pcm = lookahead_[lookaheadHead_];
    const float gainDb = autoGain_.takeGainDb();
    const float gain = dsp::dbToGain(gainDb);
    dsp::applyGainRamp(pcm, kSamplesPerFrame, lastAutoGain_, gain);
    lastAutoGain_ = gain;
    autoGainDb_ = gainDb;
    lookaheadHead_ = (lookaheadHead_ + 1) % AutoGain::kLookaheadFrames;
    encodePcm(pcm);
}

void Recorder::flushLookahead() {
    while (autoGain_.pending() > 0) {
        writeOldestLookahead();
    }
}

void Recorder::encodePcm(const int16_t* pcm) {
    if (!encoder_.isOpen() || writeError_) {
        return;
    }
    int length = 0;
    const uint8_t* mp3 = encoder_.encodeFrame(pcm, &length);
    writeEncoded(mp3, length);
}

void Recorder::writeEncoded(const uint8_t* data, size_t length) {
    if (data == nullptr || length == 0) {
        return;
    }
    if (stagingCount_ + length > sizeof(staging_)) {
        writeStaging(true);
    }
    memcpy(staging_ + stagingCount_, data, length);
    stagingCount_ += length;
    if (stagingCount_ >= kSdWriteBlock) {
        writeStaging(false);
    }
}

void Recorder::writeStaging(bool all) {
    // Whole 4 KB blocks keep the card fast; the remainder waits unless `all`.
    const size_t length = all ? stagingCount_ : (stagingCount_ / kSdWriteBlock) * kSdWriteBlock;
    if (length == 0 || !file_ || writeError_) {
        return;
    }
    const size_t written = file_.write(staging_, length);
    if (written != length) {
        log_e("SD write failed (%u of %u bytes)", written, length);
        writeError_ = true;
    }
    bytesWritten_ += written;
    memmove(staging_, staging_ + length, stagingCount_ - length);
    stagingCount_ -= length;
}

void Recorder::finishFile() {
    if (encoder_.isOpen()) {
        int length = 0;
        const uint8_t* tail = encoder_.flush(&length);
        writeEncoded(tail, length);
        encoder_.end();
    }
    writeStaging(true);
    if (file_) {
        file_.close();
    }
    stagingCount_ = 0;
    lastSyncAt_ = millis();

    summary_.path = path_;
    summary_.bytes = bytesWritten_;
    summary_.audioMs = framesWritten_ * kFrameMs;
    summary_.droppedFrames = droppedFrames_;
    summary_.writeError = writeError_;
    log_i("Recording saved: %s, %u bytes, %u ms", path_.c_str(), summary_.bytes, summary_.audioMs);
    state_ = RecorderState::Idle;
}

}  // namespace voicerec
