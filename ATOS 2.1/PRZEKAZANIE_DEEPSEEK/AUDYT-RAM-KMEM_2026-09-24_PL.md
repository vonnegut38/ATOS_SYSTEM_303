# ATOS 2.0 — audyt zuzycia ST-RAM po starcie (kmem)

Data: 24.09.2026. Autor: DeepSeek. Metoda: tymczasowy kmem_report()/kmem_audit[] w kernel_memory.c,
odczyt tablicy przez hatari-debug memdump (bez zrzutow ekranu, bez limitow scrolla).

## Wynik

Basen ST (po starcie, z shell gotowym, bez aplikacji):

| Pole | Wartosc |
|---|---|
| total | 4 194 304 B (4 MB) |
| free | 3 349 840 B |
| reserved | 708 064 B |
| largest | 3 346 704 B |

Used = total - free = 844 464 B (~824 KB) = reserved 708 064 + zaalokowane ~136 400.

## Co sklada sie na "reserved" (~692 KB)

- Baza kernela 0x00080000 = **512 KB** — caly obszar ponizej kernela jest poza basenem.
- Obraz kernela: text+data ~99 KB + BSS 67,6 KB = **~167 KB**.
- Gorny zapas 16 KB (kmem_init odejmuje 0x4000).

Razem ~695 KB. To znaczy, ze na maszynie 512 KB obecny kernel (baza 512 KB + 167 KB) fizycznie sie
nie miesci — jest to glowna przeszkoda dla malych maszyn, wieksza niz jakiekolwiek bufory.

## Zaalokowane (~136 KB)

| Owner | B | x | Co to |
|---|---|---|---|
| 1 (kernel) | 32 000 | 1 | desktop_framebuffer (grafika) |
| 257 (0x0101) | 69 744 | 3 | block cache ~64 KB + dma + obraz CORE |
| 263 (0x0107) | 16 464 | 1 | modul SHELL (obraz+BSS) |
| 259 (0x0103) | 3 328 | 3 | PROC |
| 264 (0x0108) | 3 008 | 1 | VOFA.SYS (zaladowany, bo VIDEO.CFG=VOFA) |
| 769/770/771 | 7 520 | 6 | stosy/struktury runtime |
| 261/262/260/258/256 | 3 408 | 5 | EVENT/INPUT/TIMER/VFS/ACSI |

Uwaga: owner 0x0101 jest wspoldzielony przez BLOCK (kernel_block.c) i CORE (modul) — warto je
rozdzielic w przyszlosci, zeby audyt byl jednoznaczny.

## Co juz zdjeto z BSS (leniwe)

- FAT undo 66 KB (przy pierwszym zapisie),
- WM console_surface + app_surface 96 KB (przy pierwszym uzyciu GUI),
- VOFA shadow 32 KB (przy aktywacji karty).

## Plan dalszej redukcji (priorytety)

1. **Obnizyc baze kernela** z 0x80000 na 0x20000-0x30000 — zysk 320-384 KB. Wymaga spójnej zmiany:
   include/boot_protocol.h, linker/kernel-sys.ld, system/kernel_entry.S (3 miejsca),
   loader/bootstage.c, tests/test_kernel_load_addr.py. Uwaga: BOOT.SYS jest ladowany pod 0x20000;
   albo kernel na 0x30000, albo przeniesc BOOT.SYS na 0x08000 i kernel na 0x20000. Konieczny test bootu.
2. **Block cache 128 -> 32 sektory** (kernel_block.c) — zysk ~48 KB.
3. **desktop_framebuffer leniwie** (kernel_graphics.c) — zysk 32 KB w profilu tekstowym.
4. **TOS=0** (profil build-time) — zysk ~13 KB.

Po 1-3 zuzycie spada z ~824 KB do ok. 400-450 KB, co pozwala myslec o maszynie 512 KB.

## Pliki

- kmem_report/kmem_audit: system/kernel_memory.c, include/kernel_memory.h (diagnostyka, nieuzywana
  w produkcyjnym starcie — gc-sections ja usuwa).
- Zrzut odczytu: atos2-tools/scenario_kmemread.py, log odczytu w atos2-tools (wynik powyzej).
