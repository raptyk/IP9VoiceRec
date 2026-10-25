# Changelog

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
