# EMUTOS-INTEGRATION — analiza (ATOS v10, Phase 18)

Źródła: `files-pasted-by-the-user-projekt/work/emutos-reference/` (checkout EmuTOS,
HEAD `e0f0c23…`). Analiza wyłącznie z plików w repo; nic tu nie jest kopiowane.
Decyzja projektu: **OPTION A (własna architektura) + akceptacja GPL-2.0**, więc
wolno używać komponentów/zasobów EmuTOS z podaniem pochodzenia.

## 1. Które moduły EmuTOS za co odpowiadają (fakty z drzewa)

| Warstwa | Pliki EmuTOS | Uwagi |
|---|---|---|
| GEMDOS | `bdos/` — `bdosmain.c`, `osmem.c`, `umem.c`, `iumem.c`, `proc.c`, `kpgmld.c`, `fs*.c`, `console.c`, `time.c`, `rwa.S` | pliki/uchwyty/pamięć/procesy/loader .PRG |
| BIOS | `bios/bios.c`, `bios/screen.c`, `bios/clock.c`, `bios/ikbd.c`, `bios/acsi.c`, `bios/ide.c`, `bios/floppy.c`, `bios/biosmem.c`, `bios/vectors.S` | sprzęt + przerwania + VBL |
| XBIOS | `bios/xbios.c`, `bios/xbios.h` | Physbase/Logbase/Getrez/Setscreen/Vsync/Supexec… |
| VDI | `vdi/` — `vdi_main.c`, `vdi_control.c`, `vdi_line.c`, `vdi_fill.c`, `vdi_text.c`, `vdi_mouse.c`, `vdi_blit.S`, `vdi_raster.c`, `vdi_defs.h` | workstation `Vwk`, tabela `vdi_v_*` (`vdi_defs.h:338+`) |
| AES | `aes/` — `gemstart.S`, `geminit.c`, `gemgraf.c`, `gemgsxif.c` (most do VDI), `gemwmlib.c`, `gemevlib.c`, `gemoblib.c`, `gemrslib.c`, `gem*.c` | okna/zdarzenia/obiekty/zasoby; wejście przez `gemstart.S` |
| Zasoby GEM | `aes/gem.rsc`, `aes/gem.def`, `desk/` | RSC/ikony/desktop |

## 2. Jak EmuTOS komunikuje się ze sterownikiem ekranu

- VDI operuje na **workstation** (`Vwk`) i rysuje bezpośrednio w formacie ekranu;
  `vdi_defs.h` definiuje `VDI_HANDLE`, clip (`VDI_CLIP`), tabelę funkcji
  `vdi_v_opnvwk` … `vdi_vrt_cpyfm` (`vdi_defs.h:338-412`).
- Baza ekranu i parametry Line-A żyją w zmiennych systemowych; tryb/paleta/kursor
  przez `bios/screen.c` + XBIOS.
- **Nie ma czystego „video_driver_t"** — VDI/BIOS są splecione z planarami ST
  (i z VIDEL/TT w `videl.c`). To jest miejsce, które w ATOS zastępujemy abstrakcją.

## 3. Gdzie wpiąć `video_driver_t`

1. **Pod VDI**: podmienić backendy rysujące (`vdi_line/fill/text/blit/raster`)
   na wywołania `video_driver_t` (fill_rect/draw_line/blit) — AES i aplikacje
   nie widzą różnicy.
2. **Pod `bios/screen.c`/XBIOS**: set_mode, paleta, kursor, Vsync → `video_driver_t`.
3. **AES bez zmian** — korzysta z VDI (`gemgsxif.c`), więc nie dotyka sprzętu.

## 4. Czy można użyć AES/VDI EmuTOS bez zastępowania ATOS BIOS?

Tak — na dwa sposoby:
- **(B) kompilacja źródeł do naszego jądra**: GEMDOS/BIOS/XBIOS/VDI/AES EmuTOS
  linkowane nad ATOS, z naszym `video_driver_t` pod VDI. Wymaga dostarczenia
  zmiennych systemowych/wektorów/cookie jar i uruchamiania w 68000-owym modelu.
- **(C) osobny „ROM"/agregat EmuTOS**: ATOS BIOS bootuje i uruchamia obraz EmuTOS
  (jak przy F5/MMU). GPL „mere aggregation" nie zaraża naszego kodu; dystrybucja
  wymaga źródeł/oficjalnej oferty EmuTOS.
  W obu wypadkach **ATOS BIOS/kernel zostaje**.

## 5. Zależności, które trzeba spełnić (fakty)

- **Zmienne systemowe 0x380–0x800** (`tosvars.ld`): `_proc_lives=0x380` (crash page),
  `_memvalid=0x420`, `_phystop=0x42E`, `_membot=0x432`, `_memtop=0x436`,
  GEM wektory `_etv_timer=0x400`…, dalej `_vblqueue`, `_vbclock`, `_frclock`,
  `_p_cookies=0x5A0`. ATOS dziś ustawia część z nich (`_hz_200`, `_membot/_memtop`,
  `_v_bas_ad`, `_sysbase`) — brakuje m.in. `_phystop`, kolejki VBL, `_p_cookies`.
- **Wektory**: tablica pod adresem 0 (68000) / VBR; BIOS/XBIOS/GEMDOS/AES-VDI trapy.
- **Cookie jar**: `_p_cookies` + `include/cookie.h`; ATOS ma własny statyczny jar.
- **VBL**: `bios/vectors.S` + `_vblqueue`/`_vbclock`; ATOS ma centralne IRQ (v10)
  i własny Timer C — trzeba podpiąć kolejkę VBL EmuTOS.
- **Pamięć pod adresem 0**: EmuTOS zakłada układ 68000 (wektory @0, sysvars @0x380+,
  TPA od `_membot`). ATOS też ustawia VBR=0 (`kernel_main.c`), więc model jest zgodny.
- **68030/VBR**: EmuTOS działa stylem 68000. VBR per proces (przyszły multitasking
  ATOS) wymaga, by warstwa zgodności nie liczyła na VBR=0 na stałe — do rozstrzygnięcia
  w `docs/CPU030.md`.

## 6. Obsługa .PRG

- EmuTOS: `bdos/kpgmld.c` (nagłówek 0x601A, relokacje, basepage) + `bdos/proc.c`
  (basepage, środowisko, uruchomienie). ATOS ma własny loader w `system/tos_prg.c`
  (relokacja poprawna) — do porównania i ewentualnego zastąpienia.

## 7. Trzy warianty (do decyzji; NIE wybierać automatycznie)

| Aspekt | A: własny AES/VDI | B: wybrane komponenty EmuTOS w ATOS | C: EmuTOS jako warstwa zgodności |
|---|---|---|---|
| Zostaje z ATOS | BIOS, kernel, VFS/FAT, video_driver_t | BIOS, kernel, VFS/FAT, video_driver_t, kompozytor | BIOS, kernel, sterowniki, video_driver_t |
| Zastąpione | — | GEMDOS+VDI+AES z EmuTOS pod video_driver_t | cały GEM/TOS = EmuTOS (osobny moduł) |
| Boot | ATOS | ATOS → jądro → usługi EmuTOS | ATOS → obraz EmuTOS |
| .PRG | własny loader | loader EmuTOS | loader EmuTOS |
| VDI | własny (LEGACY) | EmuTOS VDI + nasz backend | EmuTOS VDI |
| AES | własny (LEGACY) | EmuTOS AES | EmuTOS AES |
| ET4000 | przez video_driver_t | przez video_driver_t | przez video_driver_t |
| GPL | tylko zaadaptowane fragmenty | całość GPL-2.0+ | ATOS agregat; EmuTOS osobno GPL |

## 8. Co w ATOS staje się zbędne przy B/C

- `system/tos_vdi.c` (LEGACY) i AES w `system/tos_prg.c` (LEGACY),
- większość własnej persony TOS i `tos_prg.c` loadera (gdy przejmie EmuTOS GEMDOS),
- tymczasowe łaty w kompozytorze — kompozytor zostaje, ale nad `video_driver_t`.

## 9. Rekomendacja inżynierska (do zatwierdzenia)

Przy decyzji **A+GPL** naturalny jest **wariant B**: ATOS BIOS/kernel/video_driver_t
zostają, a GEMDOS/VDI/AES bierzemy ze źródeł EmuTOS (GPL-2.0+, z notami), podpinając
ich rysowanie pod `video_driver_t`. Daje to wysoką zgodność bez oddawania kontroli
nad bootem/sprzętom i bez utraty własnego pulpitu docelowo.

> Uwaga metodologiczna: dokument opiera się na inwentarzu i kluczowych symbolach
> źródeł EmuTOS z repo. Głębsza analiza (np. dokładne API `Vwk` i kolejki VBL)
> przed integracją.
