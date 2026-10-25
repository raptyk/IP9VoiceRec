# CardputerVoiceRec 🎙️

**CardputerVoiceRec** to profesjonalny, autonomiczny dyktafon cyfrowy z kompresją audio w locie do formatu **MP3** oraz inteligentnym systemem aktywacji głosem (**VAS / VAD**) stworzony dla **M5Stack Cardputer** oraz **Cardputer ADV** (ESP32-S3).

Zaprojektowany z myślą o rejestracji wielogodzinnych notatek głosowych, wykładów, spotkań oraz monitoringu audio bez marnowania miejsca na ciszę i szumy tła.

---

## Główne Funkcje

* **Kompresja MP3 w Czasie Rzeczywistym (`libshine`)**:
  - Dźwięk kompresowany na bieżąco na procesorze ESP32-S3 do formatu MPEG-2 Layer 3 (16 kHz, Mono, 48 kbps).
  - Zużycie miejsca: zaledwie **~6 kB na sekundę** mowy (karta SD 8 GB pomieści ponad **370 godzin nagrań**).
  - Obciążenie procesora: ~15% jednego rdzenia 240 MHz (wyłącznie arytmetyka stałoprzecinkowa).
  - Pliki `.mp3` odtwarzają się bezpośrednio na każdym komputerze, telefonie i w przeglądarce.

* **Inteligentny System Aktywacji Głosem (VAS / VAD)**:
  - **Zero utraty pierwszego słowa (Pre-Roll 360 ms)**: Bufor kołowy w pamięci RAM przechowuje ostatnie 360 ms dźwięku; w momencie wykrycia mowy historia jest natychmiast dopisywana przed pierwszym słowem.
  - **Histereza detekcji**: Osobny próg załączenia (Attack, np. -36 dBFS) i podtrzymania (Release, np. -41 dBFS) eliminujący drgania na granicy ciszy.
  - **Hangover Timer (1.5 s)**: Naturalne pauzy na oddech między zdaniami nie przerywają nagrania.
  - **Brak trzasków**: Spójność ramek MP3 (576 próbek) z łagodnym wznawianiem filtrów polifazowych.

* **Statystyczna Kalibracja Szumów Otoczenia (Ambient Noise Profiler)**:
  - 2.5-sekundowy test akustyczny pomieszczenia wyliczający percentyle $P_{40}$ (stabilne tło) i $P_{85}$ (piki szumów).
  - Niewrażliwy na przypadkowe stuknięcia w obudowę Cardputera.
  - Automatycznie dobiera optymalną czułość dla cichego pokoju, biura lub hałaśliwej ulicy.

* **Wbudowany Odtwarzacz z Dekoderem `Helix MP3`**:
  - Odsłuch nagrań bezpośrednio z karty SD przez wbudowany głośnik Cardputera.
  - Pasek postępu, pauza, regulacja głośności i usuwanie plików.

* **Rygorystyczna Architektura Sprzętowa (Half-Duplex)**:
  - Separacja peryferiów: mikrofon (`I2S_NUM_0`) i głośnik (`I2S_NUM_1`) dzielące GPIO 43 i 41 nigdy nie pracują równolegle, co zapobiega zwarciom wyjść zegarowych.
  - Dedykowana magistrala karty SD na kontrolerze `SPI2_HOST` (FSPI) odseparowana od wyświetlacza (`SPI3_HOST`).
  - Dedykowana kolejka FreeRTOS `StreamBuffer` (36 KB) eliminująca dropy audio przy opóźnieniach zapisu Flash na karcie SD.

* **Ergonomiczny Interfejs Retro/Cyberpunk**:
  - Płynny, dynamiczny wskaźnik **VU Meter** w dBFS z peak hold oraz pionowym znacznikiem progu detekcji.
  - Ekrany: *Menu Główne*, *Ekran Nagrywania*, *Biblioteka i Odtwarzacz*, *Kalibrator Szumów*, *Ustawienia*.

---

## Sterowanie Klawiaturą

### Menu Główne
| Klawisz | Akcja |
| :--- | :--- |
| **`[;]` / `[,]` / `[Strzałka w górę]`** | Przejście do poprzedniej pozycji |
| **`[.]` / `[/]` / `[Strzałka w dół]`** | Przejście do następnej pozycji |
| **`[Enter]` / `[Spacja]`** | Wybór podświetlonej opcji |
| **`[1]` .. `[4]`** | Bezpośredni skrót do ekranu (1: Rec, 2: Play, 3: Calib, 4: Set) |
| **`[R]`** | Szybki start nagrywania |
| **`[C]`** | Szybka kalibracja szumów |

### Ekran Nagrywania (Dyktafon)
| Klawisz | Akcja |
| :--- | :--- |
| **`[Enter]` / `[Spacja]` / `[Esc]` / `[G0]`** | Zakończ i zapisz nagranie na karcie SD |
| **`[P]`** | Wstrzymaj (Pause) / Wznów (Resume) nagrywanie |
| **`[+]` / `[=]`** | Zwiększ czułość mowy (obniż próg o 1 dB) |
| **`[-]` / `[_]`** | Zmniejsz czułość mowy (podnieś próg o 1 dB) |
| **`[C]`** | Wejdź do kreatora kalibracji szumu |

### Biblioteka i Odtwarzacz
| Klawisz | Akcja |
| :--- | :--- |
| **`[;]` / `[.]`** | Wybór pliku z listy |
| **`[Enter]` / `[Spacja]` / `[P]`** | Odtwórz / Pauza |
| **`[D]` / `[Backspace]`** | Usuń zaznaczony plik (wymaga potwierdzenia `[Y]`) |
| **`[+]` / `[-]`** | Regulacja głośności głośnika |
| **`[Esc]` / `[Opt]`** | Zatrzymanie odtwarzania i powrót do Menu |

---

## Wskaźnik Diodowy LED RGB

Cardputer posiada wielokolorową diodę informującą o stanie systemu:
* **Czerwony (Pulsowanie)**: Trwa nagrywanie mowy (VAS wykrył głos).
* **Błękitny / Niebieski (Ciągły)**: VAS oczekuje na głos (cisza – oszczędzanie miejsca).
* **Żółty / Bursztynowy**: Okres podtrzymania mowy (Hangover hold).
* **Zielony**: Odtwarzanie pliku przez głośnik.
* **Zgaszona**: Bezczynność / menu.

---

## Gotowe Pliki Binarne (Katalog `release/`)

Po kompilacji w katalogu `release/` generowane są dwa formaty:
1. **`VoiceRecCardputer-1.0.0.bin`**: Obraz samej aplikacji do skopiowania na kartę SD (`SD:/VoiceRecCardputer-1.0.0.bin`). Uruchamiany jednym kliknięciem przez **M5Launcher**.
2. **`VoiceRecCardputer-1.0.0-full.bin`**: Pełny obraz scalony (bootloader + tablica partycji + boot_app0 + aplikacja) do wgrania przez **esptool** lub **M5Burner** pod adres `0x0`.

### Wgrywanie przez `esptool`:
```bash
esptool.py --chip esp32s3 write_flash 0x0 release/VoiceRecCardputer-1.0.0-full.bin
```

---

## Kompilacja ze Źródeł

Wymagany jest zainstalowany [PlatformIO](https://platformio.org/):

```bash
cd C:\Projekty\ip9.pl\CardputerVoiceRec
pio run
```

---

## Licencja
Projekt objęty licencją MIT.
Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>.
