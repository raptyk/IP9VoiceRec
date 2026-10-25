# CardputerVoiceRec

Inteligentny dyktafon cyfrowy z kompresją audio (MP3/Shine), zapisem na kartę SD, systemem aktywacji głosem (VAS / VAD) z buforem pre-roll, adaptacyjną percentylową kalibracją szumów otoczenia oraz wbudowanym odtwarzaczem nagrań (Helix MP3) dla M5Stack Cardputer i Cardputer ADV (ESP32-S3).

## Architektura i Założenia Projektu

- **Platforma**: PlatformIO, framework Arduino, Espressif ESP32-S3 (`board = esp32-s3-devkitc-1`, 240 MHz, 8 MB Flash, partycje `default_8MB.csv`).
- **USB & Serial**: `-DARDUINO_USB_MODE=1` i `-DARDUINO_USB_CDC_ON_BOOT=1` – UART0 nie dotyka GPIO 43 (taktowanie mikrofonu/kodeka).
- **Inicjalizacja Audio w M5Unified**:
  - `M5.config()` musi mieć `cfg.internal_mic = true; cfg.internal_spk = true;`, aby `M5Unified` poprawnie przypisał piny i zarejestrował procedury startowe kodeka ES8311 (Cardputer ADV).
  - Natychmiast po starcie `AudioModeManager::begin()` gasi oba peryferia (`Mic.end()`, `Speaker.end()`) w celu ochrony wspólnej linii zegarowej GPIO 43/41.
- **Audio Half-Duplex Manager**:
  - Mikrofon (`I2S_NUM_0`) i Głośnik (`I2S_NUM_1`) dzielą GPIO 43 (i GPIO 41 na ADV).
  - Peryferia są rygorystycznie wykluczające się w czasie: przejście do `Record` zamyka głośnik i odczekuje 15 ms przed startem mikrofonu. Przejście do `Playback` zamyka mikrofon i odczekuje 15 ms przed startem głośnika.
  - Wzmocnienie mikrofonu dostosowane do modelu: `magnification = 16` na PDM MEMS (standardowy Cardputer) oraz `magnification = 2` na ES8311 z analogowym PGA (Cardputer ADV) dla ochrony przed obcinaniem sygnału.
- **Akwizycja Audio: Podwójne Buforowanie Ping-Pong DMA**:
  - Obsługa przez `M5.Mic.setBufferReleaseCallback()` i dwa bufory `dmaBuf_[2][576]`.
  - Sprzęt I2S DMA na bieżąco wypełnia bufor, a po jego zakończeniu wywołuje callback i powiadamia zadanie `AudioCaptureTask` przez `xTaskNotifyGive()`.
  - Zadanie natychmiast re-armuje ten sam slot do kolejki DMA, dzięki czemu próbkowanie jest w 100% ciągłe i pozbawione jakichkolwiek przerw.
- **Wielowątkowa Karta SD (SPI2_HOST / FSPI)**:
  - Piny: SCK=40, MISO=39, MOSI=14, CS=12.
  - Chroniona sprzętowym muteksem `SdStorage::lock() / unlock()` zapobiegającym kolizjom między zapisem MP3 na Core 0 a sprawdzaniem miejsca/listowaniem na Core 1.
  - Bufor sektorowy 8 KB, zapis w blokach po 4096 B. Bezpieczny `flush()` przy przejściu w fazę ciszy VAS lub zakończeniu nagrywania.
- **Silnik kompresji i dekompresji**:
  - **Enkoder**: `libshine` (Fixed-point MP3 Encoder). Złota zasada: ramka $N = 576$ próbek (MPEG-2 Layer 3, 16 kHz mono, 48 kbps). 1 ramka PCM = dokładnie 1 ramka MP3 (216 B).
  - **Dekoder**: `libhelix-mp3` (Fixed-point MP3 Decoder) z synchronizacją tempa rzeczywistego (`while (Speaker.isPlaying(0) != 0)`).
- **Voice Activated System (VAS / VAD)**:
  - DC-Blocker IIR 80 Hz na sprzętowym FPU ESP32-S3.
  - Statystyczna percentylowa kalibracja szumów tła ($P_{40}$ / $P_{85}$) na próbie 2.5 s.
  - Bufor kołowy Pre-Roll 360 ms (10 ramek po 576 próbek) eliminujący ucinanie pierwszej głoski.
  - Histereza ataku/zwolnienia ($T_{\text{attack}}$ vs $T_{\text{release}} = T_{\text{attack}} - 5\text{ dB}$).
  - Hangover timer (domyślnie 1.5 s).
- **Wielowątkowość FreeRTOS**:
  - Rdzeń 1: `AudioCaptureTask` (Priorytet 6), UI & Keyboard (Priorytet 1).
  - Rdzeń 0: `Mp3WriterTask` (Priorytet 3) ze `StreamBuffer` 36 KB (trigger level 1152 B) oraz `AudioPlayerTask` (Priorytet 2).
