# ATOS 2.1 — TOS compatibility: handover (start here)

## 0. FIRST ACTION in the new session (one run, decides everything)

The `-mno-pcrel` change removed the app size ceiling but is **not proven safe**.
Since rebuilding apps with it, the desktop wallpaper stays black and F10 no longer
opens the menu. Either the harness misroutes keys, or absolute addressing breaks
apps (loader may relocate the image).

Decide it with an A/B on the SAME image:

1. Install the OLD app build (`-mpcrel`): revert `GUI_APP_CFLAGS` in `Makefile`
   (~line 1145), `gmake TOS=1 build/system/DESKTOP.APP`, install into
   `work/atos-tos1.img` as `SYSTEM/APP/DESKTOP.APP`, boot, press F10.
2. Install the NEW app build (`-mno-pcrel`), boot, press F10.

* old app opens the menu, new one does not  -> `-mno-pcrel` breaks apps.
  Then fix the app load/relocation story instead (check `__app_policy` /
  `linker/app-sys.ld` / the loader), or scope the flag to apps that fit in 32 KB.
* neither opens the menu -> the UI is fine, the harness input is wrong; go to §3.

Do NOT re-derive this from zero: F10 DID open the menu earlier with the old app.

## 1. What is built and verified (do not redo)

TOS.SYS module (`system/tos_module.S`, 264 B) = the personality's WORKING SET
(the private 640x400 screen), not GEMDOS/BIOS/AES/VDI (those stay in the kernel).

Two INDEPENDENT keys in `\SYSTEM\DESKTOP.CFG`, read by the kernel at START
(`tos_cfg_read()` in `system/kernel_main.c`):

| DESKTOP.CFG | loads at boot | buffer |
|---|---|---|
| no keys (default) | 0 -> lazy on first TOS program | ST RAM |
| `TOSTA=1 TOSTT=1` | 1 (preload) | TT RAM (0x108EE10) |
| `TOSTA=1 TOSTT=0` | 1 (preload) | ST RAM (0x1FD520) |

Also verified: 2x run -> `loads=1`, `adopted=1,2`, `fallbacks=0`; module missing
-> `fallbacks=1,2` and the program still runs (fallback visible, not hidden).

Fixed review bugs: D3 preserved on all return paths; error codes never 0
(`-state-100`); `register_service` failure frees the buffer; kernel publishes
TOSC via `kernel_publish_service()` (plain `register_service` needs a module
load window and silently failed).

## 2. What is NOT done

* UI end-to-end unproven: every drive attempt produced byte-identical
  screenshots and `DESKTOP.CFG` unchanged.
* The SETTINGS rows + SAVE SETTINGS exist in source and link, but were never
  seen working.

## 3. Harness facts that cost time (respect them)

* Boot with the desktop takes ~150-190 s. Wait, do not poll for "ATOS 2.0"
  (the desktop replaces the text console, so detection returns False).
* Screenshots MUST use ABSOLUTE paths (relative ones land in Hatari's CWD).
* With the desktop up, keys may go to the shell session behind it, not the
  desktop. Verify routing by typing `run \SYSTEM\TEST\TOSPROBE.PRG` and reading
  markers at `0x810` (START=0x5A5AA5A5 at 0x814, DONE=0x900D at 0x818).
* Menu keys: only Up/Down/Enter/Esc. F10 opens OPTIONS directly
  (`desktop_wm.c` ~line 2225) - do NOT "navigate right".
* `setup_sel` starts on the CURRENT MODE index, not 0. Clamp with Up x12 first,
  then walk down to the exact row.
* Only ONE Hatari at a time: a second instance on the same image gives
  `ERROR: Locking IDE HD file for writing failed`.
* NEVER hardcode `kdbg_*` addresses - re-resolve with `nm` after every rebuild
  (stale addresses produced a false "preload did not run").

## 4. Build / install / test commands

```bash
gmake TOS=1 -j4 all                 # TOS=1 profile
python3 tools/build_atos2.py        # TOS=0 profile (OVERWRITES build/system!)
gmake TOS=1 -j4 all                 # restore TOS=1 artifacts
gmake TOS=1 tosprobe toscolour tosmed tosmono tosdemo

python3 tools/install_boot_sys.py <image> build/system/KERNEL.SYS
python3 tools/install_boot_sys.py <image> build/system/TOS.SYS
python3 tools/install_boot_sys.py <image> build/system/DESKTOP.APP --directory SYSTEM/APP
python3 tools/install_boot_sys.py <image> work/DESKTOP.CFG --directory SYSTEM --data
```

Images: `work/atos-tos1.img` (desktop on), `work/atos-tos-mod.img` (test copy).
`work/rename_app.py <img> <path> <NEWNAME.EXT>` renames inside the FAT image
(used to disable/enable `DESKTOP.APP`, retire `ATOS.CFG` -> `ATOS.BAK`).

Helpers: `atos2-tools/run_demo_interactive.py -` (visible window, boot only),
`work/bootdiag.py` (nm-resolved TOS diagnostics), `atos2-tools/scenario_tos_module.py`.
`work/img_cat.py` is BROKEN for files - debug it or read the image with the
inline FAT dumper used in the session.

## 5. The size ceiling (context, already fixed but see §0)

`GUI_APP_CFLAGS` had `-mpcrel`: 933 `R_68K_PC16` relocations in `desktop_wm.o`,
image 34 212 B vs the +-32 767 limit -> `relocation truncated to fit:
R_68K_PC16 against .bss.fb_more`. `-mno-pcrel` gives 0 relocations.
Reordering sections cannot help: the body itself exceeds the limit.
