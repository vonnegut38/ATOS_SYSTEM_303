# ATOS compatibility test suite (v10, Phase 16)

Goal: **real Atari `.PRG`** programs, each printing `PASS` or `FAIL: reason`,
runnable on **TOS 2.06**, **EmuTOS** and **ATOS**.  Differences between platforms
are the real compatibility signal.

| Directory | Area | Planned tests |
|---|---|---|
| `bios/` | trap #13 | Bconstat, Bconin, Bconout, Rwabs, Setexc, Tickcal, Getbpb, Bcostat, Mediach |
| `xbios/` | trap #14 | Physbase, Logbase, Getrez, Setscreen, Setpalette, Setcolor, Vsync, Supexec, Initmous |
| `gemdos/` | trap #1 | Fopen, Fread, Fseek, Fsfirst/Fsnext, Pexec, Malloc, Mshrink |
| `vdi/` | trap #2 VDI | v_opnvwk, vs_clip, v_bar, v_gtext |
| `aes/` | trap #2 AES | appl_init, wind_create/open/get/close/delete, appl_exit |
| `prg/` | loader | 0x601A header, TEXT/DATA/BSS, relocation, basepage, command line |
| `video/` | video | Physbase/Logbase/Getrez, mode, palette |
| `memory/` | memory | Malloc/Mfree/Mshrink bounds |

## Reference behaviour (Phase 17)

* **TOS 2.06** = historic reference behaviour.
* **EmuTOS** = open-source implementation reference.
* **ATOS** = implementation under test.

Do not copy TOS bugs unless they are required for compatibility.  Document every
deliberate difference (see `docs/REFERENCE-BEHAVIOUR.md`).

## Status

Structure created by v10.  The `.PRG` binaries are **not written yet**: they
depend on the compatibility-layer decision (Milestone V10-C).  Host tests remain
in the parent directory (`tests/test_*.c`, `tests/test_*.py`).
