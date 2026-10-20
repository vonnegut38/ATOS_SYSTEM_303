# Dwa obrazy do porównania (kopie obrazu live z 12 paź 00:04)

## A. `work/atos-live-sync.img` — minimalna, bezpieczna poprawka

Wasz obraz live **bez zmian w jądrze i BIOS-ie Codexa**; podmienione tylko:

* `SYSTEM/APP/ARKANOID.APP` 18 448 → **18 534** (gra z poprawką dźwięku: każdy
  efekt to teraz krótki blip + STOP, więc nie piszczy),
* dołożone sondy `T.PRG`, `TOSAUDIO.PRG` i `ATEST.APP` (artefakty testowe).

Użyjcie BIOS-u z `ATOS_AKTUALNY/BIOS.bin` (jądro Codexa zostaje).

## B. `work/atos-tree-full.img` — wszystko z obecnego drzewa

Ta sama kopia, ale nadpisane: `KERNEL.SYS` **458 844** (mój build obecnego drzewa),
`DESKTOP.APP` 29 914, `ARKANOID.APP` 18 534, sondy + `ATEST.APP`.
**Ten obraz wymaga BIOS-u z drzewa**: `work/audio-baseline/BIOS-tree.bin`
(BIOS zawiera własną kopię `hw/psg.c`, więc musi pasować do jądra).

Manifest: `manifest-TREE-FULL.md` — **5/5 ZGODNE** z FAT.

## Dlaczego dwa

Jądro w obrazie live ma **459 714 B**, a obecne drzewo buduje **458 844 B**
(różnica 870 B) i inny BIOS (`55e630fd…` vs `e128088d…`). Czyli obraz live
powstał ze **stanu, którego w drzewie nie ma** — nie chciałem go nadpisywać.
A jest w nim **stara gra** (18 448), czyli **bez poprawki dźwięku**.

## Co warto kliknąć (30 sekund)

1. Uruchom obraz A (BIOS z ATOS_AKTUALNY) → ikona **ARKANOID** → **Space**.
   Dźwięki mają być **krótkimi blipami** (odbicie, cegła, bonus inny ✓), a nie
   ciągłym piskiem.
2. Potem obraz B (BIOS `BIOS-tree.bin`) → to samo + `ATEST` z RUN i `T.PRG` z RUN,
   jeśli chcecie zobaczyć test „natywny STOP + sesja TOS” (u mnie: głośność
   kanału A 12 → 0, właściciel bez zmian, TOS odrzucony).

## Czego NIE zweryfikowałem

Audio na **jądrze Codexa** (obraz A): to inny build, więc adresy moich liczników
się nie zgadzają, a start aplikacji z mojego harnessu (klawiatura) jest zawodny.
Na obrazie B (jądro z drzewa) widzę, że **przerwanie audio pracuje**
(`isr_max=2`), ale gry nie udało mi się wystartować automatycznie.
