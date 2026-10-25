// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#pragma once

// All user-visible text in one place (ready for translation).
// Screen budget: 240 px. Footers use the 6 px font (max ~38 characters);
// long texts are word-wrapped by the painter.
namespace voicerec::str {

constexpr const char* kAppName = "Voice Recorder";

// Status bar chips (section of the current screen).
constexpr const char* kChipApp = "VOICE";
constexpr const char* kChipRecord = "REC";
constexpr const char* kChipLibrary = "FILES";
constexpr const char* kChipPlayer = "PLAY";
constexpr const char* kChipMicCheck = "MIC";

// Status bar text.
constexpr const char* kStatusNoCard = "No SD card";
constexpr const char* kStatusRecordings = "%u recordings, %s free";
constexpr const char* kStatusOneRecording = "1 recording, %s free";

// Home launcher
constexpr const char* kFooterHome = "<> pick  enter open  r record  tab help";
constexpr const char* kCardRecordTitle = "Record";
constexpr const char* kCardRecordText = "Starts at once. Voice activation skips the silence, so files stay small.";
constexpr const char* kCardRecordTextAlways = "Starts at once and records everything: voice activation is off.";
constexpr const char* kCardLibraryTitle = "Recordings";
constexpr const char* kCardLibraryText = "Listen to your recordings, skip around and delete old ones.";
constexpr const char* kCardMicCheckTitle = "Mic check";
constexpr const char* kCardMicCheckText = "Live level meter. Calibration measures room and voice, then sets gain.";
constexpr const char* kCardSettingsTitle = "Settings";
constexpr const char* kCardSettingsText = "Voice activation, sensitivity, gain, quality, screen and light.";
constexpr const char* kCardHelpTitle = "Help";
constexpr const char* kCardHelpText = "All keys at a glance.";
constexpr const char* kReadySpace = "Ready: space for %s";
constexpr const char* kInsertCard = "Insert an SD card";
constexpr const char* kCardFull = "The SD card is full";
constexpr const char* kRecordingsNone = "No recordings yet";
constexpr const char* kRecordingsCount = "%u recordings";
constexpr const char* kRecordingsOne = "1 recording";
constexpr const char* kCalibrated = "Calibrated, gain +%d dB";
constexpr const char* kNotCalibrated = "Not calibrated yet - recommended";
constexpr const char* kSettingsSummaryOn = "Voice activation on, sensitivity %u";
constexpr const char* kSettingsSummaryOff = "Voice activation off";

// Key caps (drawn) and their labels.
constexpr const char* kKeyEnter = "ENTER";
constexpr const char* kKeySpace = "SPACE";
constexpr const char* kKeyV = "V";
constexpr const char* kKeyD = "D";
constexpr const char* kKeySeek = ", /";
constexpr const char* kKeyBrackets = "[ ]";
constexpr const char* kKeyMinusPlus = "- =";
constexpr const char* kLabelSave = "Save";
constexpr const char* kLabelPause = "Pause";
constexpr const char* kLabelResume = "Resume";
constexpr const char* kLabelPlay = "Play";
constexpr const char* kLabelVoiceOn = "Voice on";
constexpr const char* kLabelVoiceOff = "Voice off";
constexpr const char* kLabelSeek = "-+10 s";
constexpr const char* kLabelDelete = "Delete";
constexpr const char* kLabelCalibrate = "Calibrate";
constexpr const char* kLabelSens = "Sens %u";
constexpr const char* kLabelGain = "Gain";

// Record screen
constexpr const char* kBandStarting = "STARTING...";
constexpr const char* kBandListening = "LISTENING";
constexpr const char* kBandRecording = "RECORDING";
constexpr const char* kBandPaused = "PAUSED";
constexpr const char* kBandSaving = "SAVING...";
constexpr const char* kSkipped = "silence skipped %s";
constexpr const char* kRecordsEverything = "records everything";
constexpr const char* kSpaceLeft = "%s left";
constexpr const char* kFooterRecord = "[ ] sensitivity  - = gain  tab help";
constexpr const char* kToastSensitivity = "Sensitivity %u: %s";
constexpr const char* kToastGain = "Mic gain +%d dB";
constexpr const char* kToastVoiceOn = "Voice activation on: silence is skipped";
constexpr const char* kToastVoiceOff = "Voice activation off: records everything";
constexpr const char* kSaved = "Saved %s (%s)";
constexpr const char* kNothingSaved = "No voice was heard - nothing saved";
constexpr const char* kLostAudio = "Saved, but the SD card was too slow: %s of audio lost";
constexpr const char* kErrorCard = "SD card full or removed - recording stopped";
constexpr const char* kErrorMic = "The microphone did not start";
constexpr const char* kErrorNoCard = "Insert an SD card first";
constexpr const char* kErrorCardFull = "The SD card is full";
constexpr const char* kErrorFile = "Cannot create a file on the SD card";
constexpr const char* kErrorBusy = "Still saving the last recording";

// Recordings (library)
constexpr const char* kLibraryTitle = "Recordings";
constexpr const char* kLibraryEmptyTitle = "No recordings yet";
constexpr const char* kLibraryEmptyText = "Press r to start recording.";
constexpr const char* kLibraryDetail = "%s. enter play, d delete, r record";
constexpr const char* kFooterLibrary = "enter play  d delete  esc back";
constexpr const char* kFooterLibraryEmpty = "r record  esc back";
constexpr const char* kConfirmDelete = "Delete %s?";
constexpr const char* kConfirmDeleteDetail = "The file is removed from the SD card.";
constexpr const char* kDeleted = "Deleted";
constexpr const char* kDeleteFailed = "Could not delete the file";

// Player
constexpr const char* kBandPlaying = "PLAYING";
constexpr const char* kBandFinished = "FINISHED";
constexpr const char* kPlayError = "Cannot play this file";
constexpr const char* kVolume = "Volume %u";
constexpr const char* kFooterPlayer = "; . prev/next  - = volume  esc back";

// Mic check and calibration
constexpr const char* kBandMicCheck = "MIC CHECK";
constexpr const char* kBandCalQuiet = "CALIBRATION 1/2";
constexpr const char* kBandCalSpeak = "CALIBRATION 2/2";
constexpr const char* kGainShort = "Gain +%d dB";
constexpr const char* kVoiceDetected = "Voice detected";
constexpr const char* kVoiceHold = "Voice (holding)";
constexpr const char* kNoVoice = "No voice";
constexpr const char* kNoiseInfo = "noise %d dB, trigger %d dB";
constexpr const char* kVoiceActivationOffInfo = "voice activation off";
constexpr const char* kMicStarting = "Starting the microphone...";
constexpr const char* kCalQuietTitle = "Stay quiet for 3 s";
constexpr const char* kCalQuietText = "Measuring the background noise of the room.";
constexpr const char* kCalSpeakTitle = "Now speak normally";
constexpr const char* kCalSpeakText = "Count to ten at the distance you will record from.";
constexpr const char* kCalDoneTitle = "Calibrated";
constexpr const char* kCalNoVoiceTitle = "No voice heard";
constexpr const char* kCalResult = "%s, gain +%d dB, sensitivity %u";
constexpr const char* kCalNoVoiceBody = "%s measured. Gain unchanged.";
constexpr const char* kCalNoVoiceDetail = "Speak closer and run it again (enter).";
constexpr const char* kCalSnrGood = "Your voice is %d dB above the noise: good.";
constexpr const char* kCalSnrOk = "Your voice is %d dB above the noise: OK.";
constexpr const char* kCalSnrLow = "Only %d dB above the noise: get closer.";
constexpr const char* kRoomQuiet = "Quiet room";
constexpr const char* kRoomNormal = "Normal room";
constexpr const char* kRoomNoisy = "Noisy room";
constexpr const char* kFooterMicCheck = "v voice act.  tab help  esc back";
constexpr const char* kFooterCalibrating = "esc cancel";

// Settings
constexpr const char* kSettingsTitle = "Settings";
constexpr const char* kOn = "On";
constexpr const char* kOff = "Off";
constexpr const char* kNever = "Never";
constexpr const char* kSensitivityNames[] = {"Low", "Medium", "Normal", "High", "Max"};
constexpr const char* kSetVoiceActivation = "Voice activation";
constexpr const char* kSetVoiceActivationText = "Records only while someone speaks; silence is skipped, files stay small.";
constexpr const char* kSetSensitivity = "Sensitivity";
constexpr const char* kSetSensitivityText = "Higher catches quieter voices, but also more noise. Live: [ and ].";
constexpr const char* kSetHold = "Keep after voice";
constexpr const char* kSetHoldText = "How long recording goes on after the voice stops. Keeps short pauses.";
constexpr const char* kSetGain = "Mic gain";
constexpr const char* kSetGainText = "Amplifies the microphone. Mic check calibration sets it for you.";
constexpr const char* kSetQuality = "Quality";
constexpr const char* kSetQualityText = "MP3 bitrate. 32: 14 MB/h, 48: 21 MB/h (speech), 64: 28 MB/h.";
constexpr const char* kSetVolume = "Speaker volume";
constexpr const char* kSetVolumeText = "Playback volume. Also - and = while playing.";
constexpr const char* kSetBrightness = "Brightness";
constexpr const char* kSetBrightnessText = "Screen brightness. It dims after 30 s without a key.";
constexpr const char* kSetScreenOff = "Screen off after";
constexpr const char* kSetScreenOffText = "Recording goes on with the screen off. Any key wakes it up.";
constexpr const char* kSetStatusLed = "Status light";
constexpr const char* kSetStatusLedText = "Red = voice recorded, pulsing = listening, amber = paused.";
constexpr const char* kSetMicCheck = "Mic check";
constexpr const char* kSetMicCheckText = "Level meter and calibration of gain and sensitivity.";
constexpr const char* kSetStorage = "SD card";
constexpr const char* kSetStorageText = "Recordings are in the folder /recordings as MP3 files.";
constexpr const char* kSetAbout = "About";
constexpr const char* kSetAboutText = "Version, device and memory.";
constexpr const char* kStorageInfo = "%s free of %s";
constexpr const char* kAboutTitle = "Voice Recorder %s";
constexpr const char* kAboutBody = "%s, free memory %u KB";
constexpr const char* kAboutDetail = "(c) 2026 Patryk Strzelecki, MIT licence";

// Help
constexpr const char* kHelpTitle = "Help";
constexpr const char* kHelpFooter = "< > page  esc close";

// Shared
constexpr const char* kConfirmFooter = "enter yes  esc no";
constexpr const char* kFooterInfo = "enter ok";
constexpr const char* kFooterList = "enter select  esc back";
constexpr const char* kFooterValue = "< > change  esc back";
constexpr const char* kHours = "%u h";
constexpr const char* kMinutes = "%u min";

}  // namespace voicerec::str
