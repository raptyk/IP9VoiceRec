# Changelog

## 1.3.0 – 2026-10-25

- **Auto level**: recordings that came out too quiet to hear (distant or soft voice, low mic gain, no calibration)
  are now raised to a clear, steady level while they are written; loud voices are brought down before the limiter.
  Lookahead of 288 ms (the gain is ready for the first syllable, knocks and shouts are not clipped), only voiced
  frames count (no pumping in pauses), noise never raised above -42 dBFS, range -12..+30 dB. On by default,
  *Settings > Auto level*; the record screen shows the current gain ("auto +18").
- Host simulation of the recording chain, `tools/autogain_sim.cpp` (14 scenarios).

## 1.2.0 – 2026-10-25

- Part of the IP9 apps (https://cardputer.ip9.pl): the recorder is now **IP9 VoiceRec**, the
  repository `IP9VoiceRec` and the release images `IP9VoiceRec-<version>[-full].bin`.
- Built on [IP9Kit](https://github.com/raptyk/IP9Kit) 1.0.0, the core shared with the other IP9 apps
  (app shell, home carousel, menus, help, dialogs, keyboard, status LED, battery). Pop-ups close
  with `enter` or `esc` (footer says so); *About* shows the IP9 website.
- Settings (NVS) and recordings are kept.

## 1.1.1

- MP3 frames are written without a false padding bit (files from 1.0.0 / 1.1.0: `tools/repair_mp3.py`).

## 1.1.0

- New audio pipeline, adaptive voice activation and the LG Remote style UI.

## 1.0.0

- First release: MP3 encoder, voice activation, pre-roll, player.
