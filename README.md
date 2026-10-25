# IP9 VoiceRec

Dyktafon cyfrowy dla **M5Stack Cardputer** i **Cardputer ADV** (ESP32-S3): nagrywa do plików **MP3** na kartę SD,
z **aktywacją głosem**, która pomija ciszę, kalibracją mikrofonu i wbudowanym odtwarzaczem.
Jedna z aplikacji IP9 dla Cardputera – **https://cardputer.ip9.pl**. Interfejs wspólny z innymi aplikacjami IP9
(m.in. [IP9 LG Remote](https://github.com/raptyk/IP9LGRemote)): karuzela kart na starcie, kolorowe ekrany sekcji,
rysowane klawisze, podpowiedzi w stopce i opis każdej opcji.

---

## Funkcje

* **MP3 w locie** (`libshine`): 16 kHz, mono, 32 / 48 / 64 kbit/s. Przy 48 kbit/s ok. 21 MB na godzinę mowy.
  Pliki `rec_0001.mp3`, `rec_0002.mp3`… w katalogu `/recordings` – otwierają się na każdym komputerze i telefonie.
* **Aktywacja głosem, która dopasowuje się do pomieszczenia**:
  - próg nie jest stałą wartością w dBFS, tylko marginesem nad **na bieżąco śledzonym poziomem szumu**
    (średnia tła i jego zmienność); ta sama czułość działa w cichej sypialni i w biurze,
  - start po dwóch głośnych ramkach z rzędu (72 ms) – kliknięcia klawiszy i stuknięcia nie uruchamiają nagrania,
  - **pre-roll 360 ms** – pierwsza sylaba nie jest ucinana,
  - **podtrzymanie** (domyślnie 1,5 s) – krótkie pauzy między słowami nie dzielą nagrania,
  - włączony nagle wentylator podtrzymuje nagranie najwyżej kilka sekund, potem detektor uznaje go za tło.
* **Mic check i kalibracja** (2 kroki, ok. 8 s): 3 s ciszy (szum tła i jego zmienność), potem 4,5 s mowy
  (poziom głosu). Kalibracja ustawia **wzmocnienie** tak, by mowa trafiała w ok. -20 dBFS, i dobiera **czułość**
  do odstępu głosu od szumu. Na żywo: miernik z zaznaczonym poziomem szumu i progiem wyzwalania.
* **Wzmocnienie mikrofonu**: Cardputer – mikrofon PDM ×8 + programowe 0…30 dB; Cardputer ADV – kodek ES8311
  z analogowym PGA +21 dB (M5Unified zostawia go na 0 dB, stąd wcześniejszy zbyt cichy mikrofon) + programowe
  0…30 dB. Głośne fragmenty są łagodnie limitowane zamiast twardo obcinane.
* **Odtwarzacz** (Helix MP3): pauza, przewijanie ±10 s, poprzedni/następny plik, głośność, usuwanie.
* **Wskaźnik LED**: ciągły czerwony = nagrywany głos, pulsujący czerwony = nasłuch, bursztynowy = pauza,
  pulsujący biały = zapisywanie, dwa zielone mignięcia = zapisano, dwa czerwone = błąd.
* **Ekran gaśnie podczas nagrywania** (ustawialne), nagrywanie trwa dalej; klawisz budzący ekran nie jest
  przekazywany dalej, więc nie zatrzyma przypadkiem nagrania.

---

## Sterowanie

### Ekran startowy

| Klawisz | Akcja |
| :--- | :--- |
| `,` `/` (oraz `;` `.`) | Wybór karty: Record, Recordings, Mic check, Settings, Help |
| `enter` / `G0` | Otwarcie karty (Record od razu zaczyna nagrywać) |
| `r` | Nagrywanie od razu (także z listy nagrań) |
| `1`–`5` | Otwarcie karty o danym numerze |
| `tab` | Pomoc |

### Nagrywanie

| Klawisz | Akcja |
| :--- | :--- |
| `enter` / `G0` / `esc` / `del` | Zatrzymanie i zapis |
| `space` | Pauza / wznowienie |
| `v` | Aktywacja głosem wł./wył. (na żywo) |
| `[` `]` | Czułość 1–5 |
| `-` `=` | Wzmocnienie mikrofonu (krok 3 dB) |

### Nagrania i odtwarzacz

| Klawisz | Akcja |
| :--- | :--- |
| `;` `.` | Poprzedni / następny plik |
| `enter` / `space` | Odtwarzanie / pauza |
| `,` `/` | Przewijanie -10 s / +10 s |
| `-` `=` | Głośność |
| `d` | Usunięcie pliku (z potwierdzeniem) |
| `esc` / `del` | Powrót |

### Mic check

| Klawisz | Akcja |
| :--- | :--- |
| `enter` / `c` | Kalibracja (cisza, potem mowa) |
| `[` `]` / `-` `=` | Czułość / wzmocnienie |
| `v` | Aktywacja głosem wł./wył. |

---

## Gotowe pliki (`release/`)

Kompilacja tworzy w `release/`:

1. `IP9VoiceRec-<wersja>.bin` – sama aplikacja, do uruchomienia z karty SD przez **M5Launcher**,
2. `IP9VoiceRec-<wersja>-full.bin` – pełny obraz (bootloader + partycje + aplikacja) pod adres `0x0`.

```bash
esptool.py --chip esp32s3 write_flash 0x0 release/IP9VoiceRec-1.2.0-full.bin
```

## Kompilacja

Wymaga [PlatformIO](https://platformio.org/). Wspólny rdzeń aplikacji IP9, [IP9Kit](https://github.com/raptyk/IP9Kit)
(powłoka aplikacji, menu, karuzela, pomoc, okienka, klawiatura, dioda LED, bateria), PlatformIO pobiera w wersji
przypiętej w `platformio.ini`.

```bash
pio run
```

Praca nad IP9Kit razem z dyktafonem: sklonuj IP9Kit obok tego katalogu (`../IP9Kit`) i skopiuj
`ip9kit-local.ini.example` do `ip9kit-local.ini`.

## Naprawa nagrań z wersji 1.0.0 i 1.1.0

Te wersje zapisywały w każdym nagłówku ramki MP3 bit „padding”, choć ramki go nie miały, więc żaden odtwarzacz
nie umiał ich odczytać. Dźwięk w plikach jest cały; skrypt czyści tylko ten bit (tworzy `*_fixed.mp3`,
z `--in-place` nadpisuje plik):

```bash
python tools/repair_mp3.py rec_0001.mp3
```

---

## Licencja

MIT. Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>.
Repozytorium: https://github.com/raptyk/IP9VoiceRec · więcej aplikacji IP9: https://cardputer.ip9.pl
