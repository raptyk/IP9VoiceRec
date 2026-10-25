# CardputerVoiceRec

Inteligentny dyktafon z kompresją audio (MP3/Shine), zapisem na kartę SD, systemem aktywacji głosem (VAS / VAD) z buforem pre-roll, adaptacyjną percentylową kalibracją szumów otoczenia oraz wbudowanym odtwarzaczem nagrań (Helix MP3) dla M5Stack Cardputer i Cardputer ADV (ESP32-S3).

## Architektura i Założenia Projektu

- **Platforma**: PlatformIO, framework Arduino, Espressif ESP32-S3 (`board = esp32-s3-devkitc-1`, 240 MHz, 8 MB Flash, partycje `default_8MB.csv`).
- **USB & Serial**: `-DARDUINO_USB_MODE=1` i `-DARDUINO_USB_CDC_ON_BOOT=1` – UART0 nie dotyka GPIO 43 (taktowanie mikrofonu/kodeka).
- **Audio Half-Duplex Manager**:
  - Mikrofon (`I2S_NUM_0`) i Głośnik (`I2S_NUM_1`) dzielą GPIO 43 (i GPIO 41 na ADV).
  - Nigdy nie działają jednocześnie. Przed startem głośnika następuje `Mic.end()`, a przed startem nagrywania `Speaker.end()`.
- **Karta SD (SPI2_HOST / FSPI)**:
  - Piny: SCK=40, MISO=39, MOSI=14, CS=12.
  - Wyraźna separacja od magistrali wyświetlacza ST7789 (`SPI3_HOST`).
  - Bufor sektorowy 8 KB, zapis w blokach po 4096 B. `flush()` wywoływany w fazie ciszy VAS lub przy zatrzymaniu nagrywania.
- **Silnik kompresji i dekompresji**:
  - **Enkoder**: `libshine` (Fixed-point MP3 Encoder). Złota zasada: ramka $N = 576$ próbek (MPEG-2 Layer 3, 16 kHz mono, 48 kbps). 1 ramka PCM = dokładnie 1 ramka MP3 (216 B).
  - **Dekoder**: `libhelix-mp3` (Fixed-point MP3 Decoder) do odtwarzania przez `M5.Speaker.playRaw()`.
- **Voice Activated System (VAS / VAD)**:
  - DC-Blocker IIR 80 Hz na sprzętowym FPU ESP32-S3.
  - Statystyczna percentylowa kalibracja szumów tła ($P_{40}$ / $P_{85}$) na próbie 2.5 s.
  - Bufor kołowy Pre-Roll 360 ms (10 ramek po 576 próbek) eliminujący ucinanie pierwszej głoski.
  - Histereza ataku/zwolnienia ($T_{\text{attack}}$ vs $T_{\text{release}} = T_{\text{attack}} - 5\text{ dB}$).
  - Hangover timer (domyślnie 1.5 s).
  - Brak zrywania strumienia MP3 – spójność ramek.
- **Wielowątkowość FreeRTOS**:
  - Rdzeń 1: `AudioCaptureTask` (Priorytet 6), UI & Keyboard (Priorytet 1).
  - Rdzeń 0: `Mp3WriterTask` (Priorytet 3) ze `StreamBuffer` 36 KB (trigger level 1152 B).
- **Interfejs Użytkownika**:
  - Wzorowany na `CardputerLGRemote` (architektura ekranów, `Painter`, podwójne buforowanie `M5Canvas` 240x135, dynamiczny wskaźnik VU-meter z markerem progu ciszy).
