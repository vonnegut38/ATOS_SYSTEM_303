# Raport: TT RAM w Hatari (Mega ST + TF536) i użycie TT w ATOS

Data: 2026-09-25

## Cel

Uruchomić w Hatari konfigurację "Mega ST + TF536" z TT RAM (fast RAM), a jeśli
BIOS ją wykryje — lokować w TT część systemu i uruchamiane aplikacje.

## Ustalenia

### 1. Hatari mapuje TT RAM tylko dla maszyn TT/Falcon

Zweryfikowane empirycznie:
- `--machine ste --cpulevel 3 --ttram 64` -> `memory`: TT-RAM FREE 0 KB.
- `--machine megaste --cpulevel 3 --ttram 64` -> SYSINFO: TT-RAM total/free
  0 KiB.
- `--machine st/megast` + 68030 w ogóle nie bootuje ATOS: loader zatrzymuje
  się na `BOOT PARTITION NOT FOUND` (odczyt tablicy partycji przez DMA ST)
  i wchodzi do menu BIOS-u.

### 2. Hatari wymusza STE dla ROM-u ATOS

Log `work/hatari-*.log`:

```
ERROR: This TOS version does not work in TT/Falcon mode.
 ==> Switching to STE mode now.
```

Nagłówek ROM-u ATOS ma wersję TOS `0x0206` pod offsetem 2
(`build/bios_mste.bin`), a Hatari dopuszcza tryb TT/Falcon tylko dla TOS >= 3.0.
Dlatego `--machine tt --ttram 64` i tak kończy jako STE bez TT RAM.

### 3. Obejście: podmiana wersji w nagłówku ROM-u

Kopia ROM-u z offsetem 2 = `0x0306` wpuszcza Hatari w tryb TT:

```
32MHz/030(CED)/68882 4/64MB TT, TOS 3.06
Automatically enabling 32-bit addressing to support TT-RAM.
```

Czyli Hatari **potrafi** dać 4 MB ST + 64 MB TT. Problem: BIOS ATOS (STE) nie
renderuje się na TT (czarny ekran, bus errors na próbach sprzętowych) — ATOS to
system ST/STE, nie TT.

## Co ATOS już robi z TT RAM

Gdy TT RAM jest obecne (`h->tt_ram_bytes != 0`), `kmem` ma pulę TT pod
`0x01000000` (`kernel_memory.c`), a `prefer_tt` trafia tam:

| Bufor | Plik | prefer_tt |
|---|---|---|
| Block cache (128 sektorów, 64 KB) | kernel_block.c:70 | TAK |
| Obraz aplikacji (spawn) | kernel_module.c:433,437 | TAK |
| Stos aplikacji (spawn) | kernel_module.c:440 | **TAK (poprawione)** |
| Obraz/stos klona | kernel_exception.c:947,953 | TAK |
| TOS: ekran/sterta/chunk | tos_prg.c | TAK |
| DMA staging | kernel_block.c:67 | NIE (DMA nie sięga TT) |
| desktop framebuffer / backbuffer | kernel_graphics.c:273,787 | NIE |
| WM app/console surface | kernel_wm.c:352,425 | NIE |
| VOFA shadow | kernel_video.c:19 | NIE |

Poprawka: stos spawna był ST-only mimo komentarza w kodzie klona ("Stack is
CPU-only (never DMA), so it can live in the faster TT-RAM") — ujednolicone na
TT. To realizuje "aplikacje w TT".

## ROZWIĄZANIE: patched Hatari z atari-030-port

W poprzedniej sesji zbudowano lokalnie Hatari 2.6.1-devel z dodatkami
(`/Users/xxx/Documents/Deepseek_test/atari-030-port`):

- `hatari-src/` — źródła + `build/src/hatari` (~16 MB, Release)
- `hatari-tf536` — launcher na ten binarny
- nowe opcje:
  - `--tf536 <bool>` — "Model a TF536 accelerator in ST/MegaST: 68030,
    64 MB ALT-RAM, IDE"
  - `--graphics-card <x>` (none/vofa), `--gfx-chip <x>` (et4000/et4000w32i)

To NIE jest `/opt/homebrew/bin/hatari` (stock 2.6.1 bez tych opcji).

### Uruchomienie (dokładnie konfiguracja fizyczna)

`ATOS 2.0/run-tf536.sh` (boot z IDE TF536; `BOOT=acsi` dla ACSI):

```
hatari-tf536 --machine megast --cpulevel 3 --cpuclock 32 --memsize 4 --ttram 64 \
  --tf536 on --graphics-card vofa --gfx-chip et4000 --mono \
  --acsi 0..7=none --ide-master work/atos-v2.img \
  --tos build/bios_mste.bin --patch-tos false
```

### Wynik (SYSINFO, shots/tt536/f-sysinfo.png)

- Machine: **Atari Mega ST**, Processor: **Motorola 68030**
- ST-RAM: 4096 KiB (free 3702)
- **TT-RAM: 65536 KiB total, 49031 KiB free** (~16 MB już zajęte w TT)
- Largest block: ST 3701 KiB / **TT 49051 KiB**
- Active output: **VOFA + ET4000**, "Card detected by BIOS: ET4000 / W32 family"
- BIOS: **ALT-RAM: 65536 KB**

Czyli BIOS wykrywa ALT-RAM, a `kmem` + `prefer_tt` lokują tam block cache,
obrazy/stosy aplikacji i bufory TOS.

## Wniosek

- Docelowa konfiguracja (Mega ST + TF536 + ET4000, boot IDE) **działa** w
  lokalnie zbudowanym Hatari i jest tożsama z fizycznym sprzętem użytkownika.
- Stock Hatari 2.6.1 tego nie potrafi (brak `--tf536`/karty i wymusza STE dla
  ROM-u ATOS).
- Poprawka stosu spawna (`kernel_module.c`) dopełnia "aplikacje w TT".

## Stabilizacja bootu (kontynuacja)

Ustalenie: **IDE i ALT-RAM nie są źródłem niestabilności**. Poll zrzutów co 5 s
pokazuje, że jądro ATOS wstaje w ~5 s i loguje:

```
GRAPHICS BACKEND: SHIFTER 640x400 / BLITTER READY
ST-RAM POOL: 3757 KB FREE
TT-RAM POOL: 49151 KB FREE
MEMORY SELF-TEST: PASS
```

Z `GFX=shifter` (`--graphics-card none`) cały boot log jest widoczny i zawiera:

```
BLOCK CACHE: 128 SECTORS / TT-RAM
```

— block cache naprawdę ląduje w TT. IDE (`--ide-master`) działa w obu wariantach.

Czarny ekran występował tylko z `--graphics-card vofa` (ET4000). **Naprawione.**

### Przyczyna i poprawka (VOFA)

- `kgfx_console_present()` rysował konsolę na sztywno do `SCREEN_BASE`, a
  `kvideo_flush()` kopiował do VRAM karty bufor `front`, który przy aktywnym
  VOFA wskazywał `desktop_framebuffer` — więc na kartę szły zera.
- Dodatkowo most VOFA w Hatari **kasuje raster Shiftera co ramkę**
  (`memset(STRam + VideoBase, 0, 32000)`), więc obraz konsoli z `SCREEN_BASE`
  i tak był kasowany, zanim dotarł do karty.
- Poprawka: przy aktywnym VOFA konsola renderuje **wprost do VRAM karty**
  (`kvideo_scanout()` z `kernel_video.c`), a `kvideo_flush()` synchronizuje
  kopię cienia. Bez VOFA pozostaje `SCREEN_BASE` jak dotąd.

Weryfikacja (`shots/vofafix/`): `f12.png` — pełny boot log z
`BLOCK CACHE: 128 SECTORS / TT-RAM` i `VIDEO: VOFA REQUESTED / 640x480 MONO`;
`f-memory.png` — `memory`: ST-RAM FREE 3716 KB, **TT-RAM FREE 49059 KB**.
Konsola widoczna od startu, bez potrzeby uruchamiania aplikacji GUI.
