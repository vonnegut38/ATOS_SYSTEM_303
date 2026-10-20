# UI / boot — zadania (2026-09-26)

## 1. "LOADING COMPONENTS" ma pokazywać listę modułów
Stan obecny: system/kernel_main.c:299-301 **celowo** wypisuje JEDEN wiersz,
w którym zmienia się tylko wartość ("LOADING COMPONENTS: XXX"), i nie przewija.
Dlatego "nic nie widać" — to nie brak funkcji, tylko zamierzony pojedynczy
wiersz, który przelatuje zbyt szybko przy 640x480.

Do zrobienia: wypisać moduły jako czytelny ślad (osobne wiersze albo
dopisywanie na końcu), np.:
    LOADING COMPONENTS:
      CORE.SYS   OK
      VFS.SYS    OK
      ...
Miejsce: system/kernel_main.c:285-310, tekst w kconsole_puts.
Ograniczenie: nie może przewijać — jeśli lista przekracza ekran, ustalić limit
i ewentualnie skrócić nagłówek.

## 2. Boot zostaje w 640x480 (może być mono)
Bez zmian. Boot nie przełącza się na 800x600.

## 3. Desktop docelowo 800x600x256 (8 bit)
Stan obecny: tryby JUŻ SĄ — system/desktop_wm.c:531-535:
    0: 640x400
    1: 800x600
    2: 1024x768
    3: 800x600x256     (depth = 0x8000 -> marker trybu kolorowego)
  oraz system/desktop_wm.c:328: "800x600 istnieje w obu głębiach, więc tryb
  kolorowy ma własny opcode".

Do zrobienia:
  a) ustawić tryb 3 (800x600x256) jako DOMYŚLNY dla desktopu,
  b) sprawdzić, że backend ET4000/VOFA faktycznie przełącza na 8-bit
     (nie tylko deklaruje),
  c) potwierdzić, że boot pozostaje w 640x480/mono aż do startu desktopu,
  d) zweryfikować w Hatari: --graphics-card vofa --gfx-chip et4000.

## Kolejność
1. Lista modułów w "LOADING COMPONENTS" (widoczność, małe ryzyko).
2. Domyślny tryb desktopu = 3 (800x600x256) + weryfikacja 8-bit na VOFA.
3. Dopiero potem ewentualne skracanie logu bootowania.
