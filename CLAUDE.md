# IP9 VoiceRec

Dyktafon MP3 z aktywacją głosem, kalibracją mikrofonu i odtwarzaczem dla M5Stack Cardputer i Cardputer ADV (ESP32-S3).
Część marki IP9 (https://cardputer.ip9.pl), repozytorium `IP9VoiceRec`.

## Wspólny rdzeń IP9Kit

- Powłoka aplikacji (`ip9::Shell`: stos ekranów, pasek statusu, stopka, toasty, wygaszanie, LED), `ListScreen`,
  `CarouselScreen`, `HelpScreen`, `ConfirmScreen` / `InfoScreen`, `painter`, `Device`, `KeyboardInput`, `StatusLed`
  pochodzą z biblioteki **IP9Kit** (osobne repozytorium, `../IP9Kit`).
- `platformio.ini`: `[ip9kit] source` przypina tag IP9Kit; lokalnie `ip9kit-local.ini` (kopia `.example`,
  git-ignored) przełącza na `symlink://../IP9Kit`.
- `src/ui/Screen.h` wprowadza `using namespace ip9;` do `voicerec` i definiuje klasy `Screen` / `ListScreen` aplikacji (z `app_`).
- Co jest wspólne dla wielu aplikacji – do IP9Kit (z nową wersją i tagiem), nie tutaj.

## Zasady

- Kod w 100% po angielsku; wszystkie teksty UI w `src/ui/Strings.h` (stopki Font0: max ~38 znaków, 240 px).
- Include'y względem `src/` (`#include "audio/Recorder.h"`), przestrzeń nazw `voicerec`.
- Wątek UI (Arduino `loop`, rdzeń 1) **nigdy nie czeka na audio**: tylko wysyła polecenia i odczytuje atomiki.

## Platforma

- PlatformIO, Arduino, `espressif32@6.9.0`, ESP32-S3, 8 MB, `default_8MB.csv`. `pio run` buduje i kopiuje obrazy do `release/`.
- `ARDUINO_USB_MODE=1`, `ARDUINO_USB_CDC_ON_BOOT=1`, `serial_baudrate = 0`: UART0 nie może dotykać GPIO43 (zegar mikrofonu/głośnika).
- **Bez `-ffast-math`**: z nim shine liczył 215,999… zamiast 216 bajtów na ramkę i oznaczał każdą ramkę jako „padded”
  (pliki 1.0.0/1.1.0 nieodtwarzalne; naprawa: `tools/repair_mp3.py`). `lib/shine/src/layer3.c` liczy teraz sloty
  na liczbach całkowitych – zmiana względem upstreamu shine, nie nadpisywać przy aktualizacji biblioteki.
- Weryfikacja nagrania na PC: `ffmpeg -v error -i rec_0001.mp3 -f null -` musi nie wypisać żadnego błędu.
- `M5.config()`: `internal_mic = true`, `internal_spk = true` – M5Unified ustawia piny (i ES8311 na ADV).

## Audio – wątki i własność sprzętu

- `hw/AudioHardware`: mikrofon i głośnik dzielą GPIO43 (i 41 na ADV) – w danej chwili działa tylko jedno.
  Każdą stronę włącza/wyłącza **wyłącznie zadanie, które jej używa**; `claim()` czeka max 600 ms na zwolnienie drugiej.
- `audio/Recorder` – dwa zadania:
  - `capture` (rdzeń 1, prio 5; zadanie mikrofonu M5Unified: prio 6, rdzeń 1, 12 buforów DMA) – jedyny użytkownik `M5.Mic`.
    Trzy bufory po 576 próbek: dwa zawsze w kolejce sterownika, `record()` trzeciego wraca, gdy najstarszy jest pełny
    (bez callbacków, bez utraty slotów). Ramka: DC blocker 80 Hz → wzmocnienie + limiter (`audio/Dsp`) → poziom →
    `VoiceDetector` → pre-roll / kolejka zapisu.
  - `mp3writer` (rdzeń 0, prio 3): kolejka 32 ramek PCM (1,15 s; każda z `FrameInfo`: poziom, szczyt, szum, głos,
    wzmocnienie) → Auto level (`audio/AutoGain`, okno 8 ramek = 288 ms, rampa + limiter `dsp::applyGainRamp`)
    → shine → bloki 4 KB na kartę; `Sync`/`Close` najpierw opróżniają okno; `Sync` po końcu
    segmentu głosu i co 10 s, `Close` kończy plik i przełącza stan na `Idle`.
  - `start()` (UI) otwiera plik i enkoder, `stop()` tylko ustawia żądanie; stany: `Idle → Starting → Recording → Saving → Idle`.
  - Tryb `Monitor` (Mic check): mikrofon i detektor bez zapisu. Rozgrzewka: odrzucenie ~290 ms po starcie
    i zerowych ramek ES8311 (do ~1 s).
- `audio/Player` (rdzeń 0, prio 4): zadanie posiada plik, dekoder Helix i głośnik; UI wysyła polecenia przez kolejkę.
  `M5.Speaker.playRaw` gra z pamięci wywołującego – PCM jest kopiowany do 3 rotujących buforów, nowy dopiero gdy w kolejce
  głośnika są < 2. Długość i przewijanie z nagłówka MP3 (`audio/Mp3Info`, CBR).

## Mikrofon i aktywacja głosem

- Cardputer: PDM SPM1423, `magnification = 16` (×8). ADV: ES8311, M5Unified zostawia PGA na 0 dB – `AudioHardware::tuneAdvCodec()`
  ustawia PGA +21 dB (reg 0x14 = 0x17, jak M5Stack StopWatch) po `Mic.begin()`; `magnification = 2` (×1).
- Programowe wzmocnienie 0–30 dB (krok 3) z miękkim limiterem od -2,5 dBFS. Domyślnie +12 dB (Cardputer) / +18 dB (ADV).
  Detektor głosu i mierniki widzą sygnał **przed** Auto level – progi aktywacji nie zależą od AGC.
- Auto level (`AutoGain`, ustawienie `agc` w NVS, domyślnie wł., brane przy `start()`): poziom mowy = obwiednia
  sylab ramek z głosem (>6 dB nad szumem; skok w górę, opadanie 0,35 dB/ramkę) wygładzona (w górę 0,3, w dół 0,04);
  wzmocnienie = -19 dBFS − poziom mowy, zakres -12…+30 dB, nie wyżej niż szum → -42 dBFS (ale szum nigdy nie
  wymusza < 0 dB). Do ~1 s głosu w sesji skacze od razu; potem w górę max 0,2 dB/ramkę i tylko gdy w oknie jest głos,
  w dół 1 dB/ramkę. Strażnik szczytów: szczyty tej i 2 następnych ramek ≤ -3 dBFS. Zmiana wzmocnienia mikrofonu
  przesuwa wyuczony poziom; wzmocnienie przechodzi na następne nagranie (×0,75). Zmiany sprawdzać symulacją
  `tools/autogain_sim.cpp` (14 scenariuszy, wszystkie muszą przejść).
- `VoiceDetector`: próg = śledzony szum + max(margines czułości, 3 × odchylenie tła); margines 16/12/9/7/5 dB dla czułości 1–5.
  Między głosami szum = średnia tła (wolno w górę, szybciej w dół); w trakcie głosu tylko w dół + powolne podciąganie do
  minimum z 2 s (wentylator nie trzyma nagrania). Start: 2 ramki nad progiem; koniec: poniżej progu zwolnienia przez czas
  podtrzymania. Pierwsza sekunda po resecie: szum = minimum + 2 odchylenia. Zachowanie sprawdzone symulacją scenariuszy
  (cisza/mowa, zły start, wentylator, kliknięcia, 60 s mowy).
- Kalibracja (`audio/Calibration`): 3 s ciszy (mediana + rozrzut P90) i 4,5 s mowy (P80 ramek >8 dB nad szumem) →
  wzmocnienie tak, by mowa ≈ -20 dBFS, czułość z rozrzutu tła i SNR. Zmierzony szum (przeliczony na nowe wzmocnienie)
  jest tylko wartością startową detektora.

## Karta SD i ustawienia

- SD na FSPI: SCK 40, MISO 39, MOSI 14, CS 12, 20 MHz. `SdStorage` serializuje wywołania (rekurencyjny mutex),
  wolne miejsce liczone tylko po montażu / nagraniu / usunięciu (liczenie przechodzi FAT). Nowa karta wykrywana co 10 s,
  nigdy w trakcie nagrywania/odtwarzania.
- Nagrania: `/recordings/rec_NNNN.mp3`, numer nigdy nie nadpisuje istniejącego pliku; puste nagranie (0 ramek) jest usuwane.
- Ustawienia w NVS (`Preferences`, przestrzeń `voicerec`), nie na karcie. Zmiany w trakcie nagrywania zapisywane po jego końcu.

## UI

- `App` (na `ip9::Shell`): `statusLine()` = chip sekcji + stan karty, `updateStatusLed()` = stan nagrywania,
  `setDisplayPower()` – 30 s przyciemnienie, wyłączenie wg ustawień (klawisz budzący jest połykany przez Shell).
- Ekrany: `HomeScreen` (karuzela), `RecordScreen`, `LibraryScreen` + `PlayerScreen`, `MicCheckScreen`,
  ustawienia (`makeSettingsScreen` → `ListScreen`), `HelpScreen`, `ConfirmScreen` / `InfoScreen`.
- Klawisze jak w innych aplikacjach IP9: `;` `.` `,` `/` strzałki, `esc` to klawisz `` ` ``, `del` = wstecz, `tab` = pomoc, `G0` = enter.
