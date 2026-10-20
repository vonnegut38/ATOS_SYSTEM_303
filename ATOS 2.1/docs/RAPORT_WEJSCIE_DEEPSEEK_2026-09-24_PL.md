# ATOS 2.0 — kwalifikacja poprawki wejścia (DeepSeek)

Data: 24.09.2026. Autor: DeepSeek (sesja robocza).
Projekt: przeniesiony do /Users/xxx/Documents/Deepseek_test/ATOS 2.0 (oryginał w /Users/xxx/Documents/Codex/ATOS 2.0 pozostał nietknięty).

## Werdykt (skrót)

**Poprawka wejścia NIE przechodzi dłuższego obciążenia.** W dwóch niezależnych przebiegach stressu
(12 i 16 cykli: shell + SYSINFO z F5 + Commander, przełączanie sesji Shift+Alt+F10, wpisywanie
INPUTSTAT w tts1) liczniki strat znów zaczęły rosnąć:

- przebieg A: pierwsza strata ok. 8. cyklu (IKBD DROPPED=1, ACIA=1, QUAR=3), końcowo ACIA=2, QUAR=6, PARTIAL=1;
- przebieg B (16 cykli): końcowo ACIA=1, QUAR=9, PARTIAL=1, oraz jednorazowo powielony znak ("inputstatt").

Krótki test odtworzeniowy (jeden cykl) nadal daje zera — dlatego nie wolno uznawać krótkich prób za
potwierdzenie stabilności. To zgadza się z ostrzeżeniem z przekazania z 24.09.2026.
Nie zmieniano architektury, nie dodano drugiego parsera IKBD, nie obniżano globalnie IPL, nie
modyfikowano commander-v10 i nie zapisywano obrazu HDD przy działającym Hatari.

## Środowisko

- m68k-elf-gcc 16.2.0, GNU Make 4.4.1, Python 3.14.3, Hatari 2.6.1 (Homebrew).
- Profil: STE + 68030, 16 MHz, 4 MB ST-RAM, mono 640x400, własny ROM build/bios_mste.bin, HDD work/atos-v2.img.
- atos2.cfg: poprawiono bezwzględne ścieżki z .../Codex/ATOS 2.0 na .../Deepseek_test/ATOS 2.0
  (sLogFileName, szScreenShotDir, ACSI sDeviceFile0, ROM szTosImageFileName). Pozostałe ścieżki tymczasowe
  z oryginalnego uruchomienia (AppTranslocation) dotyczą nieużywanych urządzeń.

## 1. Przeniesienie i weryfikacja integralności

- rsync -a całości do workspace; liczba plików zgodna (1135 = 1135).
- Sumy SHA-256 po kopiowaniu zgodne z PRZEKAZANIE_DEEPSEEK/sha256.json dla bios_mste.bin,
  KERNEL.SYS, INPUT.SYS, ACSI.SYS, SHELL.SYS, work/atos-v2.img i archiwum checkpointu.

## 2. Czysty build i testy

- Wykonano gmake clean oraz python3 tools/build_atos2.py (bez --install).
- Wynik: EXIT=0, 254 wywołania kompilatora, 22 testy Python OK, wszystkie zestawy C
  (terminal, console, sessions, commander, sysinfo, languages) z ASan/UBSan: PASS.
- Powtórnie zbudowane artefakty są bit w bit zgodne z punktem kontrolnym:
  - bios_mste.bin 798d254bf37f5b44752579dc6779a548f55a48c47f1c8fdf9899e5563f6af100
  - KERNEL.SYS b6d69617248ce606b3e2fba1a46989c5a0625b9062b6ad1ef590913c46865825
  - INPUT.SYS 771e9ef39335f573e205dc7af29f165ff8b9b2988e3c3c71c54f81f632a2419b
  - ACSI.SYS 8adf896a19c296682ccf9525a538fab9e2db5019ea1fbd1ff039138661a4fb9b
  - SHELL.SYS 244d112d59576ffb6b0df456ca8bd5d96607cc3a99c87cb3373c9c0f84a42ec4
- Obraz HDD po wszystkich testach: 7424bb59909836dcda84a3d27c613862d9d712618df41b4e0647ffafbba1c4d7 — bez zmian.

## 3. Metoda automatyzacji Hatari (bez uprawnień Accessibility)

Host (macOS) nie ma przyznanych uprawnień Accessibility: AXIsProcessTrusted()=0, a System Events zwraca
-10004. Syntetyczna mysz/klawiatura przez CGEvent/osascript nie dociera więc do emulatora.
Zamiast tego użyto wbudowanego interfejsu sterowania Hatari 2.6.1 (--cmd-fifo / src/control.c):

- hatari-event keypress|keydown|keyup <znak|scancode>  — wstrzykiwanie klawiszy na poziomie IKBD,
- hatari-event doubleclick|rightdown|rightup             — zdarzenia przycisków myszy,
- hatari-debug screenshot <plik>                          — zrzut ekranu Atari,
- hatari-debug quit.

To nie wymaga żadnych uprawnień macOS i wstrzykuje wejście dokładnie tak, jak fizyczny IKBD.
Ważne: tryb --disable-video on NIE renderuje ekranu Atari do zrzutu (widać tylko pasek statusu),
dlatego testy ekranowe prowadzono w trybie okienkowym.

Narzędzia (w workspace):
- atos2-tools/atos2_auto.py      — biblioteka: komendy, scancody, wpisywanie, kombinacje, zrzuty,
- atos2-tools/scenario_repro.py  — krótka reprodukcja,
- atos2-tools/scenario_stress.py — stress A + B,
- atos2-tools/scenario_confirm.py— powtórka 16 cykli,
- atos2-tools/scenario_mem.py    — bilans pamięci,
- atos2-tools/shots/*.png        — zrzuty dowodowe,
- ATOS 2.0/work/hatari-*.log     — logi Hatari, work/*.log.out — wyjście debuggera.

Ograniczenie: interfejs nie obsługuje ruchu myszy (tylko przyciski), więc "ruchy myszy" i przeciąganie
nie były automatyzowalne; to nadal wymaga testu fizyczną myszą.

## 4. Wyniki

### 4.1 Krótka reprodukcja (scenario_repro.py)

Świeży start, tts2, run sysinfo.app, F5, Shift+Alt+F10 do tts1, po 2,5 s inputstat:
RAW=0 ACIA=0 KEY=0 MOUSE=0 QUAR=0 PARTIAL=0. Pamięć ST-RAM FREE: 3083 KB przed i po. PASS.

### 4.2 Stress A (scenario_stress.py, 12 cykli)

Konfiguracja: tts1 shell, tts2 SYSINFO (F5 co cykl), tts3 Commander (strzałki, Tab, doubleclick, prawy przycisk).
Cykl: przełączenie do tts1 -> inputstat -> przełączenie do tts2 -> F5 -> przełączenie do tts3 -> nawigacja.

- cykle 1-7: wszystkie liczniki 0,
- ok. cyklu 8: pierwsza strata: IKBD DROPPED=1, ACIA=1, QUAR=3,
- cykl 12: IKBD DROPPED=2, ACIA=2, QUAR=6, PARTIAL=1.

### 4.3 Powtórka (scenario_confirm.py, 16 cykli)

Ten sam scenariusz, dłużej. Wynik końcowy (dwa odczyty INPUTSTAT):
RAW=0 ACIA=1 KEY=0 MOUSE=0 QUAR=9 PARTIAL=1.

Dodatkowe obserwacje (zgodne z wcześniejszymi uwagami o automatyce):
- jednorazowo polecenie zostało powielone znakiem: "inputstatt" -> PROCESS START FAILED,
- jeden odczyt na starcie zakończył się PROCESS START FAILED mimo poprawnego wyglądu linii.
Te anomalie dotyczą tekstu polecenia; liczniki ACIA/QUAR/PARTIAL pochodzą z modułu INPUT i są niezależne
od poprawności samej komendy.

### 4.4 Bilans pamięci (scenario_mem.py)

- start: ST-RAM FREE 3083 KB, TT-RAM FREE 0 KB, inputstat wszystkie 0,
- tts2: run sysinfo.app + F5, F10 (wyjście); tts3: run command.app + nawigacja + F10 (wyjście),
- po zamknięciu: ST-RAM FREE 3083 KB, TT-RAM FREE 0 KB, inputstat wszystkie 0.
Brak wycieku i brak strat w tym krótkim cyklu otwórz/zamknij. Nie było testu wielogodzinnego.

## 5. Interpretacja

- ACIA (index 2) rośnie, gdy gość odczyta status ACIA z ustawionym bitem overrun — czyli bajt przyszedł,
  gdy przerwanie IRQ6 było zamaskowane zbyt długo (długa sekcja supervisor / brak odpytywania).
- QUAR (index 5) to bajty odrzucone w kwarantannie po wykryciu utraty synchronizacji ramki;
  PARTIAL (index 6) to wygasły niekompletny pakiet myszy.
- To dokładnie klasa błędu, którą miała zamknąć poprawka (kernel_ikbd_poll_capture, odczyt w ACSI,
  callback w rendererze, odczyt między konsumentami). Skoro liczniki nadal rosną po kilku-kilkunastu cyklach,
  któryś długi odcinek supervisor nadal nie oddaje sterowania na czas.

## 6. Ograniczenia metody

- To emulator Hatari, nie fizyczne Atari; jeden profil sprzętowy (STE/030/16 MHz/mono).
- Wejście wstrzykiwane przez cmd-fifo z odstępem ok. 0,09-0,12 s na znak (tempo człowieka); nie testowano
  skrajnych prędkości ani autorepeat.
- Brak ruchu myszy (tylko przyciski) — fizyczny test myszy nadal otwarty.
- Pojedyncze anomalie tekstu poleceń (duplikacja/odrzucenie) mogą częściowo pochodzić z automatyzacji;
  nie wpływają na odczyty liczników, ale utrudniają automatyczne potwierdzanie każdej komendy.
- Host miał wyłączone uprawnienia Accessibility, więc nie dało się porównać z wstrzykiwaniem hostowym.

## 7. Proponowane następne kroki

1. Zlokalizować konkretny długi odcinek supervisor bez obsługi ACIA. Kandydaci wg przekazania:
   koszt overlay/kursora, kopiowanie komórek, oczekiwanie na blitter; powrót Timer-C przez
   kernel_user_pause_recover bez przywrócenia wcześniejszego SR; praca pętli głównej przy IPL6.
2. Instrumentacja: znaczniki czasu wejścia/wyjścia sekcji supervisor, licznik zamaskowań IRQ6,
   pomiar odstępu między odczytami ACIA. Ewentualnie --trace IRQ / supervisor w Hatari.
3. Dopiero potem minimalna zmiana punktowa (np. dodatkowy punkt przechwycenia w zidentyfikowanym odcinku),
   bez globalnego obniżania IPL i bez drugiego parsera IKBD.
4. Osobno: test fizyczną myszą, dłuższy stress (dziesiątki cykli) i zamknięcie/odtworzenie slotów tts2/tts3.
5. Utrzymać regresje i próg: każdorazowo gmake clean + python3 tools/build_atos2.py oraz stress z odczytem INPUTSTAT.

## 8. Zgodność z wytycznymi

- Nie zmieniano commander-v10 ani architektury.
- Nie dodano drugiego parsera IKBD; nie zmieniono globalnej polityki IPL/SR.
- Nie uruchamiano starych celów image-v10*.
- Nie zapisywano obrazu HDD przy działającym Hatari (--install nie był używany; obraz bez zmian).
- Zmiany wprowadzono w kopii roboczej w workspace; oryginał w Codex pozostał nietknięty.
