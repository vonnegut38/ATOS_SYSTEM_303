# Paczka audio (etap 5) — do promocji po PASS

Zbudowana produkcyjnie: **bez** hooków testowych (`build/.profile` bez
`ARK_TEST_*`, w `kernel.elf` **0** symboli `ar_done`/`ar_start`).

## Co jest w środku

| plik | rozmiar | rola |
|---|---|---|
| `KERNEL.SYS` | 457 762 | jądro z poprawkami audio (PSG, Dosound, właściciel, generacje, sprzątanie) |
| `BIOS.bin` | 262 144 | BIOS przebudowany — zawiera własną kopię `hw/psg.c`, więc **musi** iść razem z jądrem |
| `DESKTOP.APP` | 29 914 | pulpit: dialog RUN jako nakładka jądra + `RUN FAILED rc=-N` |
| `T.PRG` | 1442 | sonda audio (tryb normalny) — artefakt **testowy** |
| `TOSAUDIO.PRG` | 1442 | to samo pod oryginalną nazwą — artefakt **testowy** |
| `ARKANOID.APP` | 18 448 | aktualny build gry używany w testach — artefakt **testowy** |

SHA256 wszystkich plików: `SHA256.txt`.

## Co się zmieni w zestawie live

* `ATOS.img`: `SYSTEM/KERNEL.SYS`, `SYSTEM/APP/DESKTOP.APP` (oraz opcjonalnie
  `SYSTEM/APP/T.PRG` — tylko jeśli chcecie mieć sondę na obrazie),
* `BIOS.bin`: **poza** obrazem — emulator ładuje go przez `--tos`.

## Jak wdrożyć

```sh
cp ATOS_AKTUALNY/ATOS.img /tmp/ATOS-test.img      # kopia, nie oryginał
work/audio-baseline/PACZKA-PROD/install-to-copy.sh /tmp/ATOS-test.img
# BIOS: podmienić BIOS.bin w kopii katalogu, nie w ATOS_AKTUALNY
```

Skrypt **odmawia** pracy, gdy cel leży w `ATOS_AKTUALNY` lub `ATOS_KOPIA`.

## Jak zweryfikować

```sh
python3 tools/audio_manifest.py --image /tmp/ATOS-test.img --out manifest-check.md
```

Manifest czyta pliki **z FAT** i porównuje SHA z buildem; wszystkie pozycje mają
być `ZGODNE`. To narzędzie już raz wyłapało rozjazd (jądro w obrazie vs manifest),
więc nie pomijajcie tego kroku.

## Czego jeszcze nie dowiedziono

* Zmierzone na emulatorze (runda 24): natywne **zaplanowane** zdarzenie STOP
  wykonało się w terminie **z sesją TOS w środku** — głośność kanału A `12 → 0`,
  `owner` bez zmian, żądania TOS odrzucone (`rej=2`, `ref=4`), sesja posprzątana.
  Narzędzie: `ATEST.APP` (aplikacja testowa) + sonda czytająca stan przez
  `Giaccess`. Kontrakt pokrywa też deterministyczny test hostowy 19
  (`volume 12 → 0`, właściciel bez zmian) i Wasza sonda `probe_generation.c`.
* porównania płynności **poza tą sceną** — w tej samej scenie (Arkanoid, serw,
  4 próbki po 10 s) klatki przy audio **włączonym** (`owner=5`, `isr_max=2`) i
  **wyłączonym** w gościu (`KERNEL-OFF`, `owner=0`, `isr_max=0`) są **identyczne
  bajtowo** (te same SHA), czyli w tej scenie koszt podsystemu nie jest widoczny;
  nie jest to jednak pomiar percentyli płynności w wielu scenach.
