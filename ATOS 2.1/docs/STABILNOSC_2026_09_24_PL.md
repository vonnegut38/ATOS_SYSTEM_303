# ATOS 2.0 — punkt kontrolny stabilności, 24.09.2026

Projekt: /Users/xxx/Documents/Codex/ATOS 2.0. Oryginalny commander-v10 nie został zmieniony.
Ten dokument uzupełnia JEZYKI_PL.md i POPRAWKI_WEJSCIA_I_SESJI_PL.md.

## Naprawione w tym pakiecie

1. RUN korzysta z istniejącego loadera STARTFG i własności zasobów procesu w kernelu. Dawna ścieżka interpretowała powrót po kwancie schedulera jako koniec programu, drukowała PROGRAM EXIT i prompt na ekranie aplikacji. Teraz zakończenie jest raportowane przez kolejkę rzeczywistych wyników procesów.
2. Aplikacja na pierwszym planie może przejąć GUI. Klawisze GUI mają pierwszeństwo przed terminal_getchar. Prawo przejęcia przysługuje właścicielowi terminala lub jego potomkowi w tej samej sesji; obcy proces jest odrzucany.
3. Usunięto drugi prompt po zakończeniu programu wypisującego tekst.
4. Dekoder ramek 68030: SR +0, PC +2, format/vector offset +6, numer wektora = offset/4. Format 0 nie jest wykrywany heurystyką z SR. A/B: adres błędu danych +0x10 tylko przy SSW.DF. Dodano wersję z ograniczeniem długości. Nie interpretuje się formatu 7 jako ramki 030.
5. Stuby wyjątków kopiują tylko wspólny sześciobajtowy prefiks, bez odczytu 24 bajtów z krótkiej ramki. SR jest prawidłowo rozszerzany do 32 bitów. Korekta PC w diagnostyce TOS. Snapshot kernel_fault_frame nie zawiera całej ramki bus error — dalsze bajty są zerami.
6. Natywna obsługa błędu sprawdza bit supervisor w zapisanym SR; błąd jądra nie jest przypisywany aktywnej aplikacji. Illegal korzysta z tej samej ścieżki zakończenia i powrotu na zapisany stos kernela co pozostałe błędy. Nie wykonujemy RTE uszkodzonego procesu z ramką A/B.
7. EXWAIT obejmuje także illegal (11 potomków). LINEF używa nieobecnego koprocesora ID 7. Stare F000 wywoływało na 68030 privilege violation (potwierdzone wynikiem FAULT 264), więc oczekiwanie Line-F było błędne.

Źródło układu ramek: Motorola MC68030 User’s Manual, część 2, rozdział 8:
https://www.nxp.com/docs/en/reference-manual/MC68030UM-P2.pdf

## Weryfikacja

- python3 tools/build_atos2.py --install: PASS; pełny log stability-final-install.log.
- gmake check: zestawy C oraz 21 testów Python PASS. Dodatkowo terminal, console, sessions, commander, sysinfo, languages PASS.
- Dekoder wyjątków sprawdzony dodatkowo przez ASan/UBSan.
- Testy obejmują rzeczywistą funkcję routingu wejścia i kontrolę właścicieli GUI, w tym potomków, obcy terminal oraz cykl rodziców.
- Hatari 2.6.1, STE, 68030, 16 MHz, 4 MiB ST-RAM, mono 640x400, własny build/bios_mste.bin, atos2.cfg.
- RUN COMMAND.APP: pełny pierwszy ekran bez dodatkowego klawisza i bez fałszywego EXIT. Dialog F7/anulowanie, Tab, zaznaczenie, ruch myszy bez samoczynnego wyjścia. F10: jedno rzeczywiste EXIT 0.
- Dwa Commandery i SYSINFO równocześnie: niezależne aktywne panele i zaznaczenia, dziewięć przełączeń i dodatkowe pojedyncze przejścia. SYSINFO skończyło CPU/RAM/odczyt dysku w tle. To próba funkcjonalna, nie wielogodzinny stress test.
- Zamknięcie Commandera w tts2, następnie EXIT zamykające pusty terminal i ponowne TTS2: czysty shell, zachowany ekran tts1. SYSINFO: wolna pamięć wzrosła z 2968 do 3004 KiB po zamknięciu jednej instancji.
- Końcowy build: EXWAIT: 11 CHILD FAULTS / WAKE / REAP / MEMORY PASS, EXIT 0, jeden prompt. Test sprawdza wyniki wyjątków, obudzenie rodzica, usunięcie potomków i bilans pamięci.
- Końcowy INPUTSTAT po EXWAIT: IKBD DROPPED 0, EVENTS PENDING 0. W wcześniejszym dłuższym przebiegu licznik wyniósł 1; nie ogłaszamy usunięcia wszystkich strat wejścia.
- Audyt zamkniętego obrazu: zgodne kopie FAT, clean flag, brak crosslinków; szczegóły w kontrola_dysku.json. Pozostały dawny osierocony klaster 437. Samo porównanie obrazu z nim samym nie jest dowodem zachowania plików historycznego obrazu; sprawdzono strukturę i zgodność instalowanych binariów z buildem.

## Odtworzenie i przekazanie

Archiwum punktu kontrolnego zawiera źródła, testy, dokumentację, build z BIOS-em i aplikacjami oraz zamknięty obraz HDD. Sumy są w sha256.json. Konfiguracja zawiera ścieżki bezwzględne do obecnego katalogu; po przeniesieniu trzeba poprawić ścieżkę ROM/HDD.

Standardowy build/test/instalacja, przy zamkniętym Hatari:

    cd '/Users/xxx/Documents/Codex/ATOS 2.0'
    python3 tools/build_atos2.py --install

EXWAIT i jego próbki zainstalowano dodatkowo w SYSTEM (standardowy instalator nie aktualizuje tych aplikacji testowych). Aby odtworzyć ich aktualizację:

    for n in EXWAIT FAULT BUSFLT ADRFLT DIVFLT CHKFLT TRAPVFLT PRIVFLT LINEA LINEF TRAP1 TRAP15; do
      python3 tools/install_boot_sys.py work/atos-v2.img "build/system/$n.APP"
    done

W Hatari: File → Load Config → atos2.cfg → Reset. Sprawdzić pasek: 16MHz / 030 / STE / MONO, nie domyślny profil 32MHz MegaST RGB. W shellu RUN EXWAIT.APP. Sesje tworzy się poleceniami TTS2/TTS3; Shift+Alt+F10 przełącza istniejące sesje. EXIT zamyka pustą sesję poza tts1. F10 w Commanderze/SYSINFO kończy aplikację.

## Pozostały zakres — kolejność dla DeepSeek

1. Wejście: rozdzielić liczniki ACIA overrun, raw ring overflow i event/mailbox overflow; odtworzyć zanik znaków z fizycznej myszy/klawiatury i automatów. Szybkie serie skrótów nie stanowią jeszcze dowodu bezstratnego przełączania. Nie dodawać kolejnego surowego czytnika IKBD poza INPUT.SYS.
2. Zakończyć akceptację B3: odczyt SR/PC rzeczywistych illegal/bus error z debuggera i porównanie z adresem instrukcji, osobny test supervisor fault, kwalifikacja 68000 i legacy TOS. Obecny EXWAIT potwierdza containment, lecz nie zastępuje pomiaru SR/PC ani kontroli ochrony pamięci.
3. B4 z docs/audit/ZAKRES_PRAC.md: wydzielić zależność natywnego startu/timera od persony TOS. Mały osobny pakiet; nie przepisywać schedulera i GUI jednocześnie.
4. Następnie dłuższy stress: powtarzalne cykle start/kill/close/create, pomiary pamięci i wejścia oraz awarie aplikacji przy dwóch aktywnych GUI.

Polityka SR 0x0500, PMMU/ochrona pamięci, VM, FAT overwrite/rollback, kontrakty VOFA i nowy desktop nie są zamknięte tym pakietem. Bez MMU aplikacja nadal może uszkodzić cudzą pamięć. Nie deklarować pełnej stabilności ani zamknięcia całego B3/B4.
