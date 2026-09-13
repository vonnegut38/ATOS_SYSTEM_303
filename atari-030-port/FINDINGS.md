# Port ASF BIOS na 68030 / TF534 — ustalenia

Dokument roboczy. Zawiera **dowody** (nie hipotezy) i wnioski inżynierskie z sesji
diagnostycznej. Wszystkie testy prowadzone na konfiguracji Hatari użytkownika
(MegaST, 030 @32 MHz, FPU 68882, 4 MB ST-RAM, VGA, boot z obrazu ACSI);
CLI nadpisywał wyłącznie `--tos`, `--machine`, `--cpulevel`, `--cpuclock`, `--fpu`.

## 1. Port na 68030 nie jest problemem

| Konfiguracja | Wynik |
|---|---|
| MegaSTE + 68000 @8 MHz | ✅ boot do shella `C:\>` |
| MegaSTE + 68030 @32 MHz + FPU 68882 | ✅ boot do shella `C:\>` |
| MegaST + 68030 @32 MHz + FPU | ❌ `BLOCK LAYER: NO MEMORY` / `PRESS RESET` |
| MegaST + 68000 @8 MHz | ❌ `BLOCK LAYER: NO MEMORY` / `PRESS RESET` |

**Wniosek:** awaria nie zależy od CPU. Jądro ma już detekcję 68020/68030
(TC, CACR, `EXTB.L`), raportowanie MMU/cache i `kernel_image_cache_sync`.
68030 na MegaSTE przechodzi cały łańcuch: FAT16, CORE/VFS/PROC/TIMER/EVENT/INPUT/SHELL,
usługi startowe `SYSTEM.INIT`, `SYSTEM.GFX`, `SYSTEM.GUI`.

## 2. Przyczyna awarii na MegaST: odczyt ACSI DMA pisze poza bufor

Diagnostyka krok po kroku w `kernel_load_acsi()` (wartości: wolne/`largest` w bajtach):

```
ACSI pool after module init:  ~3 807 704 / ~3 807 000   (alokacja obrazu modułu)
ACSI probe allocated:         ~3 807 616 / ~3 807 200   (kmem_alloc 1024 B, align 512)
ACSI probe read done:         416 / 400                 ← PO ODCZYTANIU DMA
ACSI probe freed:             1504 / 1488               (sterta już zniszczona)
```

Krok próbny to `kmem_alloc(1024, 512)` + odczyt 2 sektorów ACSI + `kmem_free`.

Bufory-kanarki (`0xA5`, 512 B przed i 1536 B za buforem 1024 B):

| Maszyna | CPU | `guard hits before/after` |
|---|---|---|
| **MegaST** | 68000 @8 MHz | **0 / 256** |
| **MegaSTE** | 68030 @32 MHz | **0 / 0** |

**Kontrola izolująca:** na MegaSTE, gdzie kanarki są czyste, ten sam system bootuje
do shella i `DMA GUARD hits before/after: 0/0`. Na MegaST **256 bajtów zapisu poza
bufor** trafia dokładnie w 32-bajtowy nagłówek następnego bloku sterty
(`block_t`: magic/size/owner/flags/next/guard) i rozrywa listę wolnych bloków —
`stats()` widzi wtedy 416 B zamiast 3,8 MB, a `pool_alloc` nie znajduje bloku na
warstwę blokową. Stąd `BLOCK LAYER: NO MEMORY`.

**Dlaczego to nie widać na MegaSTE:** to nie „naprawa”, tylko układ pamięci.
Bufor DMA trafia tam w inne miejsce i nadpisanie ląduje w wolnym obszarze.
Bug jest realny na obu maszynach.

## 3. Wniosek inżynierski (ważniejszy niż samo obejście)

Każdy bufor będący celem DMA w tym jądrze jest używany **bez marginesu**:

- `kernel_load_acsi()` — `kmem_alloc(1024, 512)` jako bufor próbny,
- `kblock_init()` — `kmem_alloc(4096, 512)` jako `dma_buffer` warstwy blokowej,
- `kblock_read()` — DMA do `dma_buffer`,
- warstwa FAT — odczyty do buforów wywołującego (m.in. obrazy modułów).

Przy nadpisaniu 256 B każdy z tych przypadków może zniszczyć stertę lub obraz
modułu. **Dlatego poprawka musi być systemowa, nie punktowa.**

Kierunki naprawy (do decyzji):

1. **Margines ochronny** — alokować bufory DMA z zapasem ≥512 B
   (`bytes + DMA_GUARD`) i nigdy nie traktować bajtów zapasu jako danych.
2. **Rewizja programowania DMA w module ACSI** — `system/acsi_module.S`
   (`acsi_read_blocks`): sprawdzić semantykę rejestru liczby sektorów
   (`0xffff8604`) i kolejność zapisu rejestrów adresu (`0xffff8609/860b/860d`).
   W kodzie jest już martwy fragment: `move.l %d3,%d0; add.l %d4,%d0; bcs …`
   natychmiast nadpisany przez `move.l %a2,%d0` (linie 47–51).
3. **Kanarki w kodzie produkcyjnym** — tanie sprawdzenie guardów po każdym
   odczycie DMA; przy naruszeniu raportować zamiast po cichu psuć pamięć.

## 4. TT-RAM na MegaST — odpowiedź

Hatari **nie udostępnia TT-RAM maszynie MegaST**: opcja `--ttram` działa dla
modeli TT/Falcon, mimo że konfiguracja użytkownika ma `nTTRamSize = 20480`
(20 MB). Efekt: jądro raportuje `TT-RAM POOL: NOT PRESENT`, mimo że kod wykrywania
(`hw/memory.c:memory_detect_fast`) i pula TT (`kmem_init`, `0x01000000`) są gotowe —
dokładnie tam, gdzie TF534 ma swoje 4 MB.

Dwie drogi:

- **Pominąć** — ścieżkę TT-RAM testować na `--machine tt` (tam Hatari TT-RAM ma),
  a na MegaST weryfikować resztę portu.
- **Dodać TT-RAM dla MegaST do Hatari** — naturalne, bo i tak piszemy własne
  rozszerzenie do Hatari (Nova/ET4000). To ten sam rodzaj pracy: model akceleratora
  (TF534) z pamięcią szybką mapowaną pod `0x01000000` dla konfiguracji MegaST + 030.
  **Rekomendacja: połączyć oba w jedno rozszerzenie „TF534 + Nova/ET4000”.**

## 5. Co już jest w projekcie (fundament pod dalsze etapy)

- `hw/nova.c` — detekcja adapterów Nova/Vofa/Wittich i chipsetów
  (`NOVA_CHIP_ET4000_FAMILY`, `MACH32`, `MACH64`), wykrywanie **tylko odczytem**.
- `include/system.h` — `VIDEO_PREF_NOVA`, `VIDEO_ACTIVE_NOVA`, `nova_present`,
  `nova_adapter`, `nova_chipset`.
- `work/emutos-reference/` — pełne źródła **EmuTOS 1.4** z `bios/nova.h`
  (sterownik Nova/ET4000) i `bios/processor.S`.
- Detekcja Nova wymaga VME, a kod sprawdza `MACHINE_MEGA_STE || MACHINE_TT`.
  **MegaST też ma VME** — to do rozważenia przy porcie na Twoją maszynę.

## 6. Stan roboczy

- Diagnostyka w `system/kernel_main.c` i `system/kernel_module.c` oznaczona
  `TEMPORARY DIAGNOSTIC (MegaST port)` — do usunięcia po wdrożeniu właściwej poprawki.
- Bufor próbny ACSI przeniesiony na statyczny `acsi_probe_area[4096]` (BSS, 512-align) —
  to **obejście**, które odblokowuje boot na MegaST, nie docelowa naprawa.
- Obraz roboczy: `atari-030-port/disk/work.img` (kopia `atari_system_desktop_768.img`
  z wgranym `KERNEL.SYS`). Oryginały w projekcie nietknięte.
- Narzędzia: `hatari_shot.py` (zrzut przez FIFO debuggera), `avi_frames.py`
  (ekstrakcja klatek z AVI Hatari — Hatari zapisuje `movi` z zerowymi rozmiarami,
  więc klatki wycinane są po sygnaturze PNG).

## 7. Następne kroki

1. Wdrożyć systemową ochronę buforów DMA (margines + weryfikacja) i usunąć diagnostykę.
2. Sprawdzić, czy nadpisanie 256 B występuje dla innych rozmiarów/liczb sektorów
   (1, 4, 8) — od tego zależy dobór marginesu i ewentualna poprawka w module ACSI.
3. Uruchomić pełny test na MegaST + 68030 @32 MHz (docelowy profil TF534).
4. Rozszerzenie Hatari: TF534 (TT-RAM pod `0x01000000`) + Nova/ET4000.
5. Sterownik IDE dla TF534.
