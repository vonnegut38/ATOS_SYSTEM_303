# ATOS 2.0 — lokalizacja utraty wejścia i punktowa poprawka SR

Data: 24.09.2026. Autor: DeepSeek.
Cel: znaleźć konkretny długi odcinek supervisor bez obsługi ACIA i przygotować punktową poprawkę
bez globalnego obniżania IPL i bez drugiego parsera IKBD.

## Werdykt

Przyczyną nie jest jedna długa funkcja, lecz **wyciek poziomu przerwań IPL6** z sekcji
zwrotnej wywłaszczenia Timer-C. Po pierwszym wywłaszczeniu procesu natywnego ścieżka
kernel_user_pause_recover wraca do supervisora instrukcją RTS (a nie RTE), więc SR pozostaje
ustawione na 0x2600 (IPL6) i przez resztę pracy pętli głównej **IRQ6 jest zamaskowane**.
Bajty IKBD, które przyjdą w tym czasie, nie są obsługiwane przez przerwanie; jeśli w jednej
sekcji pętli trafi się para make/break klawisza, odbiornik ACIA się przepełnia.

Poprawka to **jedna instrukcja** w system/kernel_entry.S: przywrócenie bazowego SR nadzorcy
(0x2500 = S=1, IPL5) przed RTS w kernel_user_pause_recover. Po poprawce: 20 cykli tego samego
stressu bez ani jednej straty (wszystkie liczniki INPUTSTAT = 0), wobec ACIA=1, QUAR=9,
PARTIAL=1 po 16 cyklach bez poprawki.

## Metoda diagnozy

Dodano tymczasową (diagnostyczną) instrumentację jądra, nie wchodzącą do poprawki:

1. kernel_ikbd_gap_begin()/kernel_ikbd_gap_end(id) — na granicach każdej sekcji pętli głównej:
   begin opróżnia odbiornik ACIA, end sprawdza bit overrun (0x20) i bit gotowego bajtu (0x01)
   oraz zlicza je per sekcja. Dzięki temu overrun jest przypisany do sekcji, w której powstał.
2. Ten sam probe w handlerze Timer-C (id 14).
3. Histogram IPL (kernel_current_sr) — ile wywołań gap_end widziało dany poziom przerwań.
4. Odczyt liczników przez hatari-debug memdump z ustalonych adresów (jądro linkowane pod 0x80000).

Adresy i obraz diagnostyczny: work/atos-v2-gap.img (jądro z instrumentacją).
Źródła diagnostyczne zachowane w work/gap-diagnostics/. Właściwa poprawka jest niezależna od tych plików.

## Dowody

### 1. Poziom IPL pętli głównej (histogram)

Przed poprawką, format [IPL0..IPL7]:

- tylko shell (bez wywłaszczeń):[0,0,0,0,0,2202,0,0] — pętla przy IPL5, IRQ6 dozwolone.
- po uruchomieniu SYSINFO + F5:   [0,0,0,0,0,6687,1384,0] — pojawia się IPL6 (1384 z 5869 nowych
  próbek, ok. 23,6%). To jest wyciek.

Po poprawce:

- tylko shell: [0,0,0,0,0,2278,0,0],
- po SYSINFO + F5: [0,0,0,0,0,11389,417,0] — IPL6 pochodzi wyłącznie z samego handlera Timer-C
  (probe id 14), a nie z sekcji pętli głównej.

### 2. Overruny per sekcja (przed poprawką, 16 cykli)

Suma overrun = 9, rozłożone na sekcje: id0 timer/switch 2, id1 input poll 1, id2 ksessions 1,
id3 consumer loop 1, id4 notify 1, id11 ksessions_paint 1, id12 kvideo_flush 2; Timer-C 0.
Rozkład po wszystkich sekcjach (także bardzo krótkich) potwierdza, że to nie jedna długa
funkcja, tylko regularne okna IPL6 wywołane wywłaszczeniem.

### 3. INPUTSTAT bez poprawki

Obraz bez poprawek, 16 cykli (ten sam scenariusz): końcowo IKBD DROPPED=1,
RAW=0 ACIA=1 KEY=0 MOUSE=0 QUAR=9 PARTIAL=1.

### 4. INPUTSTAT z poprawką

Obraz work/atos-v2-fix.img (tylko poprawka SR), 20 cykli: wszystkie odczyty końcowe
INPUTSTAT = RAW=0 ACIA=0 KEY=0 MOUSE=0 QUAR=0 PARTIAL=0, IKBD DROPPED=0, EVENTS PENDING chwilowo 0/1.
Pamięć ST-RAM FREE 2954 KB (SYSINFO i Commander nadal uruchomione).

Diagnostyczny wariant z poprawką, 16 cykli: 0 overrunów (wobec 9 przed).

## Poprawka

Plik: system/kernel_entry.S, etykieta kernel_user_pause_recover, bezpośrednio przed RTS:

    /* This path returns to the supervisor with RTS instead of RTE, so the
       Timer-C exception's SR (IPL6, IRQ6 masked) would leak into the main
       loop. Restore the kernel's baseline supervisor SR (S=1, IPL5): the
       same level used when a native process is entered below (0x0500 user). */
    move.w #0x2500,%sr

Patch: work/fix-input-sr.patch (diff względem oryginału z katalogu Codex, tylko te 5 linii).

### Dlaczego 0x2500

- Jądro w pętli głównej pracuje przy SR=0x2500 (S=1, IPL5) — to poziom bazowy przed wywłaszczeniem.
- Proces natywny jest wprowadzany z SR=0x0500 (kernel_entry.S, okolice linii 575), czyli IPL5 w trybie użytkownika.
- IRQ6 ma poziom 6, a 6 > 5, więc przy IPL5 przerwanie ACIA jest obsługiwane. Przy IPL6 jest maskowane.
- Pozostałe wywołania kernel_user_pause_recover pochodzą ze ścieżek TRAP (switch/sleep/wait), gdzie SR
  to już 0x2500, więc dla nich ta instrukcja jest no-op. Zmiana dotyczy wyłącznie ścieżki przerwania Timer-C.

## Walidacja

- python3 tools/build_atos2.py: PASS (zestawy C z ASan/UBSan oraz 20 pozycji PASS; log work/fix-build.log).
- Stress 20 cykli (shell + SYSINFO z F5 + Commander, Shift+Alt+F10, wpisywanie INPUTSTAT, przyciski myszy):
  0 strat we wszystkich licznikach.
- Porównanie 1:1 z przebiegiem bez poprawki (identyczny scenariusz) wypada na korzyść poprawki.
- Poprawka jest zainstalowana w obrazie punktu kontrolnego work/atos-v2.img (sha256 403b7a1e...)
  przez python3 tools/build_atos2.py --install; build/system/KERNEL.SYS ma teraz sha256 15c986f8....
  Obraz sprzed poprawki pozostaje w oryginalnym katalogu Codex oraz w archiwum wejściowym.

## Ryzyka i ograniczenia

- To nadal emulator Hatari (STE/030/16 MHz/mono), nie fizyczne Atari; brak ruchu myszy (tylko przyciski).
- Poprawka zmienia poziom przerwań w kontynuacji supervisora po wywłaszczeniu — celowo wraca do
  poziomu bazowego 0x2500, ale wymaga dłuższego stressu (dziesiątki cykli), testu fizyczną myszą
  i sprawdzenia innych profili CPU/sprzętu.
- Ścieżki recover z TRAP działają już przy IPL5; nie zmieniano kernel_user_recover (fault), bo błędy
  nie podnoszą IPL.
- Nie zmieniano globalnej polityki IPL w innych miejscach ani parsera INPUT.SYS.

## Pliki

- Patch: work/fix-input-sr.patch
- Obraz z poprawką: work/atos-v2.img (zaktualizowany; sha256 403b7a1e...)
- Sumy po poprawce: PRZEKAZANIE_DEEPSEEK/sha256-fix-input-sr.json
- Obraz diagnostyczny: work/atos-v2-gap.img (jądro z instrumentacją)
- Obraz work/atos-v2-fix.img usunięto jako tożsamy z work/atos-v2.img
- Źródła diagnostyczne: work/gap-diagnostics/
- Logi: work/fix-build.log, atos2-tools/fix-confirm-run.log, atos2-tools/gap2-run.log, atos2-tools/gap-fix-run.log
- Zrzuty: atos2-tools/shots/c-final-inputstat2.png (po poprawce), sA12-tts1-inputstat.png (przed)
