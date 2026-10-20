# ATOS Desktop na warstwie EmuTOS (kierunek v10)

Decyzja: **pulpit budujemy jako aplikację GEM na warstwie zgodności EmuTOS**,
a nie jako kolejną warstwę LEGACY-owego kompozytora jądra. Powód: nasz stary
kompozytor ma błąd arbitrażu odświeżania (miganie black↔gray), a EmuTOS daje
sprawdzony tor AES/VDI — na nim budujemy nasz wygląd.

## Architektura

    DESKTOP.PRG / MODULES (nasz kod, wlasny)
        |
        v
    AES (appl_init / wind_* / evnt_multi / menu_*)      <- EmuTOS
    VDI (v_opnvwk / v_bar / v_gtext / vs_clip / ...)    <- EmuTOS
        |
        v
    EmuTOS (GEMDOS/BIOS/XBIOS) uruchomiony pod ATOS BIOS (loader RAM-TOS)
        |
        v
    ATOS BIOS + sprzet (docelowo ET4000 przez video_driver_t)

Nasz pulpit NIE zna sprzętu — używa AES/VDI. EmuTOS jest GPL-2.0+ (noty już są
w `licenses/`); nasza aplikacja to nasz kod.

## Jak to zbudować naszym toolchainem (bez mint)

Mamy `m68k-elf-gcc`. Aplikację GEM można zbudować jako:
- C + cienkie bindingi AES/VDI przez `trap #2` (inline asm / mały stub .S),
- `-m68000 -ffreestanding`, własny linkerscript do surowego obrazu,
- nagłówek `.PRG` 0x601A + relokacje (narzędzie `mk_tosprg.py` / własny wrapper).

To samo podejście co nasze istniejące `system/*_app.S`, ale dla GEMDOS/AES
(trap #2) zamiast trap #0.

## Pierwszy milestone (DESKTOP.PRG)

1. `appl_init()`, `graf_handle()`,
2. `v_opnvwk()` — otwarcie stacji VDI,
3. rysowanie: górny pasek (v_bar) + **zegar** (v_gtext) + dolny launcher (v_bar),
4. pętla zdarzeń `evnt_multi(MU_TIMER|MU_KEYBD|MU_BUTTON)` z odświeżaniem
   tylko zmienionego fragmentu (damage = przeliczenie godziny),
5. auto-start z `\AUTO\DESKTOP.PRG` na obrazie EmuTOS-pod-ATOS.

## Zasady (z audytu)

- Jedna konwencja współrzędnych; zero `wm_app_flip`.
- Rysujemy tylko uszkodzony fragment (wzór EmuTOS), nie cały pulpit.
- Moduły (zegar, sieć, monitor, launcher) jako małe aplikacje/usługi AES —
  zgodnie z `docs/DESKTOP-ARCHITECTURE.md`, ale oparte o AES/VDI EmuTOS.
- Docelowo ET4000/W32i 1024x768x8 — przez EmuTOS VDI + nasz backend.

## Czego NIE robimy

- Nie rozwijamy LEGACY `tos_vdi.c` / AES w `tos_prg.c`.
- Nie naprawiamy dalej starego kompozytora jądra (poza tym, co już jest).
- Nie dodajemy 16bpp, animacji ani multitaskingu na tym etapie.
