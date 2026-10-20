#!/usr/bin/env python3
"""Build/test native text profile; --install updates its stopped Hatari image."""
import argparse
import fcntl
from pathlib import Path
import subprocess

root=Path(__file__).resolve().parents[1]
ap=argparse.ArgumentParser(description=__doc__)
ap.add_argument('--install',action='store_true')
a=ap.parse_args()
def run(*args):subprocess.run(args,cwd=root,check=True)
image=None
if a.install:
    image=(root/'work/atos-v2.img').open('r+b')
    try:
        fcntl.flock(image,fcntl.LOCK_EX|fcntl.LOCK_NB)
    except BlockingIOError:
        raise SystemExit('Obraz jest używany. Zamknij testową instancję Hatari przed instalacją.')
run('gmake','-j4','all')
# Hand-written assembly that the kernel calls synchronously MUST honour the ABI:
# d2-d7/a2-a6 are callee-saved. A clobber here corrupts a live kernel register
# and turns into a wild jump (see the shell_do_path_display panic). Application
# .S files are excluded: an app runs as its own process, not as a subroutine.
run('python3','tools/asm_abi_audit.py','--check',
    'system/shell_module.S','system/kernel_entry.S','boot/exception.S',
    'boot/mmu030.S','system/boot_entry.S','system/tos_entry.S',
    'system/tos_start.S','system/vofa_entry.S','system/video_bus.S',
    'system/gui_app_runtime.S','system/acsi_module.S','system/core_module.S',
    'system/event_module.S','system/input_module.S','system/proc_module.S',
    'system/timer_module.S','system/vfs_module.S')
run('gmake','check','check-terminal','check-console','check-sessions','check-commander','check-sysinfo','check-languages')
if a.install:
    run('python3','tools/make_bios_cfg.py','work/BIOS.CFG','--auto-tos','0','--boot-delay','1')
    # Applications live once, in \SYSTEM\APP, which the kernel default PATH
    # includes; installing them in \SYSTEM too only duplicated ~37 KB.
    for name in ['BOOT.SYS','KERNEL.SYS','ACSI.SYS','CORE.SYS','VFS.SYS','PROC.SYS','TIMER.SYS','EVENT.SYS','INPUT.SYS','SHELL.SYS','TOS.SYS','VOFA.SYS','INIT.APP','GFX.APP','GUI.APP']:
        run('python3','tools/install_boot_sys.py','work/atos-v2.img','build/system/'+name)
    for name in ['SYSINFO.APP','SETTINGS.APP','SETUP.APP','HW.APP','PATH.APP','HANG.APP','GFXBENCH.APP','WMDEMO.APP','DESKTOP.APP','WINDEMO.APP','AC.APP','TASKMGR.APP','PLASMA.APP','ARKANOID.APP']:
        run('python3','tools/install_boot_sys.py','work/atos-v2.img','build/system/'+name,'--directory','SYSTEM/APP')
    # AC is the graphical successor of the text commander: install it as
    # COMMAND.APP (the shell's 'command') and keep the text-mode commander
    # reachable as TEXTCMD.APP, so neither is lost.
    run('cp','build/system/AC.APP','work/COMMAND.APP')
    run('cp','build/system/COMMAND.APP','work/TEXTCMD.APP')
    run('python3','tools/install_boot_sys.py','work/atos-v2.img','work/COMMAND.APP','--directory','SYSTEM/APP')
    run('python3','tools/install_boot_sys.py','work/atos-v2.img','work/TEXTCMD.APP','--directory','SYSTEM/APP')
    # The first real .PRG test for the TOS personality.
    run('python3','tools/install_boot_sys.py','work/atos-v2.img','build/system/TOSPROBE.PRG','--directory','SYSTEM/TEST')
    # Desktop launcher entries (user-editable; preserved across reinstall).
    run('python3','tools/install_boot_sys.py','work/atos-v2.img','config/LAUNCH.CFG','--directory','SYSTEM','--data','--if-missing')
    # Graphics drivers live in \SYSTEM\DRV, one module per card family; the
    # kernel picks by the chipset the BIOS reported.  VOFA.SYS stays in \SYSTEM
    # as the compatibility fallback for images without the driver directory.
    run('cp','build/system/VOFA.SYS','work/ET4000.SYS')
    run('python3','tools/install_boot_sys.py','work/atos-v2.img','work/ET4000.SYS','--directory','SYSTEM/DRV')
    # \BIOS\BIOS.CFG is the canonical location the ROM reads and rewrites;
    # the root copy is only the legacy fallback, so seed both.
    run('python3','tools/install_boot_sys.py','work/atos-v2.img','work/BIOS.CFG','--directory','BIOS','--data')
    run('python3','tools/install_boot_sys.py','work/atos-v2.img','work/BIOS.CFG','--root','--data')

    for name in ['EN.LNG','PL.LNG']:
        run('python3','tools/install_boot_sys.py','work/atos-v2.img','languages/'+name,'--directory','SYSTEM/LANG','--data')
    run('python3','tools/install_boot_sys.py','work/atos-v2.img','languages/LANGUAGE.CFG','--data','--if-missing')
