# Raport: górne menu Commandera + sortowanie per panel

Data: 2026-09-24
Zakres: natywny Commander ATOS 2.0 (COMMAND.APP), system/kernel_fat.c, katalog języków.

## 1. Cel

Dodać w Commanderze górne menu w stylu Midnight Commandera, umieszczone
odpowiednio nad lewym i prawym oknem, z sortowaniem każdego panelu osobno:
nazwa, data, typ.

## 2. Co zostało zrobione

### 2.1 Pasek menu nad panelami (wiersz 0)

- `menu_anchor(p)` rysuje nad panelem `p` etykietę `[Lewy:Nazwa]` /
  `[Prawy:Nazwa]` (w zależności od aktywnego sortowania).
- Lewy anchor: kolumna 1, prawy anchor: kolumna 41 — dokładnie nad ramkami
  obu okien (ramki zajmują wiersze 1..21).
- Aktywny panel ma anchor podświetlony (TERM_INVERSE).

### 2.2 Rozwijane menu sortowania

- F9 (lub klik w anchor) otwiera popup `Sortuj:` z pozycjami:
  Nazwa, Data, Typ.
- Bieżący tryb oznaczony gwiazdką.
- Obsługa: Strzałka w górę/dół, Enter, Esc, oraz skróty `n`/`d`/`t`.
- Klik w pozycję popupu działa myszą (wiersze 48..96 px).
- `apply_sort()` ustawia `panels[p].sort` i przeładowuje tylko ten panel.

### 2.3 Sortowanie całego katalogu (nie jednej strony)

- Kernel zwraca katalog w kolejności FAT, więc aplikacja pobiera cały
  katalog (do 256 wpisów) przez `gui_call(52, ...)`, a dopiero potem sortuje
  i buduje okno 16 wierszy.
- `entry_cmp()`: katalogi najpierw, potem wg trybu:
  - nazwa — `name_cmp` (bez rozróżniania wielkości liter),
  - data — najnowsze najpierw,
  - typ — najpierw rozszerzenie, potem nazwa.
- Wpisy to 24-bajtowa struktura: `name[13] + attr + pad[2] + size + date`.

### 2.4 Data w listingu (kernel)

- `kfat_list_callback_t` otrzymuje teraz `uint32_t datetime`.
- `kernel_fat.c` (list_sector) składa datę/czas jak w BIOS FIB:
  `(le16(e+24)<<16)|le16(e+22)`.
- `user_dir_entry_t` (kernel_exception.c) rozszerzony o `datetime`.
- `tests/test_directory_page.py` zaktualizowany.

### 2.5 Teksty

Dodane klucze w `languages/catalog.json` (regeneracja: `tools/make_languages.py`):

| klucz | EN | PL |
|---|---|---|
| CMD_MENU_LEFT | Left | Lewy |
| CMD_MENU_RIGHT | Right | Prawy |
| CMD_SORT | Sort: | Sortuj: |
| CMD_SORT_NAME | Name | Nazwa |
| CMD_SORT_DATE | Date | Data |
| CMD_SORT_TYPE | Type | Typ |

## 3. Weryfikacja (Hatari, STE/68030/16 MHz, mono, 4 MB)

Scenariusz: `atos2-tools/scenario_cmdmenu.py`.
Zrzuty: `atos2-tools/shots/cmdmenu/`.

1. `run command.app` -> `PROCESS READY, PID: 4`, Commander rysuje się w
   kilka sekund (windowed Hatari).
2. Stan bazowy (`c0-base.png`): pasek `[Left:Name]` ... `[Right:Name]`,
   oba panele posortowane po nazwie.
3. F9 (`c1-f9.png`): popup `Sort:\n *Name\n  Date\n  Type`.
4. Strzałka w dół + Enter (`c2-leftdate.png`): lewy anchor `[Left:Date]`,
   zawartość lewego panelu zmienia kolejność; prawy pozostaje `[Right:Name]`.
5. Tab + F9 + 2x dół + Enter (`c3-righttype.png`): prawy anchor
   `[Right:Type]`, lewy nadal `[Left:Date]` — niezależność paneli
   potwierdzona.

## 4. Uwaga o komunikacie "PROCESS START FAILED"

Wcześniejszy scenariusz diagnostyczny uruchamiał kolejno `run hello.app`,
`run sysinfo.app`, `run command.app`. SYSINFO to aplikacja pełnoekranowa i
pozostaje właścicielem terminala (foreground), więc start Commandera kończył
się komunikatem `PROCESS START FAILED`. To poprawne zachowanie (jeden proces
pierwszoplanowy na sesję), a nie błąd Commandera. Po zamknięciu SYSINFO
Commander startuje normalnie.

## 5. Wynik

Feature działa i jest zweryfikowany wizualnie. Build i testy pozostają
zielone.

## 6. Optymalizacja czasu startu aplikacji

Objaw: Commander pojawiał się ~6,7 s po `PROCESS READY` (zrzuty
`atos2-tools/shots/latency/`: f015 ~7,7 s). Ślad syscalli w kernelu pokazał, że
cały ten czas siedzi w `lng_init` -> `lng_parse`, a dokładnie w skanowaniu
kluczy: dla każdej z 155 linii pliku językowego pętla wołała `lng_key(i)` dla
wszystkich 46 kluczy. Przy `-fno-jump-tables` (wymóg PIC) `lng_key` to ciąg
porównań, więc ~7 tys. wywołań paliło ~84 mln cykli 68000.

Naprawa (dwie warstwy):
1. `tools/make_languages.py` wylicza teraz **gotowe offsety każdej wartości**
   w generowanej paczce (`lng_off_en`, `lng_off_pl`, `lng_len_en/pl`).
2. `include/app_lng.h`: `lng_load` adoptuje paczkę przez kopiowanie offsetów i
   jednorazowe odkodowanie escaperów w miejscu (bez przesuwania kolejnych
   wartości). Pełny parser zostaje jako fallback dla ręcznie edytowanych
   paczek (zabezpieczenie: długość pliku + nagłówek `ATOS-LNG-1`).

Efekt (pomiar śladem): faza języka **269 -> 2 ticki** (5,4 s -> 0,04 s), cały
start Commandera ~20 ticków (~0,4 s). Wizualnie Commander pojawia się na f005
(~2,5 s od Enter, było f015 ~7,7 s).

Przy okazji: aby zmieścić nowe funkcje w zasięgu `R_68K_PC16` (±32 KB) obrazu
`-mpcrel`, bufor podglądu i `sortbuf` trafiły na stertę aplikacji (syscall 16),
a `LNG_CAPACITY` zmniejszono z 12000 do 9216 (paczki mają ~6 KB).

## 7. Menu File i klawisze F

- Pasek: `[Left:...]` `[File]` `COMMANDER` `[Right:...]`.
- `File` (F9, potem strzałka w prawo albo klik) — pozycje: View, Rename,
  Copy, New folder, Delete, Quit; wybór Enter uruchamia odpowiednią akcję.
- **F3 Podgląd** — przewijany podgląd pliku (8192 B na stercie), zamykany
  F3/Esc.
- **F6 Zmiana nazwy** — modal z nazwą pliku; Enter wykonuje `gui_call(54)`
  (rename/move). W katalogu TRASH F6 nadal przywraca plik.
- Istniejące: F5 kopiuj, F7 nowy katalog, F8 usuń/do TRASH, F10 wyjście,
  F1 pomoc, Enter wejdź/uruchom.

Weryfikacja: `atos2-tools/shots/menu3/` (menu File) i
`atos2-tools/shots/menu4/` (podgląd `BIOS.CFG`, modal zmiany nazwy).

Uwaga: `hatari-event keypress 1` jest przez Hatari interpretowane jak znak
"1", więc Esc w scenariuszach testowych nie dociera jako scancode 1; na
prawdziwym sprzęcie Esc działa (kernel dostarcza scancode 0x01).

## 8. Edytor (F4) i syscall zapisu

Aplikacje natywne nie miały syscallu zapisu pliku (`kfat_write_path_at` tylko
nadpisuje w istniejącym rozmiarze). Dodano:
- kernel: `kfat_save_path(path,data,size)` — tworzy lub podmienia plik, zwalnia
  starą listę klastrów, alokuje nowe i wpisuje rozmiar; transakcja z rollbackiem.
- natywny **syscall 60**: `gui_call(60, size, 0, path, data)`.
- Commander: **F4 Edycja** — pełnoekranowy edytor (bufor 16 KB na stercie):
  strzałki, Enter, Backspace, F2 zapis, F10 wyjście; pliki >16 KB są odrzucane
  (bez ryzyka obcięcia).
- pozycja `Edit` w menu File.

Weryfikacja end-to-end: do obrazu wgrany `\NOTES.TXT` (12 B,
`HELLO\nWORLD\n`), w Hatari F4 -> wstawiono `X` -> F2. Odczyt z obrazu:
`size=13`, treść `XHELLO\nWORLD\n`. Zrzuty: `atos2-tools/shots/editor/`.
