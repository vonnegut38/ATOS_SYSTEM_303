# ANALIZA: luki warstwy zgodności TOS w ATOS 2.1 (gap list)

Raport agenta analitycznego (read-only) z odwołaniami `plik:linia`. Numery
prawdziwych pól/funkcji AES/VDI pochodzą z EmuTOS
`files-pasted-by-the-user-projekt/work/emutos-reference/include/aesdefs.h`.

## Weryfikacja własna (to sprawdziłem i naprawiłem w tym przebiegu)

| pozycja z raportu | weryfikacja | stan |
|---|---|---|
| `wind_get` — **złe numery pól** (raport: prywatna numeracja przesunięta o 3) | potwierdzone w `aesdefs.h:534-545`: WF_KIND=1, WF_NAME=2, WF_INFO=3, WF_WXYWH=4, WF_CXYWH=5, WF_PXYWH=6, WF_FXYWH=7, WF_HSLIDE=8, WF_VSLIDE=9, WF_TOP=10, WF_FIRSTXYWH=11, WF_NEXTXYWH=12 | **NAPRAWIONE** (był to mój błąd z §57) |
| `wind_set` honoruje tylko `f==2`, a to jest WF_NAME → zapisuje połówki wskaźnika jako geometrię | potwierdzone | **NAPRAWIONE** (WF_NAME=2 → nazwa, WF_CXYWH=5 → geometria) |
| `Pterm` zwraca 76 zamiast kodu wyjścia | potwierdzone: kod jest pod `usp+2`, `usp` trzyma słowo funkcji (0x4C = 76) | **NAPRAWIONE** |

## Najważniejsze luki wg raportu (kolejność wg wpływu)

1. **Zasoby AES nie istnieją**: `appl_init` nigdy nie ustawia `ap_ptree`
   (`tos_prg.c:1923-1931`), `rsrc_gaddr` zwraca 0 (`2334-2345`), brak `rsrc_load`
   (110) → `tree == NULL` → dereferencja NULL albo dzielenie przez `ob_width = 0`.
   **To największy „zerotwórca" dla programów z dialogami.**
2. `wind_get`/`wind_set` (naprawione).
3. Brak `form_do` (50), `fsel_input`/`fsel_exinput` (90/91), `shel_write` (121)
   → `default: return 0` = dla selektora plików „Cancel".
4. `Pterm` (naprawione).
5. BIOS `Setexc` (5) i `Rwabs` (4) = **0**; brak VBL — `_vblsem`/`_nvbls`/
   `_vblqueue`/`_vbclock`/`_frclock` nieustawione → program czekający na
   `_vbclock` zawiesi się.
6. TPA tylko w TT-RAM (`0x04000000-0x05000000`, `tos_prg.c:3029-3031`) — **brak
   fallbacku do ST-RAM**, więc na 68000 nie ma gdzie załadować .PRG;
   `_membot`/`_memtop` opisują ST-RAM, nie TPA programu.
7. Martwy `kernel_tos_use_surface` → `_v_bas_ad`/`_sshiftmod` nieaktualne podczas
   runu (program rysujący wg `_v_bas_ad` maluje po pulpicie).
8. VDI: `v_get_pixel`/`vq_mouse`/`vq_key_s` = **0**, `vst_load_fonts` = **0**
   (potencjalny dzielnik), brak `vst_point` (107) i `vs_color` (14) mimo
   zdefiniowania w nagłówku.
9. GEMDOS: `Dgetpath` zawsze pusty, brak `Dsetpath` (0x3B), `Fsfirst` ucina
   listing po **16** pozycjach, `_drvbits` = tylko C:, brak `Ptermres`/`Fdatime`.
10. `kernel_tos_bootstrap` to atrapa: woła `tos_vdi_call(0x73u,…)`, a 0x73 = 115
    (`vqin_mode`) nie ma case'a → `default: return 0`, po czym i tak drukuje
    „VDI AES READY". Nie otwiera workstation przed pierwszym programem.

## Uwagi strukturalne z raportu

* Blok KBDVECS/IOREC jest alokowany **przed** `tpa_mark` i zwalniany przez
  `tos_tpa_next = tpa_mark` → **przecieka ~1 KB na każde uruchomienie**
  (także na ścieżkach błędów, bez rollbacku).
* IOREC: nikt nigdy nie zapisuje `buf/head/tail` → program czytający
  `Iorec(0/1)` nie dostanie ani jednego bajtu (zgodne z raportem o sprzęcie).
* Basepage: niezerowane dziury `0x30-0x35` (`p_uft[6]` — standardowe uchwyty!),
  `0x60-0x67`, `0x6C-0x6F`, `0x74-0x77`, `0x7C-0x7F` → śmieci z poprzedniego
  uruchomienia. `p_env` to puste „\0\0" bez `PATH=`; brak `the_env` (0x4BE).
* Nagłówek `_sysbase`: brak `os_magic` (MUPB) i `os_root` (pula GEMDOS).
