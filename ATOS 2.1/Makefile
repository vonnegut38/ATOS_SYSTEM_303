CROSS ?= m68k-elf-
.DEFAULT_GOAL := all
CC := $(CROSS)gcc
OBJCOPY := $(CROSS)objcopy
SIZE := $(CROSS)size
# Extra flags for test-only builds (empty in normal builds).
CFLAGS := -m68000 -Os -ffreestanding -fno-builtin -fomit-frame-pointer -Wall -Wextra -Werror -std=gnu11 -Iinclude
ASFLAGS := -m68000 -ffreestanding -Iinclude
# TOS persona (tos_prg.c/tos_vdi.c/tos_entry.S/tos_start.S) is optional at build
# time.  TOS=0 is the native-text default: it saves ~24 KB text and ~26 KB BSS.
# Build with TOS=1 to restore the GEMDOS/BIOS/XBIOS/AES-VDI compatibility layer.
TOS ?= 0
# Profile stamp.  CONFIG_TOS changes what is compiled, but make cannot see that
# from timestamps, so switching TOS=0 <-> TOS=1 reused the other profile's
# objects and the link failed ("multiple definition of kernel_tos_run_program").
# Every system object depends on the profile it was built with.  $(wildcard) is
# empty for objects that do not exist yet - those compile fresh anyway.
PROFILE_STAMP := build/.profile
ifeq ($(TOS),1)
TOS_OBJS := build/system/tos_vdi.o build/system/tos_gdos.o build/system/tos_prg.o build/system/tos_entry.o build/system/tos_start.o
else
TOS_OBJS :=
endif
$(wildcard build/system/*.o) build/system/tos_prg.o build/system/tos_vdi.o build/system/tos_gdos.o: $(PROFILE_STAMP)

# The stamp must name EVERY flag set that decides how an object is compiled, not
# only CFLAGS.  When GUI_APP_CFLAGS was reverted from -mno-pcrel to -mpcrel the
# stamp stayed identical, so make kept the absolute-addressed app objects and
# only the app whose .o had been deleted by hand came back PC-relative; the
# mixed build still failed tools/check_gui_apps.py (SYSINFO).  SHELL_CFLAGS is
# in here for the same reason.
STAMP_FLAGS = $(CC) $(CFLAGS) | $(GUI_APP_CFLAGS) | $(SHELL_CFLAGS)
$(PROFILE_STAMP): FORCE
	@printf '%s\n' '$(STAMP_FLAGS)' | cmp -s - $@ || printf '%s\n' '$(STAMP_FLAGS)' > $@

.PHONY: FORCE
FORCE:

CFLAGS += $(ARK_EXTRA_CFLAGS)
CFLAGS += -DCONFIG_TOS=$(TOS)
ASFLAGS += -DCONFIG_TOS=$(TOS)
# Kernel compositor (kernel_wm.c).  Default 1: a desktop app switches its
# windows onto the compositor when it enters desktop mode.  The text profile is
# unaffected because kernel_wm_frame_tick() stays gated on desktop mode.
# Build with WM=0 for the pre-compositor behaviour.
WM ?= 1
CFLAGS += -DCONFIG_WM=$(WM)
ASFLAGS += -DCONFIG_WM=$(WM)
LDFLAGS := -nostdlib -nostartfiles -Wl,-T,linker/mste-rom.ld -Wl,-Map,build/bios_mste.map -Wl,--gc-sections

OBJ := build/boot/reset.o build/boot/memdetect.o build/boot/exception.o build/boot/ramjump.o build/boot/mmu030.o build/boot/mmu_boot.o build/hw/machine.o \
	build/hw/memory.o build/hw/shifter.o build/hw/ikbd.o build/hw/rtc.o build/hw/expansion.o build/hw/nova.o build/hw/acsi.o build/hw/ide.o build/hw/block_device.o build/hw/config.o build/hw/nvram.o build/hw/fdc.o build/hw/fat12.o build/hw/psg.o \
	build/hw/vofa_rom.o build/hw/vofa_et4000_rom.o \
	build/diag/tests.o build/system/kernel_blitter.o \
	build/ui/font.o build/ui/console.o build/ui/post.o build/ui/setup.o \
	build/ui/diagnostics.o build/ui/hardware.o build/ui/boot.o build/ui/lang.o build/ui/exception_report.o build/main.o

OBJ += build/loader/tos.o build/loader/bootstage.o build/ui/tos.o
DEP := $(OBJ:.o=.d) build/boot/reset_st.d build/system/boot_entry.d \
	build/system/boot_main.d build/system/kernel_entry.d build/system/kernel_main.d \
	build/system/kernel_memory.d build/system/kernel_exception.d build/system/kernel_module.d build/system/kernel_graphics.d build/system/emutos_vdi_mono.d build/system/kernel_blitter.d build/system/kernel_surface.d \
	build/system/kernel_block.d \
	build/system/kernel_fat.d \
	build/system/kernel_partition.d \
	build/system/acsi_module.d build/system/core_module.d build/system/vfs_module.d build/system/proc_module.d build/system/timer_module.d build/system/event_module.d build/system/input_module.d build/system/shell_module.d build/system/shell_path.d build/system/shell_complete.d build/system/shell_fs.d build/system/hello_app.d build/system/fault_app.d build/system/filetest_app.d build/system/seektest_app.d build/system/argtest_app.d build/system/iotest_app.d build/system/yieldtest_app.d build/system/sleeptest_app.d build/system/sysinfo_app.d build/system/dirtest_app.d build/system/cwdtest_app.d build/system/memtest_app.d build/system/proctest_app.d build/system/pinfo_app.d build/system/ptable_app.d build/system/child_app.d build/system/spawntest_app.d build/system/argchild_app.d build/system/sparg_app.d build/system/fltchild_app.d build/system/fspawn_app.d build/system/coopa_app.d build/system/coopb_app.d build/system/async_app.d build/system/waittest_app.d build/system/blockwait_app.d build/system/ctxframe_app.d build/system/ctxswitch_app.d build/system/ctxrr_app.d build/system/ctxfault_app.d build/system/sleepctx_app.d build/system/pidwait_app.d build/system/wpid_app.d build/system/rspawn_app.d build/system/orphan_app.d build/system/multi_app.d build/system/bgyield_app.d build/system/waitkey_app.d build/system/waitline_app.d build/system/procstress_app.d build/system/stackfault_app.d build/system/pointertest_app.d build/system/heapguard_app.d build/system/busfault_app.d build/system/addressfault_app.d build/system/divfault_app.d build/system/chkfault_app.d build/system/trapvfault_app.d build/system/privfault_app.d build/system/linea_app.d build/system/linef_app.d build/system/trap1fault_app.d build/system/trap15fault_app.d build/system/exceptionwait_app.d build/system/hogtest_app.d build/system/hang_app.d build/system/quota_app.d build/system/holdmem_app.d build/system/hw_app.d

DEP += build/system/ipctest_app.d build/system/svctest_app.d build/system/init_service_app.d build/system/gfx_service_app.d build/system/gfxtest_app.d build/system/surftest_app.d build/system/gui_service_app.d build/system/guitest_app.d build/system/guikey_app.d build/system/guistream_app.d build/system/guimod_app.d build/system/guimouse_app.d build/system/desktop_app.d
DEP += build/system/path_app.d
DEP += build/system/gfx_bench.d
DEP += build/system/wm_demo.d
DEP += build/system/desktop_wm.d
DEP += build/system/windemo.d

DEP += build/system/sysinfo.d
DEP += build/system/arkanoid.d build/system/game.d build/system/app_memory.d
DEP += build/system/kernel_console.d build/system/text_terminal.d build/system/kernel_sessions.d
DEP += build/system/kernel_video.d build/system/kernel_video_boot.d build/system/vofa_et4000.d build/system/vofa_module.d build/system/vofa_entry.d
DEP += build/system/kernel_wm.d $(TOS_OBJS:.o=.d)
DEP += build/system/kernel_pcm.d build/system/modplay.d build/system/micromod.d
-include $(DEP)

.PHONY: all clean check toolchain hatari hatari-st hatari-rgb hatari-mono hatari-medium
# Ikony: stale+ID (system/icons_ids.h) i dane \SYSTEM\ICONS.DAT w formacie v5
# (sekcje 32/48/64) - regenerowane, gdy zmieni sie zestaw PNG albo generator.
system/icons_ids.h system/icons_color.h: tools/mk_icons_color.py $(wildcard system/ICONS/*.png)
	python3 tools/mk_icons_color.py

all: build/bios_mste.bin build/bios_mste_hi.bin build/bios_mste_lo.bin build/atos-bios-tf536-st-dual.bin \
	build/bios_st.bin build/bios_st_hi.bin build/bios_st_lo.bin build/system/BOOT.SYS build/system/KERNEL.SYS build/system/ACSI.SYS build/system/CORE.SYS build/system/VFS.SYS build/system/PROC.SYS build/system/TIMER.SYS build/system/EVENT.SYS build/system/INPUT.SYS build/system/SHELL.SYS build/system/TOS.SYS build/system/HELLO.APP build/system/FAULT.APP build/system/FILETEST.APP build/system/SEEKTEST.APP build/system/ARGTEST.APP build/system/IOTEST.APP build/system/YIELD.APP build/system/SLEEP.APP build/system/SYSINFO.APP build/system/TASKMGR.APP build/system/PLASMA.APP build/system/ICONS.DAT build/system/FIXDISK.APP build/system/DIRTEST.APP build/system/CWDTEST.APP build/system/MEMTEST.APP build/system/PROCTEST.APP build/system/PINFO.APP build/system/PTABLE.APP build/system/CHILD.APP build/system/SPAWN.APP build/system/ARGCHILD.APP build/system/SPARG.APP build/system/FLTCHILD.APP build/system/FSPAWN.APP build/system/COOPA.APP build/system/COOPB.APP build/system/ASYNC.APP build/system/WAITTEST.APP build/system/BWAIT.APP build/system/CTXFRAME.APP build/system/CTXSW.APP build/system/CTXRR.APP build/system/CTXFLT.APP build/system/SLPCTX.APP build/system/PIDWAIT.APP build/system/WPID.APP build/system/RSPAWN.APP build/system/ORPHAN.APP build/system/MULTI.APP build/system/BGYIELD.APP build/system/WAITKEY.APP build/system/WAITLINE.APP build/system/PROCSTR.APP build/system/STKFAULT.APP build/system/PTRTEST.APP build/system/HGUARD.APP build/system/BUSFLT.APP build/system/ADRFLT.APP build/system/DIVFLT.APP build/system/CHKFLT.APP build/system/TRAPVFLT.APP build/system/PRIVFLT.APP build/system/LINEA.APP build/system/LINEF.APP build/system/TRAP1.APP build/system/TRAP15.APP build/system/EXWAIT.APP build/system/HOGTEST.APP build/system/HANG.APP build/system/QUOTA.APP build/system/HOLDMEM.APP build/system/BADPOL.APP build/checksums.txt

all: build/system/IPCTEST.APP
all: build/system/SVCTEST.APP
all: build/system/INIT.APP
all: build/system/SETTINGS.APP build/system/GFX.APP build/system/GFXTEST.APP build/system/SURFTEST.APP build/system/GUI.APP build/system/GUITEST.APP build/system/GUIKEY.APP build/system/GUISTRM.APP build/system/GUIMOD.APP build/system/GUIMOUSE.APP build/system/DESKTOP.APP
all: build/system/PATH.APP
all: build/system/ARKANOID.APP
all: build/system/VOFA.SYS

build/boot/mmu030.o: boot/mmu030.S
	@mkdir -p $(@D)
	$(CC) -m68030 -ffreestanding -Iinclude -MMD -MP -c $< -o $@

build/boot/mmu_boot.o: boot/mmu_boot.c include/tos_loader.h include/hardware.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

build/system/boot_entry.o: system/boot_entry.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

build/system/boot_main.o: system/boot_main.c include/boot_protocol.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -MMD -MP -ffunction-sections -fdata-sections -c $< -o $@

build/system/BOOT.SYS: build/system/boot_entry.o build/system/boot_main.o linker/boot-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/boot-sys.ld -Wl,--gc-sections -o build/system/boot.elf build/system/boot_entry.o build/system/boot_main.o -lgcc
	$(OBJCOPY) -O binary build/system/boot.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/kernel_entry.o: system/kernel_entry.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

build/system/kernel_main.o: system/kernel_main.c include/boot_protocol.h include/kernel_memory.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -MMD -MP -ffunction-sections -fdata-sections -c $< -o $@

build/system/png_icon.o: system/png_icon.c include/png_icon.h
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@
build/system/kernel_theme.o: system/kernel_theme.c include/kernel_theme.h include/kernel_graphics.h system/icons_ids.h
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@
build/system/kernel_graphics.o: system/kernel_graphics.c include/kernel_graphics.h include/kernel_theme.h include/font.h include/hardware.h system/icons_ids.h include/png_icon.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -MMD -MP -ffunction-sections -fdata-sections -c $< -o $@

build/system/emutos_vdi_mono.o: system/emutos_vdi_mono.c include/emutos_vdi_mono.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -MMD -MP -ffunction-sections -fdata-sections -c $< -o $@

build/system/kernel_blitter.o: system/kernel_blitter.c include/kernel_blitter.h include/hardware.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -MMD -MP -ffunction-sections -fdata-sections -c $< -o $@

build/system/kernel_surface.o: system/kernel_surface.c include/kernel_surface.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -MMD -MP -ffunction-sections -fdata-sections -c $< -o $@

build/system/wm_core.o: system/wm_core.c include/wm_core.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -MMD -MP -ffunction-sections -fdata-sections -c $< -o $@

build/system/wm_blit.o: system/wm_blit.c include/wm_blit.h include/wm_core.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -MMD -MP -ffunction-sections -fdata-sections -c $< -o $@

build/system/kernel_wm.o: system/kernel_wm.c include/kernel_wm.h include/wm_core.h include/wm_blit.h include/kernel_graphics.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -MMD -MP -ffunction-sections -fdata-sections -c $< -o $@

build/system/kernel_rtc.o: system/kernel_rtc.c include/rtc.h include/hardware.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -MMD -MP -ffunction-sections -fdata-sections -c $< -o $@

build/system/KERNEL.SYS: build/system/kernel_entry.o build/system/kernel_main.o build/system/kernel_memory.o build/system/kernel_exception.o build/system/kernel_module.o build/system/kernel_graphics.o build/system/kernel_theme.o build/system/text_terminal.o build/system/video_driver.o build/system/irq.o build/system/kernel_audio.o build/system/kernel_pcm.o build/system/kernel_sound_nf.o build/system/kernel_sound_nf_probe.o build/system/pcm_irq.o build/hw/psg.o build/system/desktop_module.o build/system/mod_clock.o build/system/kernel_console.o build/system/kernel_sessions.o build/system/kernel_video.o build/system/kernel_video_boot.o build/system/video_bus.o build/system/emutos_vdi_mono.o build/system/kernel_blitter.o build/system/kernel_surface.o build/system/wm_core.o build/system/wm_blit.o build/system/kernel_wm.o build/system/kernel_game.o build/system/kernel_rtc.o $(TOS_OBJS) build/system/kernel_block.o build/system/kernel_fat.o build/system/kernel_partition.o build/system/png_icon.o build/ui/font.o linker/kernel-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/kernel-sys.ld -Wl,--gc-sections -o build/system/kernel.elf build/system/kernel_entry.o build/system/kernel_main.o build/system/kernel_memory.o build/system/kernel_exception.o build/system/kernel_module.o build/system/kernel_graphics.o build/system/kernel_theme.o build/system/text_terminal.o build/system/video_driver.o build/system/irq.o build/system/kernel_audio.o build/system/kernel_pcm.o build/system/kernel_sound_nf.o build/system/kernel_sound_nf_probe.o build/system/pcm_irq.o build/hw/psg.o build/system/desktop_module.o build/system/mod_clock.o build/system/kernel_console.o build/system/kernel_sessions.o build/system/kernel_video.o build/system/kernel_video_boot.o build/system/video_bus.o build/system/emutos_vdi_mono.o build/system/kernel_blitter.o build/system/kernel_surface.o build/system/wm_core.o build/system/wm_blit.o build/system/kernel_wm.o build/system/kernel_game.o build/system/kernel_rtc.o $(TOS_OBJS) build/system/kernel_block.o build/system/kernel_fat.o build/system/kernel_partition.o build/system/png_icon.o build/ui/font.o -lgcc
	$(OBJCOPY) -O binary build/system/kernel.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/acsi_module.o: system/acsi_module.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

build/system/ACSI.SYS: build/system/acsi_module.o linker/module-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/module-sys.ld -Wl,--gc-sections -o build/system/acsi_module.elf build/system/acsi_module.o -lgcc
	$(OBJCOPY) -O binary build/system/acsi_module.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/core_module.o: system/core_module.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

build/system/CORE.SYS: build/system/core_module.o linker/core-module-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/core-module-sys.ld -Wl,--gc-sections -o build/system/core_module.elf build/system/core_module.o -lgcc
	$(OBJCOPY) -O binary build/system/core_module.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/vfs_module.o: system/vfs_module.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

build/system/VFS.SYS: build/system/vfs_module.o linker/vfs-module-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/vfs-module-sys.ld -Wl,--gc-sections -o build/system/vfs_module.elf build/system/vfs_module.o -lgcc
	$(OBJCOPY) -O binary build/system/vfs_module.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/proc_module.o: system/proc_module.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

build/system/PROC.SYS: build/system/proc_module.o linker/proc-module-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/proc-module-sys.ld -Wl,--gc-sections -o build/system/proc_module.elf build/system/proc_module.o -lgcc
	$(OBJCOPY) -O binary build/system/proc_module.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/timer_module.o: system/timer_module.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

build/system/TIMER.SYS: build/system/timer_module.o linker/timer-module-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/timer-module-sys.ld -Wl,--gc-sections -o build/system/timer_module.elf build/system/timer_module.o -lgcc
	$(OBJCOPY) -O binary build/system/timer_module.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/event_module.o: system/event_module.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

build/system/EVENT.SYS: build/system/event_module.o linker/event-module-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/event-module-sys.ld -Wl,--gc-sections -o build/system/event_module.elf build/system/event_module.o -lgcc
	$(OBJCOPY) -O binary build/system/event_module.elf $@
	python3 tools/patch_boot_checksum.py $@

# TOS compatibility module: loaded on demand when a TOS program starts, or into
# TT RAM right after START when the user ticks the SETTINGS option.
build/system/TOS.SYS: build/system/tos_module.o linker/tos-module-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/tos-module-sys.ld -Wl,--gc-sections -o build/system/tos_module.elf build/system/tos_module.o -lgcc
	$(OBJCOPY) -O binary build/system/tos_module.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/tos_module.o: system/tos_module.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

build/system/input_module.o: system/input_module.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

build/system/INPUT.SYS: build/system/input_module.o linker/input-module-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/input-module-sys.ld -Wl,--gc-sections -o build/system/input_module.elf build/system/input_module.o -lgcc
	$(OBJCOPY) -O binary build/system/input_module.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/shell_module.o: system/shell_module.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

# The shell is a runtime-loaded module (kernel_module.c: entry = image + offset),
# so every C source linked into SHELL.SYS must be position independent.  -mpcrel
# turns static data and string literals into PC-relative references; absolute
# pointers kept in .rodata/.data would still hold the link-time address.
SHELL_CFLAGS = $(CFLAGS) -mpcrel -fno-tree-loop-distribute-patterns -MMD -MP -ffunction-sections -fdata-sections

build/system/shell_path.o: system/shell_path.c
	$(CC) $(SHELL_CFLAGS) -c $< -o $@

build/system/shell_complete.o: system/shell_complete.c
	$(CC) $(SHELL_CFLAGS) -c $< -o $@

build/system/shell_fs.o: system/shell_fs.c include/kernel_module.h
	$(CC) $(SHELL_CFLAGS) -c $< -o $@

build/system/SHELL.SYS: build/system/shell_module.o build/system/shell_path.o build/system/shell_complete.o build/system/shell_fs.o linker/shell-module-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/shell-module-sys.ld -Wl,--gc-sections -o build/system/shell_module.elf build/system/shell_module.o build/system/shell_path.o build/system/shell_complete.o build/system/shell_fs.o -lgcc
	$(OBJCOPY) -O binary build/system/shell_module.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/hello_app.o: system/hello_app.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

build/system/HELLO.APP: build/system/hello_app.o linker/app-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/hello_app.elf build/system/hello_app.o
	$(OBJCOPY) -O binary build/system/hello_app.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/fault_app.o: system/fault_app.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

build/system/FAULT.APP: build/system/fault_app.o linker/app-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/fault_app.elf build/system/fault_app.o
	$(OBJCOPY) -O binary build/system/fault_app.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/filetest_app.o: system/filetest_app.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

build/system/FILETEST.APP: build/system/filetest_app.o linker/app-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/filetest_app.elf build/system/filetest_app.o
	$(OBJCOPY) -O binary build/system/filetest_app.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/seektest_app.o: system/seektest_app.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

build/system/SEEKTEST.APP: build/system/seektest_app.o linker/app-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/seektest_app.elf build/system/seektest_app.o
	$(OBJCOPY) -O binary build/system/seektest_app.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/argtest_app.o: system/argtest_app.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

build/system/ARGTEST.APP: build/system/argtest_app.o linker/app-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/argtest_app.elf build/system/argtest_app.o
	$(OBJCOPY) -O binary build/system/argtest_app.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/iotest_app.o: system/iotest_app.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

build/system/IOTEST.APP: build/system/iotest_app.o linker/app-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/iotest_app.elf build/system/iotest_app.o
	$(OBJCOPY) -O binary build/system/iotest_app.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/yieldtest_app.o: system/yieldtest_app.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

build/system/YIELD.APP: build/system/yieldtest_app.o linker/app-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/yieldtest_app.elf build/system/yieldtest_app.o
	$(OBJCOPY) -O binary build/system/yieldtest_app.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/sleeptest_app.o: system/sleeptest_app.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

build/system/SLEEP.APP: build/system/sleeptest_app.o linker/app-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/sleeptest_app.elf build/system/sleeptest_app.o
	$(OBJCOPY) -O binary build/system/sleeptest_app.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/sysinfo.o: system/sysinfo.c include/system_report.h include/text_terminal.h
	$(CC) $(GUI_APP_CFLAGS) -MMD -MP -c $< -o $@
build/system/SYSINFO.APP: build/system/gui_app_runtime.o build/system/sysinfo.o build/system/text_terminal_app.o linker/app-sys.ld include/system_report.h include/gui_app.h include/text_terminal.h
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/sysinfo_app.elf $(filter %.o,$^) -lgcc
	$(OBJCOPY) -O binary build/system/sysinfo_app.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/hw_app.o: system/hw_app.c include/gui_app.h include/system_report.h
	$(CC) $(GUI_APP_CFLAGS) -MMD -MP -c $< -o $@
build/system/HW.APP: build/system/gui_app_runtime.o build/system/hw_app.o linker/app-sys.ld include/system_report.h include/gui_app.h
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/hw_app.elf $(filter %.o,$^) -lgcc
	$(OBJCOPY) -O binary build/system/hw_app.elf $@

build/system/taskmgr.o: system/taskmgr.c include/gui_app.h include/gui_draw.h
	$(CC) $(GUI_APP_CFLAGS) -MMD -MP -c $< -o $@
build/system/TASKMGR.APP: build/system/gui_app_runtime.o build/system/taskmgr.o linker/app-sys.ld include/gui_app.h include/gui_draw.h
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/taskmgr.elf $(filter %.o,$^) -lgcc
	$(OBJCOPY) -O binary build/system/taskmgr.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/plasma.o: system/plasma.c include/gui_app.h include/gui_draw.h system/icons_ids.h
	$(CC) $(GUI_APP_CFLAGS) -MMD -MP -c $< -o $@
build/system/PLASMA.APP: build/system/gui_app_runtime.o build/system/plasma.o linker/app-sys.ld include/gui_app.h include/gui_draw.h
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/plasma.elf $(filter %.o,$^) -lgcc
	$(OBJCOPY) -O binary build/system/plasma.elf $@
	python3 tools/patch_boot_checksum.py $@
	python3 tools/patch_boot_checksum.py $@
all: build/system/HW.APP

build/system/dirtest_app.o: system/dirtest_app.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

build/system/DIRTEST.APP: build/system/dirtest_app.o linker/app-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/dirtest_app.elf build/system/dirtest_app.o
	$(OBJCOPY) -O binary build/system/dirtest_app.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/cwdtest_app.o: system/cwdtest_app.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

build/system/CWDTEST.APP: build/system/cwdtest_app.o linker/app-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/cwdtest_app.elf build/system/cwdtest_app.o
	$(OBJCOPY) -O binary build/system/cwdtest_app.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/memtest_app.o: system/memtest_app.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

build/system/MEMTEST.APP: build/system/memtest_app.o linker/app-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/memtest_app.elf build/system/memtest_app.o
	$(OBJCOPY) -O binary build/system/memtest_app.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/proctest_app.o: system/proctest_app.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

build/system/PROCTEST.APP: build/system/proctest_app.o linker/app-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/proctest_app.elf build/system/proctest_app.o
	$(OBJCOPY) -O binary build/system/proctest_app.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/pinfo_app.o: system/pinfo_app.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

build/system/PINFO.APP: build/system/pinfo_app.o linker/app-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/pinfo_app.elf build/system/pinfo_app.o
	$(OBJCOPY) -O binary build/system/pinfo_app.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/ptable_app.o: system/ptable_app.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

build/system/PTABLE.APP: build/system/ptable_app.o linker/app-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/ptable_app.elf build/system/ptable_app.o
	$(OBJCOPY) -O binary build/system/ptable_app.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/child_app.o: system/child_app.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

build/system/CHILD.APP: build/system/child_app.o linker/app-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/child_app.elf build/system/child_app.o
	$(OBJCOPY) -O binary build/system/child_app.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/spawntest_app.o: system/spawntest_app.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

build/system/SPAWN.APP: build/system/spawntest_app.o linker/app-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/spawntest_app.elf build/system/spawntest_app.o
	$(OBJCOPY) -O binary build/system/spawntest_app.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/argchild_app.o: system/argchild_app.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

build/system/ARGCHILD.APP: build/system/argchild_app.o linker/app-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/argchild_app.elf build/system/argchild_app.o
	$(OBJCOPY) -O binary build/system/argchild_app.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/sparg_app.o: system/sparg_app.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

build/system/SPARG.APP: build/system/sparg_app.o linker/app-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/sparg_app.elf build/system/sparg_app.o
	$(OBJCOPY) -O binary build/system/sparg_app.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/fltchild_app.o: system/fltchild_app.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

build/system/FLTCHILD.APP: build/system/fltchild_app.o linker/app-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/fltchild_app.elf build/system/fltchild_app.o
	$(OBJCOPY) -O binary build/system/fltchild_app.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/fspawn_app.o: system/fspawn_app.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

build/system/FSPAWN.APP: build/system/fspawn_app.o linker/app-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/fspawn_app.elf build/system/fspawn_app.o
	$(OBJCOPY) -O binary build/system/fspawn_app.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/coopa_app.o: system/coopa_app.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

build/system/COOPA.APP: build/system/coopa_app.o linker/app-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/coopa_app.elf build/system/coopa_app.o
	$(OBJCOPY) -O binary build/system/coopa_app.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/coopb_app.o: system/coopb_app.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

build/system/COOPB.APP: build/system/coopb_app.o linker/app-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/coopb_app.elf build/system/coopb_app.o
	$(OBJCOPY) -O binary build/system/coopb_app.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/async_app.o: system/async_app.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

build/system/ASYNC.APP: build/system/async_app.o linker/app-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/async_app.elf build/system/async_app.o
	$(OBJCOPY) -O binary build/system/async_app.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/waittest_app.o: system/waittest_app.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

build/system/WAITTEST.APP: build/system/waittest_app.o linker/app-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/waittest_app.elf build/system/waittest_app.o
	$(OBJCOPY) -O binary build/system/waittest_app.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/blockwait_app.o: system/blockwait_app.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

build/system/BWAIT.APP: build/system/blockwait_app.o linker/app-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/blockwait_app.elf build/system/blockwait_app.o
	$(OBJCOPY) -O binary build/system/blockwait_app.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/ctxframe_app.o: system/ctxframe_app.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

build/system/CTXFRAME.APP: build/system/ctxframe_app.o linker/app-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/ctxframe_app.elf build/system/ctxframe_app.o
	$(OBJCOPY) -O binary build/system/ctxframe_app.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/ctxswitch_app.o: system/ctxswitch_app.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

build/system/CTXSW.APP: build/system/ctxswitch_app.o linker/app-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/ctxswitch_app.elf build/system/ctxswitch_app.o
	$(OBJCOPY) -O binary build/system/ctxswitch_app.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/ctxrr_app.o: system/ctxrr_app.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

build/system/CTXRR.APP: build/system/ctxrr_app.o linker/app-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/ctxrr_app.elf build/system/ctxrr_app.o
	$(OBJCOPY) -O binary build/system/ctxrr_app.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/ctxfault_app.o: system/ctxfault_app.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

build/system/CTXFLT.APP: build/system/ctxfault_app.o linker/app-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/ctxfault_app.elf build/system/ctxfault_app.o
	$(OBJCOPY) -O binary build/system/ctxfault_app.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/sleepctx_app.o: system/sleepctx_app.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

build/system/SLPCTX.APP: build/system/sleepctx_app.o linker/app-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/sleepctx_app.elf build/system/sleepctx_app.o
	$(OBJCOPY) -O binary build/system/sleepctx_app.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/ipctest_app.o: system/ipctest_app.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

build/system/IPCTEST.APP: build/system/ipctest_app.o linker/app-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/ipctest_app.elf build/system/ipctest_app.o
	$(OBJCOPY) -O binary build/system/ipctest_app.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/svctest_app.o: system/svctest_app.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

build/system/SVCTEST.APP: build/system/svctest_app.o linker/app-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/svctest_app.elf build/system/svctest_app.o
	$(OBJCOPY) -O binary build/system/svctest_app.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/init_service_app.o: system/init_service_app.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

build/system/INIT.APP: build/system/init_service_app.o linker/app-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/init_service_app.elf build/system/init_service_app.o
	$(OBJCOPY) -O binary build/system/init_service_app.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/gfx_service_app.o: system/gfx_service_app.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

build/system/path_app.o: system/path_app.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

build/system/PATH.APP: build/system/path_app.o linker/app-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/path_app.elf build/system/path_app.o
	$(OBJCOPY) -O binary build/system/path_app.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/GFX.APP: build/system/gfx_service_app.o linker/app-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/gfx_service_app.elf build/system/gfx_service_app.o
	$(OBJCOPY) -O binary build/system/gfx_service_app.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/gfxtest_app.o: system/gfxtest_app.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

build/system/GFXTEST.APP: build/system/gfxtest_app.o linker/app-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/gfxtest_app.elf build/system/gfxtest_app.o
	$(OBJCOPY) -O binary build/system/gfxtest_app.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/surftest_app.o: system/surftest_app.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

build/system/SURFTEST.APP: build/system/surftest_app.o linker/app-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/surftest_app.elf build/system/surftest_app.o
	$(OBJCOPY) -O binary build/system/surftest_app.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/gui_service_app.o: system/gui_service_app.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

build/system/GUI.APP: build/system/gui_service_app.o linker/app-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/gui_service_app.elf build/system/gui_service_app.o
	$(OBJCOPY) -O binary build/system/gui_service_app.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/guitest_app.o: system/guitest_app.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

build/system/GUITEST.APP: build/system/guitest_app.o linker/app-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/guitest_app.elf build/system/guitest_app.o
	$(OBJCOPY) -O binary build/system/guitest_app.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/guikey_app.o: system/guikey_app.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

build/system/GUIKEY.APP: build/system/guikey_app.o linker/app-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/guikey_app.elf build/system/guikey_app.o
	$(OBJCOPY) -O binary build/system/guikey_app.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/guistream_app.o: system/guistream_app.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

build/system/GUISTRM.APP: build/system/guistream_app.o linker/app-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/guistream_app.elf build/system/guistream_app.o
	$(OBJCOPY) -O binary build/system/guistream_app.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/guimod_app.o: system/guimod_app.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

build/system/GUIMOD.APP: build/system/guimod_app.o linker/app-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/guimod_app.elf build/system/guimod_app.o
	$(OBJCOPY) -O binary build/system/guimod_app.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/guimouse_app.o: system/guimouse_app.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

build/system/GUIMOUSE.APP: build/system/guimouse_app.o linker/app-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/guimouse_app.elf build/system/guimouse_app.o
	$(OBJCOPY) -O binary build/system/guimouse_app.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/desktop_app.o: system/desktop_app.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

# DESKTOP.APP is the batch-based desktop: it draws through opcode 18 so the
# kernel compositor (CONFIG_WM=1) can place it in a window surface.  The old
# direct-primitive assembly desktop remains as DESKTEST.APP (its mouse selftest).
build/system/desktop_wm.o: system/desktop_wm.c include/gui_app.h include/gui_draw.h system/icons_ids.h include/system_report.h
	$(CC) $(GUI_APP_CFLAGS) -MMD -MP -c $< -o $@
build/system/DESKTOP.APP: build/system/gui_app_runtime.o build/system/desktop_wm.o linker/app-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/desktop_wm.elf $(filter %.o,$^) -lgcc
	$(OBJCOPY) -O binary build/system/desktop_wm.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/pidwait_app.o: system/pidwait_app.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

build/system/PIDWAIT.APP: build/system/pidwait_app.o linker/app-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/pidwait_app.elf build/system/pidwait_app.o
	$(OBJCOPY) -O binary build/system/pidwait_app.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/wpid_app.o: system/wpid_app.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

build/system/WPID.APP: build/system/wpid_app.o linker/app-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/wpid_app.elf build/system/wpid_app.o
	$(OBJCOPY) -O binary build/system/wpid_app.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/rspawn_app.o: system/rspawn_app.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

build/system/RSPAWN.APP: build/system/rspawn_app.o linker/app-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/rspawn_app.elf build/system/rspawn_app.o
	$(OBJCOPY) -O binary build/system/rspawn_app.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/orphan_app.o: system/orphan_app.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

build/system/ORPHAN.APP: build/system/orphan_app.o linker/app-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/orphan_app.elf build/system/orphan_app.o
	$(OBJCOPY) -O binary build/system/orphan_app.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/multi_app.o: system/multi_app.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

build/system/MULTI.APP: build/system/multi_app.o linker/app-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/multi_app.elf build/system/multi_app.o
	$(OBJCOPY) -O binary build/system/multi_app.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/bgyield_app.o: system/bgyield_app.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

build/system/BGYIELD.APP: build/system/bgyield_app.o linker/app-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/bgyield_app.elf build/system/bgyield_app.o
	$(OBJCOPY) -O binary build/system/bgyield_app.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/waitkey_app.o: system/waitkey_app.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

build/system/WAITKEY.APP: build/system/waitkey_app.o linker/app-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/waitkey_app.elf build/system/waitkey_app.o
	$(OBJCOPY) -O binary build/system/waitkey_app.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/waitline_app.o: system/waitline_app.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

build/system/WAITLINE.APP: build/system/waitline_app.o linker/app-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/waitline_app.elf build/system/waitline_app.o
	$(OBJCOPY) -O binary build/system/waitline_app.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/procstress_app.o: system/procstress_app.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

build/system/PROCSTR.APP: build/system/procstress_app.o linker/app-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/procstress_app.elf build/system/procstress_app.o
	$(OBJCOPY) -O binary build/system/procstress_app.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/stackfault_app.o: system/stackfault_app.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

build/system/STKFAULT.APP: build/system/stackfault_app.o linker/app-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/stackfault_app.elf build/system/stackfault_app.o
	$(OBJCOPY) -O binary build/system/stackfault_app.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/pointertest_app.o: system/pointertest_app.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

build/system/PTRTEST.APP: build/system/pointertest_app.o linker/app-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/pointertest_app.elf build/system/pointertest_app.o
	$(OBJCOPY) -O binary build/system/pointertest_app.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/BADPOL.APP: build/system/PTRTEST.APP tools/make_bad_policy_app.py
	python3 tools/make_bad_policy_app.py $< $@

build/system/heapguard_app.o: system/heapguard_app.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

build/system/HGUARD.APP: build/system/heapguard_app.o linker/app-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/heapguard_app.elf build/system/heapguard_app.o
	$(OBJCOPY) -O binary build/system/heapguard_app.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/busfault_app.o: system/busfault_app.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

build/system/BUSFLT.APP: build/system/busfault_app.o linker/app-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/busfault_app.elf build/system/busfault_app.o
	$(OBJCOPY) -O binary build/system/busfault_app.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/addressfault_app.o: system/addressfault_app.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

build/system/ADRFLT.APP: build/system/addressfault_app.o linker/app-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/addressfault_app.elf build/system/addressfault_app.o
	$(OBJCOPY) -O binary build/system/addressfault_app.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/divfault_app.o: system/divfault_app.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

build/system/DIVFLT.APP: build/system/divfault_app.o linker/app-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/divfault_app.elf build/system/divfault_app.o
	$(OBJCOPY) -O binary build/system/divfault_app.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/chkfault_app.o: system/chkfault_app.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

build/system/CHKFLT.APP: build/system/chkfault_app.o linker/app-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/chkfault_app.elf build/system/chkfault_app.o
	$(OBJCOPY) -O binary build/system/chkfault_app.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/trapvfault_app.o: system/trapvfault_app.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

build/system/TRAPVFLT.APP: build/system/trapvfault_app.o linker/app-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/trapvfault_app.elf build/system/trapvfault_app.o
	$(OBJCOPY) -O binary build/system/trapvfault_app.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/privfault_app.o: system/privfault_app.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

build/system/PRIVFLT.APP: build/system/privfault_app.o linker/app-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/privfault_app.elf build/system/privfault_app.o
	$(OBJCOPY) -O binary build/system/privfault_app.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/linea_app.o: system/linea_app.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

build/system/LINEA.APP: build/system/linea_app.o linker/app-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/linea_app.elf build/system/linea_app.o
	$(OBJCOPY) -O binary build/system/linea_app.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/linef_app.o: system/linef_app.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

build/system/LINEF.APP: build/system/linef_app.o linker/app-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/linef_app.elf build/system/linef_app.o
	$(OBJCOPY) -O binary build/system/linef_app.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/trap1fault_app.o: system/trap1fault_app.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

build/system/TRAP1.APP: build/system/trap1fault_app.o linker/app-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/trap1fault_app.elf build/system/trap1fault_app.o
	$(OBJCOPY) -O binary build/system/trap1fault_app.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/trap15fault_app.o: system/trap15fault_app.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

build/system/TRAP15.APP: build/system/trap15fault_app.o linker/app-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/trap15fault_app.elf build/system/trap15fault_app.o
	$(OBJCOPY) -O binary build/system/trap15fault_app.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/exceptionwait_app.o: system/exceptionwait_app.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

build/system/EXWAIT.APP: build/system/exceptionwait_app.o linker/app-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/exceptionwait_app.elf build/system/exceptionwait_app.o
	$(OBJCOPY) -O binary build/system/exceptionwait_app.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/hogtest_app.o: system/hogtest_app.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

build/system/HOGTEST.APP: build/system/hogtest_app.o linker/app-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/hogtest_app.elf build/system/hogtest_app.o
	$(OBJCOPY) -O binary build/system/hogtest_app.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/hang_app.o: system/hang_app.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

build/system/HANG.APP: build/system/hang_app.o linker/app-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/hang_app.elf build/system/hang_app.o
	$(OBJCOPY) -O binary build/system/hang_app.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/quota_app.o: system/quota_app.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

build/system/QUOTA.APP: build/system/quota_app.o linker/app-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/quota_app.elf build/system/quota_app.o
	$(OBJCOPY) -O binary build/system/quota_app.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/holdmem_app.o: system/holdmem_app.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

build/system/HOLDMEM.APP: build/system/holdmem_app.o linker/app-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/holdmem_app.elf build/system/holdmem_app.o
	$(OBJCOPY) -O binary build/system/holdmem_app.elf $@
	python3 tools/patch_boot_checksum.py $@

build/bios_mste.elf: $(OBJ) linker/mste-rom.ld
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJ) -lgcc
	$(SIZE) $@

build/bios_mste.bin: build/bios_mste.elf tools/patch_rom_checksum.py
	$(OBJCOPY) -O binary $< $@
	python3 tools/padrom.py $@ 262144
	python3 tools/patch_rom_checksum.py $@

build/boot/reset_st.o: boot/reset.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -DROM_ST=1 -c $< -o $@

build/bios_st.elf: $(filter-out build/boot/reset.o,$(OBJ)) build/boot/reset_st.o linker/st-rom.ld
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/st-rom.ld \
		-Wl,-Map,build/bios_st.map -Wl,--gc-sections -o $@ \
		build/boot/reset_st.o $(filter-out build/boot/reset.o,$(OBJ)) -lgcc
	$(SIZE) $@

build/bios_st.bin: build/bios_st.elf tools/patch_rom_checksum.py
	$(OBJCOPY) -O binary $< $@
	python3 tools/padrom.py $@ 196608
	python3 tools/patch_rom_checksum.py $@

# TF536 ST Edition EPROM: two complete ROM images in one part, bank1 = ST layout
# (0xfc0000, 192 KiB) and bank2 = MSTE/TT layout (0xe00000, 256 KiB). The card's
# jumper picks the bank, so either firmware can be tried without re-flashing.
build/atos-bios-tf536-st-dual.bin: build/bios_st.bin build/bios_mste.bin tools/mk_dualrom.py
	python3 tools/mk_dualrom.py --bank-size 262144 $@ build/bios_st.bin build/bios_mste.bin

build/bios_mste_hi.bin build/bios_mste_lo.bin &: build/bios_mste.bin tools/splitrom.py
	python3 tools/splitrom.py split $< build/bios_mste_hi.bin build/bios_mste_lo.bin

build/bios_st_hi.bin build/bios_st_lo.bin &: build/bios_st.bin tools/splitrom.py
	python3 tools/splitrom.py split $< build/bios_st_hi.bin build/bios_st_lo.bin

build/checksums.txt: build/bios_mste.bin build/bios_mste_hi.bin build/bios_mste_lo.bin \
	build/bios_st.bin build/bios_st_hi.bin build/bios_st_lo.bin tools/checksum.py
	python3 tools/checksum.py $^ > $@

build/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -MMD -MP -ffunction-sections -fdata-sections -c $< -o $@

build/%.o: %.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

# Text/session/storage host tests.  They used to exist as standalone targets but
# were never run by "check", so regressions (session menu, TYPE, FAT writes)
# slipped through.  Keep this list limited to fast deterministic host tests.
check: check-settings check-arkanoid-render check-gui-frame check-hires-clock check-game check-st-raster check-tos-ext check-machine check-lang check-config check-exception check-irq check-video-driver check-video-backends check-video check-cpu-ops check-desktop-module check-mod-clock check-app-abi check-sessions check-terminal check-console check-mono-font check-gui-damage check-trash check-fat-write check-block-cache check-graphics check-wm-core check-wm-blit check-gui-apps check-psg check-audio check-mod
	python3 -m unittest discover -s tests -v

# Audio core: capability word, single owner, time-driven effect queue and the
# guarantee that releasing the owner silences every channel.
.PHONY: check-audio
check-audio:
	@mkdir -p build/tests
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -DPSG_HOST_TEST -Iinclude tests/test_kernel_audio.c system/kernel_audio.c system/irq.c hw/psg.c -o build/tests/test_kernel_audio
	build/tests/test_kernel_audio

# PSG access rules: which registers audio may write, and that port A (shared with
# the floppy drive/side select) is only changed by read-modify-write.
.PHONY: check-psg
check-psg:
	@mkdir -p build/tests
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -DPSG_HOST_TEST -Iinclude tests/test_psg.c hw/psg.c -o build/tests/test_psg
	build/tests/test_psg

# MOD -> PSG conversion: note/period mapping against the PSG range, MOD volume
# semantics (Cxx set, Axy slide) and channel stealing with four voices.
.PHONY: check-mod
check-mod:
	@mkdir -p build/tests
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -Iinclude tests/test_mod_psg.c system/mod_psg.c -o build/tests/test_mod_psg
	build/tests/test_mod_psg

# MOD player state machine: speed/tempo handling, row advance without drift,
# pattern break, position jump and end of song.
.PHONY: check-modplay
check-modplay:
	@mkdir -p build/tests
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -Iinclude tests/test_mod_play.c system/mod_play.c system/mod_psg.c -o build/tests/test_mod_play
	build/tests/test_mod_play

.PHONY: check-video-backends
check-video-backends:
	@mkdir -p build/tests
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -Iinclude tests/test_video_backends.c system/video_driver.c video/shifter.c video/et4000.c -o build/tests/test_video_backends
	build/tests/test_video_backends

.PHONY: check-mod-clock
check-mod-clock:
	@mkdir -p build/tests
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -Iinclude tests/test_mod_clock.c system/mod_clock.c system/video_driver.c -o build/tests/test_mod_clock
	build/tests/test_mod_clock

.PHONY: check-game
check-game:
	@mkdir -p build/tests
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -Iinclude tests/test_kernel_game.c -o build/tests/test_kernel_game
	build/tests/test_kernel_game

.PHONY: check-app-abi
check-app-abi:
	@mkdir -p build/tests
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -Iinclude tests/test_app_abi.c -o build/tests/test_app_abi
	build/tests/test_app_abi

.PHONY: check-desktop-module
check-desktop-module:
	@mkdir -p build/tests
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -Iinclude tests/test_desktop_module.c system/desktop_module.c -o build/tests/test_desktop_module
	build/tests/test_desktop_module

.PHONY: check-cpu-ops
check-cpu-ops:
	@mkdir -p build/tests
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -Iinclude tests/test_cpu_ops.c system/cpu_ops.c -o build/tests/test_cpu_ops
	build/tests/test_cpu_ops

.PHONY: check-video-driver
check-video-driver:
	@mkdir -p build/tests
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -Iinclude tests/test_video_driver.c system/video_driver.c -o build/tests/test_video_driver
	build/tests/test_video_driver

.PHONY: check-irq
check-irq:
	@mkdir -p build/tests
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -Iinclude tests/test_irq.c system/irq.c -o build/tests/test_irq
	build/tests/test_irq

.PHONY: check-exception
check-exception:
	@mkdir -p build/tests
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -Iinclude tests/test_exception.c -o build/tests/test_exception
	build/tests/test_exception

.PHONY: check-config
check-config:
	@mkdir -p build/tests
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -Iinclude tests/test_config.c -o build/tests/test_config
	build/tests/test_config

.PHONY: check-lang
check-lang:
	@mkdir -p build/tests
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -Iinclude tests/test_lang.c ui/lang.c -o build/tests/test_lang
	build/tests/test_lang

.PHONY: check-machine
check-machine:
	@mkdir -p build/tests
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -Iinclude tests/test_machine_detect.c -o build/tests/test_machine_detect
	build/tests/test_machine_detect

toolchain:
	@command -v $(CC) >/dev/null || { echo "Missing $(CC). See README.md"; exit 1; }

hatari: all
	hatari --machine megaste --monitor rgb --cpulevel 0 --cpuclock 16 --memsize 4 --tos build/bios_mste.bin --patch-tos false

hatari-rgb: hatari

hatari-st: all
	hatari --machine ste --monitor rgb --cpulevel 0 --cpuclock 8 --memsize 4 --tos build/bios_st.bin --patch-tos false

hatari-mono: all
	hatari --machine megaste --monitor mono --cpulevel 0 --cpuclock 16 --memsize 4 --tos build/bios_mste.bin --patch-tos false

hatari-medium: all
	hatari --machine megaste --monitor rgb --tos-res medium --cpulevel 0 --cpuclock 16 --memsize 4 --tos build/bios_mste.bin --patch-tos false

# Clean ATOS v10 boot image: base volume + v10 BOOT.SYS/KERNEL.SYS.
.PHONY: image-v10 hatari-v10 image-v10-emutos hatari-v10-emutos
EMUTOS_PRG ?= $(HOME)/Downloads/emutos-prg256-1.4/emu256pl.prg

# ATOS v10 + EmuTOS RAM-TOS demo (auto_tos=1).
image-v10-emutos: image-v10-clean
	cp work/atos-v10-clean.img work/atos-v10-emutos.img
	python3 tools/install_boot_sys.py work/atos-v10-emutos.img "$(EMUTOS_PRG)" --data
	python3 tools/make_bios_cfg.py /tmp/BIOS.CFG --auto-tos 1 --boot-device 1
	python3 tools/install_boot_sys.py work/atos-v10-emutos.img /tmp/BIOS.CFG --root --data

hatari-v10-emutos: image-v10-emutos
	./run-v10-emutos.sh
image-v10: all
	cp -c work/work.img work/atos-v10.img 2>/dev/null || cp work/work.img work/atos-v10.img
	python3 tools/install_boot_sys.py work/atos-v10.img build/system/BOOT.SYS
	python3 tools/install_boot_sys.py work/atos-v10.img build/system/KERNEL.SYS

hatari-v10: image-v10-clean
	./run-v10.sh

# Clean image: fresh AHDI/FAT16 with ONLY v10 system files (no TEST/SysInfo).
.PHONY: image-v10-clean
image-v10-clean: all
	rm -f work/atos-v10-clean.img
	python3 tools/create_ahdi_fat16_image.py work/atos-v10-clean.img
	python3 tools/install_boot_sys.py work/atos-v10-clean.img build/system/BOOT.SYS
	python3 tools/install_boot_sys.py work/atos-v10-clean.img build/system/KERNEL.SYS
	for m in ACSI CORE VFS PROC TIMER EVENT INPUT SHELL; do python3 tools/install_boot_sys.py work/atos-v10-clean.img build/system/$$m.SYS; done
	python3 tools/install_boot_sys.py work/atos-v10-clean.img build/system/ICONS.DAT
	for a in INIT GFX GUI; do python3 tools/install_boot_sys.py work/atos-v10-clean.img build/system/$$a.APP; done


# --- Ikony pulpitu jako dane w TT-RAM (system/icons_abi.h, include/icons_blob.h) ---
# Generator pisze od razu gotowy plik (format v5): nie ma juzposredniego
# C-naglowka z megabajtami tablic (tools/make_icons_blob.c jest historia).
build/system/ICONS.DAT: tools/mk_icons_color.py tools/icons_rle.py system/icons_ids.h
	python3 tools/mk_icons_color.py

build/system/FIXDISK.APP: build/system/gui_app_runtime.o build/system/fixdisk.o linker/app-sys.ld
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/fixdisk.elf $(filter %.o,$^) -lgcc
	$(OBJCOPY) -O binary build/system/fixdisk.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/fixdisk.o: system/fixdisk.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

clean:
	rm -rf build

# Host-side differential coverage; the boot test also exercises the 68000 path.
HOSTCC ?= cc
.PHONY: check-graphics
# Standard ST screen raster: interleaved-word contract.  Runs the pure decode
# under ASan so the old "planes as separate blocks" OOB cannot come back.
check-kgfx-mono:
	@mkdir -p build/tests
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -Iinclude tests/test_kgfx_mono.c -o build/tests/test_kgfx_mono
	build/tests/test_kgfx_mono

check-st-raster:
	@mkdir -p build/tests
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -Iinclude tests/test_st_raster.c -o build/tests/test_st_raster
	build/tests/test_st_raster

# The shared TOS-program extension matcher used by every launch point
# (desktop shortcut, file manager, shell).  Case-insensitive contract.
check-tos-ext:
	@mkdir -p build/tests
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -Iinclude tests/test_tos_ext.c -o build/tests/test_tos_ext
	build/tests/test_tos_ext

check-graphics:
	@mkdir -p build/tests
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -Iinclude tests/test_emutos_planar.c system/emutos_vdi_mono.c -o build/tests/test_emutos_planar
	build/tests/test_emutos_planar
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -Iinclude tests/test_emutos_copy.c system/emutos_vdi_mono.c -o build/tests/test_emutos_copy
	build/tests/test_emutos_copy
	# Tapeta CUSTOM (wzor 4): parser WALL= -> jadro (zakres 0..4) -> kafelek.
	python3 tests/test_wallpaper_custom.py
	# Ikony 32/48/64: format ICONS.DAT v5 + dekodery wyciete z jadra.
	python3 tests/test_icons_dat_v5.py
	python3 tests/test_icons_decoders.py

# Host microbenchmark (not a pass/fail test): relative cost of the mono copy
# routines.  Run manually with 'gmake bench-emutos-copy'.
.PHONY: bench-emutos-copy
bench-emutos-copy:
	@mkdir -p build/tests
	$(HOSTCC) -O2 -std=c11 -Wall -Wextra -Iinclude tests/bench_emutos_copy.c system/emutos_vdi_mono.c -o build/tests/bench_emutos_copy
	build/tests/bench_emutos_copy

# Emulator regression executable; not part of the interactive desktop build.
build/system/desktop_selftest.o: system/desktop_app.S tests/desktop_mouse_selftest.inc
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -DDESKTOP_SELFTEST -c $< -o $@

build/system/DESKTEST.APP: build/system/desktop_selftest.o linker/app-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/desktop_selftest.elf build/system/desktop_selftest.o
	$(OBJCOPY) -O binary build/system/desktop_selftest.elf $@
	python3 tools/patch_boot_checksum.py $@

# Position-independent native GUI processes; no TOS/MiNT runtime linked.
# The loader allocates each app dynamically and does not relocate absolute
# addresses. Keep PC-relative code; put large desktop buffers on the heap
# when references exceed the signed 16-bit displacement range.
GUI_APP_CFLAGS = $(CFLAGS) -mpcrel -ffunction-sections -fdata-sections -fno-jump-tables
# Position-independent integer helpers: the app image is loaded at a runtime
# address, and stock libgcc is not built with -mpcrel, so its division helpers
# jump to absolute low addresses.  See system/app_math.c.
build/system/app_math.o: system/app_math.c
	@mkdir -p $(@D)
	$(CC) $(GUI_APP_CFLAGS) -fno-tree-loop-distribute-patterns -MMD -MP -c $< -o $@

# Every runtime-loaded application must relink when the math helpers change:
# they replace libgcc's absolute-address versions in the app image.
build/system/SYSINFO.APP build/system/HW.APP build/system/DESKTOP.APP \
build/system/MONODESK.APP build/system/COUNT.APP build/system/NOTE.APP \
build/system/COMMAND.APP build/system/FATWRITE.APP build/system/COMTEST.APP \
build/system/CACHETST.APP build/system/TRASHTST.APP build/system/WINDEMO.APP \
build/system/WMDEMO.APP build/system/GFXBENCH.APP build/system/TERMTST.APP \
build/system/AC.APP build/system/SETUP.APP build/system/TASKMGR.APP build/system/PLASMA.APP: build/system/app_math.o

build/system/gui_app_runtime.o: system/gui_app_runtime.S
	$(CC) $(ASFLAGS) -c $< -o $@
build/system/mono_desktop.o: system/mono_desktop.c include/gui_app.h include/gui_draw.h include/mono_wm.h
	$(CC) $(GUI_APP_CFLAGS) -c $< -o $@
build/system/mono_wm.o: system/mono_wm.c include/mono_wm.h
	$(CC) $(GUI_APP_CFLAGS) -c $< -o $@
build/system/mono_count.o: system/mono_client.c include/gui_app.h
	$(CC) $(GUI_APP_CFLAGS) -DAPP_KIND=1 -c $< -o $@
build/system/mono_note.o: system/mono_client.c include/gui_app.h
	$(CC) $(GUI_APP_CFLAGS) -DAPP_KIND=2 -c $< -o $@
build/system/MONODESK.APP: build/system/gui_app_runtime.o build/system/mono_desktop.o build/system/mono_wm.o linker/app-sys.ld
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/monodesk.elf $(filter %.o,$^) -lgcc
	$(OBJCOPY) -O binary build/system/monodesk.elf $@
	python3 tools/patch_boot_checksum.py $@
build/system/COUNT.APP: build/system/gui_client_runtime.o build/system/mono_count.o linker/app-sys.ld
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/mono_count.elf $(filter %.o,$^) -lgcc
	$(OBJCOPY) -O binary build/system/mono_count.elf $@
	python3 tools/patch_boot_checksum.py $@
build/system/NOTE.APP: build/system/gui_client_runtime.o build/system/mono_note.o linker/app-sys.ld
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/mono_note.elf $(filter %.o,$^) -lgcc
	$(OBJCOPY) -O binary build/system/mono_note.elf $@
	python3 tools/patch_boot_checksum.py $@
all: build/system/MONODESK.APP build/system/COUNT.APP build/system/NOTE.APP

.PHONY: check-wm
check-wm:
	@mkdir -p build/tests
	cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -Iinclude tests/test_mono_wm.c system/mono_wm.c -o build/tests/test_mono_wm
	build/tests/test_mono_wm

.PHONY: check-wm-core
check-wm-core:
	@mkdir -p build/tests
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -Iinclude tests/test_wm_core.c system/wm_core.c -o build/tests/test_wm_core
	build/tests/test_wm_core

.PHONY: check-wm-blit
check-wm-blit:
	@mkdir -p build/tests
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -Iinclude tests/test_wm_blit.c system/wm_blit.c -o build/tests/test_wm_blit
	build/tests/test_wm_blit

.PHONY: check-gui-apps
# Every runtime-loaded application must stay position independent: the loader
# puts the image at an arbitrary heap address and never relocates it, so an
# absolute call or data reference lands in low RAM.  The gate existed but was
# not part of `check` and nothing depended on it, which is how the -mno-pcrel
# regression shipped.  It now runs on every build and in `make check`.
check-gui-apps: build/system/SYSINFO.APP build/system/MONODESK.APP build/system/COUNT.APP \
	build/system/NOTE.APP build/system/COMMAND.APP build/system/FATWRITE.APP \
	build/system/COMTEST.APP build/system/CACHETST.APP build/system/TRASHTST.APP \
	build/system/TERMTST.APP
	python3 tools/check_gui_apps.py

all: check-gui-apps

build/system/gui_client_runtime.o: system/gui_app_runtime.S
	$(CC) $(ASFLAGS) -DAPP_STACK=2048 -c $< -o $@

.PHONY: check-fat-write
check-fat-write:
	@mkdir -p build/tests
	cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -Iinclude tests/test_fat_write.c system/kernel_fat.c -o build/tests/test_fat_write
	build/tests/test_fat_write

build/system/commander.o: system/commander.c include/gui_app.h include/gui_draw.h include/kernel_fat.h
	$(CC) $(GUI_APP_CFLAGS) -c $< -o $@
build/system/COMMAND.APP: build/system/gui_app_runtime.o build/system/commander.o build/system/text_terminal_app.o linker/app-sys.ld
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/commander.elf $(filter %.o,$^) -lgcc
	$(OBJCOPY) -O binary build/system/commander.elf $@
	python3 tools/patch_boot_checksum.py $@
all check-gui-apps: build/system/COMMAND.APP

.PHONY: check-mono-font
check-mono-font:
	cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -Iinclude tests/test_mono_font.c system/emutos_vdi_mono.c -o build/tests/test_mono_font
	build/tests/test_mono_font

build/system/fat_write_test.o: system/fat_write_test.c include/gui_app.h include/kernel_fat.h
	$(CC) $(GUI_APP_CFLAGS) -c $< -o $@
build/system/FATWRITE.APP: build/system/gui_app_runtime.o build/system/fat_write_test.o linker/app-sys.ld
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/fat_write_test.elf $(filter %.o,$^) -lgcc
	$(OBJCOPY) -O binary build/system/fat_write_test.elf $@
	python3 tools/patch_boot_checksum.py $@
all check-gui-apps: build/system/FATWRITE.APP

.PHONY: check-commander
check-commander:
	@mkdir -p build/tests
	cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -Iinclude tests/test_commander.c system/text_terminal.c -o build/tests/test_commander
	build/tests/test_commander

build/system/commander_test.o: system/commander.c tests/commander_selftest.inc include/gui_app.h include/gui_draw.h
	$(CC) $(GUI_APP_CFLAGS) -DCOMMANDER_SELFTEST -c $< -o $@
build/system/COMTEST.APP: build/system/gui_app_runtime.o build/system/commander_test.o build/system/text_terminal_app.o linker/app-sys.ld
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/commander_test.elf $(filter %.o,$^) -lgcc
	$(OBJCOPY) -O binary build/system/commander_test.elf $@
	python3 tools/patch_boot_checksum.py $@
all check-gui-apps: build/system/COMTEST.APP
build/system/commander.o: tests/commander_selftest.inc

.PHONY: check-block-cache
check-block-cache:
	@mkdir -p build/tests
	cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -DKBLOCK_TEST -Iinclude tests/test_block_cache.c system/kernel_block.c -o build/tests/test_block_cache
	build/tests/test_block_cache
	build/tests/test_block_cache low-memory
	cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -DKBLOCK_TEST -DFAT_TEST_CACHE -Iinclude tests/test_fat_write.c system/kernel_fat.c system/kernel_block.c -o build/tests/test_fat_cached
	build/tests/test_fat_cached

build/system/cache_test.o: system/cache_test.c include/gui_app.h
	$(CC) $(GUI_APP_CFLAGS) -c $< -o $@
build/system/CACHETST.APP: build/system/gui_app_runtime.o build/system/cache_test.o linker/app-sys.ld
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/cache_test.elf $(filter %.o,$^) -lgcc
	$(OBJCOPY) -O binary build/system/cache_test.elf $@
	python3 tools/patch_boot_checksum.py $@
all check-gui-apps: build/system/CACHETST.APP

.PHONY: bench-directory-cache
bench-directory-cache:
	@mkdir -p build/tests
	cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -DKBLOCK_TEST -Iinclude tests/bench_directory_cache.c system/kernel_fat.c system/kernel_block.c -o build/tests/bench_cache
	cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -Iinclude tests/bench_directory_cache.c system/kernel_fat.c tests/legacy_block_read.c -o build/tests/bench_legacy
	build/tests/bench_legacy "$(IMAGE)"
	build/tests/bench_cache "$(IMAGE)"

.PHONY: check-trash
check-trash:
	@mkdir -p build/tests
	cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -DKBLOCK_TEST -DFAT_TEST_CACHE -Iinclude tests/test_trash.c system/kernel_fat.c system/kernel_block.c -o build/tests/test_trash
	build/tests/test_trash

.PHONY: check-gui-damage
check-gui-damage:
	@mkdir -p build/tests
	cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -Iinclude tests/test_gui_damage.c -o build/tests/test_gui_damage
	build/tests/test_gui_damage

build/system/trash_test.o: system/trash_test.c include/gui_app.h include/kernel_fat.h
	$(CC) $(GUI_APP_CFLAGS) -c $< -o $@
build/system/TRASHTST.APP: build/system/gui_app_runtime.o build/system/trash_test.o linker/app-sys.ld
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/trash_test.elf $(filter %.o,$^) -lgcc
	$(OBJCOPY) -O binary build/system/trash_test.elf $@
	python3 tools/patch_boot_checksum.py $@
all check-gui-apps: build/system/TRASHTST.APP

build/system/gfx_bench.o: system/gfx_bench.c include/gui_app.h include/gui_draw.h
	$(CC) $(GUI_APP_CFLAGS) -MMD -MP -c $< -o $@
build/system/wm_demo.o: system/wm_demo.c include/gui_app.h include/gui_draw.h
	$(CC) $(GUI_APP_CFLAGS) -MMD -MP -c $< -o $@
build/system/windemo.o: system/windemo.c include/gui_app.h include/gui_draw.h
	$(CC) $(GUI_APP_CFLAGS) -MMD -MP -c $< -o $@
build/system/WINDEMO.APP: build/system/gui_app_runtime.o build/system/windemo.o linker/app-sys.ld
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/windemo.elf $(filter %.o,$^) -lgcc
	$(OBJCOPY) -O binary build/system/windemo.elf $@
	python3 tools/patch_boot_checksum.py $@
all check-gui-apps: build/system/WINDEMO.APP

# Arkanoid (Codex): native GUI game, no TOS/SFML.  game.c is the fixed-point
# physics (MIT-derived); arkanoid.c is the ATOS GUI front-end.  Built against
# the same app ABI as the other GUI apps, but with a dedicated runtime that
# requests a 16 KB stack (the triple-buffered draw lists are static, the extra
# stack is a safety margin the original build used).
build/system/game.o: system/game.c system/game.h system/levels.h
	$(CC) $(GUI_APP_CFLAGS) -MMD -MP -c $< -o $@
build/system/arkanoid.o: system/arkanoid.c include/gui_app.h include/gui_draw.h system/game.h system/colours.h
	$(CC) $(GUI_APP_CFLAGS) -MMD -MP -c $< -o $@
build/system/app_memory.o: system/app_memory.c
	$(CC) $(GUI_APP_CFLAGS) -MMD -MP -c $< -o $@
build/system/arkanoid_runtime.o: system/gui_app_runtime.S
	$(CC) $(ASFLAGS) -DAPP_STACK=16384 -c $< -o $@
build/system/ARKANOID.APP: build/system/arkanoid_runtime.o build/system/arkanoid.o build/system/game.o build/system/app_math.o build/system/app_memory.o linker/app-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/arkanoid.elf $(filter %.o,$^) -lgcc
	$(OBJCOPY) -O binary build/system/arkanoid.elf $@
	python3 tools/patch_boot_checksum.py $@
all check-gui-apps: build/system/ARKANOID.APP

build/system/ac.o: system/ac.c include/gui_app.h include/gui_draw.h
	$(CC) $(GUI_APP_CFLAGS) -MMD -MP -c $< -o $@
build/system/AC.APP: build/system/gui_app_runtime.o build/system/ac.o linker/app-sys.ld
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/ac.elf $(filter %.o,$^) -lgcc
	$(OBJCOPY) -O binary build/system/ac.elf $@
	python3 tools/patch_boot_checksum.py $@
all check-gui-apps: build/system/AC.APP

build/system/WMDEMO.APP: build/system/gui_app_runtime.o build/system/wm_demo.o linker/app-sys.ld
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/wm_demo.elf $(filter %.o,$^) -lgcc
	$(OBJCOPY) -O binary build/system/wm_demo.elf $@
	python3 tools/patch_boot_checksum.py $@
all check-gui-apps: build/system/WMDEMO.APP

build/system/GFXBENCH.APP: build/system/gui_app_runtime.o build/system/gfx_bench.o linker/app-sys.ld
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/gfx_bench.elf $(filter %.o,$^) -lgcc
	$(OBJCOPY) -O binary build/system/gfx_bench.elf $@
	python3 tools/patch_boot_checksum.py $@
all check-gui-apps: build/system/GFXBENCH.APP

build/system/text_terminal_app.o: system/text_terminal.c include/text_terminal.h
	$(CC) $(GUI_APP_CFLAGS) -c $< -o $@
build/system/commander.o build/system/commander_test.o: include/text_terminal.h

.PHONY: check-terminal
check-terminal:
	@mkdir -p build/tests
	cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -Iinclude tests/test_text_terminal.c system/text_terminal.c -o build/tests/test_text_terminal
	build/tests/test_text_terminal

build/system/terminal_test.o: system/terminal_test.c include/gui_app.h include/text_terminal.h
	$(CC) $(GUI_APP_CFLAGS) -c $< -o $@
build/system/TERMTST.APP: build/system/gui_app_runtime.o build/system/terminal_test.o build/system/text_terminal_app.o linker/app-sys.ld
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/terminal_test.elf $(filter %.o,$^) -lgcc
	$(OBJCOPY) -O binary build/system/terminal_test.elf $@
	python3 tools/patch_boot_checksum.py $@
all check-gui-apps: build/system/TERMTST.APP


.PHONY: check-console
check-console:
	@mkdir -p build/tests
	cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -Iinclude tests/test_kernel_console.c system/text_terminal.c -o build/tests/test_kernel_console
	build/tests/test_kernel_console

.PHONY: check-sessions
check-sessions:
	@mkdir -p build/tests
	cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -Iinclude tests/test_sessions.c -o build/tests/test_sessions
	build/tests/test_sessions

build/system/kernel_video.o: system/kernel_video.c
	$(CC) $(CFLAGS) -MMD -MP -ffunction-sections -fdata-sections -c $< -o $@
build/system/kernel_video_boot.o: system/kernel_video_boot.c
	$(CC) $(CFLAGS) -MMD -MP -ffunction-sections -fdata-sections -c $< -o $@
build/hw/vofa_et4000_rom.o: system/vofa_et4000.c
	$(CC) $(CFLAGS) -MMD -MP -ffunction-sections -fdata-sections -c $< -o $@
build/system/vofa_et4000.o: system/vofa_et4000.c
	$(CC) $(CFLAGS) -mpcrel -fno-tree-loop-distribute-patterns -MMD -MP -ffunction-sections -fdata-sections -c $< -o $@
build/system/vofa_entry.o: system/vofa_entry.S
	$(CC) $(ASFLAGS) -c $< -o $@
build/system/vofa_module.o: system/vofa_module.c include/kernel_video_service.h include/kernel_module.h
	$(CC) $(CFLAGS) -mpcrel -fno-tree-loop-distribute-patterns -MMD -MP -ffunction-sections -fdata-sections -c $< -o $@
build/system/VOFA.SYS: build/system/vofa_entry.o build/system/vofa_module.o build/system/vofa_et4000.o linker/vofa-module-sys.ld tools/patch_boot_checksum.py
	$(CC) -m68000 -nostdlib -nostartfiles -Wl,-T,linker/vofa-module-sys.ld -Wl,--gc-sections -o build/system/vofa_module.elf build/system/vofa_entry.o build/system/vofa_module.o build/system/vofa_et4000.o -lgcc
	$(OBJCOPY) -O binary build/system/vofa_module.elf $@
	python3 tools/patch_boot_checksum.py $@
build/system/video_bus.o: system/video_bus.S
	$(CC) $(ASFLAGS) -c $< -o $@

.PHONY: check-video
check-video:
	@mkdir -p build/tests
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -Iinclude tests/test_wm_clamp.c -o build/tests/test_wm_clamp
	build/tests/test_wm_clamp
	@mkdir -p build/tests
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -Iinclude tests/test_kvideo_guard.c -o build/tests/test_kvideo_guard
	build/tests/test_kvideo_guard
	@mkdir -p build/tests
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -Iinclude tests/test_setup_hit.c -o build/tests/test_setup_hit
	build/tests/test_setup_hit
	@mkdir -p build/tests
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -Iinclude tests/test_planar4.c -o build/tests/test_planar4
	build/tests/test_planar4
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -Iinclude tests/test_present16.c -o build/tests/test_present16
	build/tests/test_present16
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -Iinclude tests/test_vofa.c -o build/tests/test_vofa
	build/tests/test_vofa
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -Iinclude tests/test_video_output.c -o build/tests/test_video_output
	build/tests/test_video_output
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -Iinclude tests/test_video_boot.c -o build/tests/test_video_boot
	build/tests/test_video_boot
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -Iinclude tests/test_video_strips.c -o build/tests/test_video_strips
	build/tests/test_video_strips

# Every runtime-loaded application must stay position independent: the loader
# puts the image at an arbitrary heap address and never relocates it.  This gate
# existed but had no recipe and was not part of `check`, so the -mno-pcrel
# regression shipped.  It now runs on every build and in `make check`.
.PHONY: check-sysinfo
check-sysinfo:
	@mkdir -p build/tests
	cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -Iinclude tests/test_sysinfo.c system/text_terminal.c -o build/tests/test_sysinfo
	build/tests/test_sysinfo

# --- GEM desktop app (our desktop on the EmuTOS layer) ---
build/system/desktop_gem.o: system/desktop_gem.c
	@mkdir -p $(@D)
	$(CC) -m68000 -Os -ffreestanding -fomit-frame-pointer -Wall -Wextra -std=gnu11 -Iinclude -c $< -o $@

build/system/tos_start.o: system/tos_start.S
	@mkdir -p $(@D)
	$(CC) -m68000 -ffreestanding -c $< -o $@

.PHONY: desktop-gem
desktop-gem: build/system/desktop_gem.o build/system/desktop_data.o build/system/tos_start.o linker/tos-prg.ld tools/mk_tosprg.py
	$(CC) -m68000 -nostdlib -nostartfiles -Wl,-T,linker/tos-prg.ld -Wl,--gc-sections -Wl,--emit-relocs -o build/system/desktop_gem.elf build/system/tos_start.o build/system/desktop_gem.o build/system/desktop_data.o
	$(OBJCOPY) -O binary build/system/desktop_gem.elf build/system/desktop_gem.bin
	python3 tools/mk_tosprg.py build/system/desktop_gem.elf build/system/DESKTOP.PRG

build/system/desktop_data.o: system/desktop_data.c
	@mkdir -p $(@D)
	$(CC) -m68000 -Os -ffreestanding -fomit-frame-pointer -Wall -Wextra -std=gnu11 -Iinclude -c $< -o $@

# Native runtime language packs (English recovery text is generated at build time).
build/system/commander.o build/system/commander_test.o build/system/sysinfo.o build/system/setup_app.o: include/app_lng.h include/lng_catalog.h
build/system/setup_app.o: system/setup_app.c include/text_terminal.h
	$(CC) $(GUI_APP_CFLAGS) -c $< -o $@
build/system/settings_app.o: system/settings_app.c include/gui_app.h include/settings_config.h include/atos_display.h
	$(CC) $(GUI_APP_CFLAGS) -MMD -MP -c $< -o $@
build/system/SETTINGS.APP: build/system/gui_app_runtime.o build/system/settings_app.o build/system/app_math.o linker/app-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/settings_app.elf $(filter %.o,$^) -lgcc
	$(OBJCOPY) -O binary build/system/settings_app.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/SETUP.APP: build/system/gui_app_runtime.o build/system/setup_app.o build/system/text_terminal_app.o linker/app-sys.ld
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/setup_app.elf $(filter %.o,$^) -lgcc
	$(OBJCOPY) -O binary build/system/setup_app.elf $@
	python3 tools/patch_boot_checksum.py $@
all: build/system/SETUP.APP

.PHONY: check-languages
check-languages:
	@mkdir -p build/tests
	@for group in CMD SYS SET; do \
	 cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -Iinclude -DLNG_$$group tests/test_languages.c -o build/tests/test_languages_$$group && build/tests/test_languages_$$group || exit 1; done
	cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -Iinclude tests/test_language_setting.c -o build/tests/test_language_setting
	build/tests/test_language_setting
	cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -Iinclude tests/test_setup.c system/text_terminal.c -o build/tests/test_setup
	build/tests/test_setup

.PHONY: check-terminal-chunky
check: check-terminal-chunky
check-terminal-chunky:
	@mkdir -p build/tests
	cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -Iinclude tests/test_terminal_chunky.c system/text_terminal.c -o build/tests/test_terminal_chunky
	build/tests/test_terminal_chunky

# First real .PRG test for the TOS personality: built like the GEM example, but
# self-contained so it can run before any GEM subsystem is up.
.PHONY: tosprobe
tosprobe: build/system/tosprobe.o build/system/tos_start.o linker/tos-prg.ld tools/mk_tosprg.py
	$(CC) -m68000 -nostdlib -nostartfiles -Wl,-T,linker/tos-prg.ld -Wl,--gc-sections -Wl,--emit-relocs -o build/system/tosprobe.elf build/system/tos_start.o build/system/tosprobe.o
	$(OBJCOPY) -O binary build/system/tosprobe.elf build/system/tosprobe.bin
	python3 tools/mk_tosprg.py build/system/tosprobe.elf build/system/TOSPROBE.PRG

build/system/tosprobe.o: system/tosprobe.c
	@mkdir -p $(@D)
	$(CC) -m68000 -Os -ffreestanding -fomit-frame-pointer -Wall -Wextra -Werror -std=gnu11 -c $< -o $@

# Audio XBIOS probe: exercises Giaccess/Offgibit/Ongibit/Dosound and proves the
# Dosound sequence advances during a busy loop that never calls the OS (the
# Timer C daemon must be the one playing it).
.PHONY: tosaudio
# NFTEST.APP — ATOS Sound po NatFeats (test integracyjny w gosciu).
# Uzywa tej samej biblioteki klienta co symulator; ASND-v1 jest jeden.
NF_SRC := ../ATOS-Sound/reference-v1
build/nf/%.o: $(NF_SRC)/src/%.c
	@mkdir -p $(@D)
	$(CC) $(GUI_APP_CFLAGS) -Iinclude -I$(NF_SRC)/include -idirafter ../PRE2/atos/include -c $< -o $@
build/system/nftest.o: system/nftest.c include/app_abi.h include/gui_app.h
	@mkdir -p $(@D)
	$(CC) $(GUI_APP_CFLAGS) -Iinclude -I$(NF_SRC)/include -idirafter ../PRE2/atos/include -c $< -o $@
.PHONY: nftest
nftest: build/system/nftest.o build/system/gui_app_runtime.o build/system/app_math.o linker/app-sys.ld tools/patch_boot_checksum.py \
        build/nf/wire.o build/nf/client.o build/nf/upload.o build/nf/guest_seq.o build/nf/natfeats_client.o
	$(CC) $(GUI_APP_CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections \
	  -o build/system/nftest.elf build/system/nftest.o build/system/gui_app_runtime.o build/system/app_math.o \
	  build/nf/wire.o build/nf/client.o build/nf/upload.o build/nf/guest_seq.o build/nf/natfeats_client.o -lgcc
	$(OBJCOPY) -O binary build/system/nftest.elf build/system/NFTEST.APP
	python3 tools/patch_boot_checksum.py build/system/NFTEST.APP
	@ls -l build/system/NFTEST.APP

atest: build/system/atest.o build/system/gui_app_runtime.o linker/app-sys.ld
	$(CC) $(GUI_APP_CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/atest.elf build/system/atest.o build/system/gui_app_runtime.o -lgcc
	$(OBJCOPY) -O binary build/system/atest.elf build/system/ATEST.APP
	python3 tools/patch_boot_checksum.py build/system/ATEST.APP

build/system/atest.o: system/atest.c include/gui_app.h include/app_abi.h
	$(CC) $(GUI_APP_CFLAGS) -MMD -MP -c $< -o $@

tosaudio: build/system/tos_audio.o build/system/tos_start.o linker/tos-prg.ld tools/mk_tosprg.py
	$(CC) -m68000 -nostdlib -nostartfiles -Wl,-T,linker/tos-prg.ld -Wl,--gc-sections -Wl,--emit-relocs -o build/system/tos_audio.elf build/system/tos_start.o build/system/tos_audio.o
	$(OBJCOPY) -O binary build/system/tos_audio.elf build/system/tos_audio.bin
	python3 tools/mk_tosprg.py build/system/tos_audio.elf build/system/TOSAUDIO.PRG

build/system/tos_audio.o: system/tos_audio.c
	@mkdir -p $(@D)
	$(CC) -m68000 -Os -ffreestanding -fomit-frame-pointer -Wall -Wextra -Werror -std=gnu11 -c $< -o $@

# T2 colour-client test: a TOS program that switches itself to 320x200x16 and
# paints bars, so the window's colour path can be verified from a screenshot.
# T2 VDI test: draws only through the GEM VDI (trap #2), so the VDI redirect to
# the program's own screen (and therefore into the window) can be verified.
.PHONY: tosvdi
tosvdi: build/system/tosvdi.o build/system/tos_start.o linker/tos-prg.ld tools/mk_tosprg.py
	$(CC) -m68000 -nostdlib -nostartfiles -Wl,-T,linker/tos-prg.ld -Wl,--gc-sections -Wl,--emit-relocs -o build/system/tosvdi.elf build/system/tos_start.o build/system/tosvdi.o
	$(OBJCOPY) -O binary build/system/tosvdi.elf build/system/tosvdi.bin
	python3 tools/mk_tosprg.py build/system/tosvdi.elf build/system/TOSVDI.PRG

build/system/tosvdi.o: system/tosvdi.c
	@mkdir -p $(@D)
	$(CC) -m68000 -Os -ffreestanding -fomit-frame-pointer -Wall -Wextra -Werror -std=gnu11 -c $< -o $@

.PHONY: toscolour
toscolour: build/system/toscolour.o build/system/tos_start.o linker/tos-prg.ld tools/mk_tosprg.py
	$(CC) -m68000 -nostdlib -nostartfiles -Wl,-T,linker/tos-prg.ld -Wl,--gc-sections -Wl,--emit-relocs -o build/system/toscolour.elf build/system/tos_start.o build/system/toscolour.o
	$(OBJCOPY) -O binary build/system/toscolour.elf build/system/toscolour.bin
	python3 tools/mk_tosprg.py build/system/toscolour.elf build/system/TOSCOL.PRG

build/system/toscolour.o: system/toscolour.c
	@mkdir -p $(@D)
	$(CC) -m68000 -Os -ffreestanding -fomit-frame-pointer -Wall -Wextra -Werror -std=gnu11 -c $< -o $@

# T2 mono-client test: a TOS program that switches itself to 640x400x1 (rez 2)
# and paints a checkerboard, so the fixed 640x400 mono window can be verified.
.PHONY: tosmono
tosmono: build/system/tosmono.o build/system/tos_start.o linker/tos-prg.ld tools/mk_tosprg.py
	$(CC) -m68000 -nostdlib -nostartfiles -Wl,-T,linker/tos-prg.ld -Wl,--gc-sections -Wl,--emit-relocs -o build/system/tosmono.elf build/system/tos_start.o build/system/tosmono.o
	$(OBJCOPY) -O binary build/system/tosmono.elf build/system/tosmono.bin
	python3 tools/mk_tosprg.py build/system/tosmono.elf build/system/TOSMONO.PRG

build/system/tosmono.o: system/tosmono.c
	@mkdir -p $(@D)
	$(CC) -m68000 -Os -ffreestanding -fomit-frame-pointer -Wall -Wextra -Werror -std=gnu11 -c $< -o $@

# T2 medium-res colour client test: 640x200x4 (rez 1), 2 bitplanes.
.PHONY: tosmed
tosmed: build/system/tosmed.o build/system/tos_start.o linker/tos-prg.ld tools/mk_tosprg.py
	$(CC) -m68000 -nostdlib -nostartfiles -Wl,-T,linker/tos-prg.ld -Wl,--gc-sections -Wl,--emit-relocs -o build/system/tosmed.elf build/system/tos_start.o build/system/tosmed.o
	$(OBJCOPY) -O binary build/system/tosmed.elf build/system/tosmed.bin
	python3 tools/mk_tosprg.py build/system/tosmed.elf build/system/TOSMED.PRG

build/system/tosmed.o: system/tosmed.c
	@mkdir -p $(@D)
	$(CC) -m68000 -Os -ffreestanding -fomit-frame-pointer -Wall -Wextra -Werror -std=gnu11 -c $< -o $@

# T2 demo: cycles rez 0 -> 1 -> 2 so all three client resolutions can be watched.
.PHONY: tosdemo
tosdemo: build/system/tosdemo.o build/system/tos_start.o linker/tos-prg.ld tools/mk_tosprg.py
	$(CC) -m68000 -nostdlib -nostartfiles -Wl,-T,linker/tos-prg.ld -Wl,--gc-sections -Wl,--emit-relocs -o build/system/tosdemo.elf build/system/tos_start.o build/system/tosdemo.o
	$(OBJCOPY) -O binary build/system/tosdemo.elf build/system/tosdemo.bin
	python3 tools/mk_tosprg.py build/system/tosdemo.elf build/system/TOSDEMO.PRG

build/system/tosdemo.o: system/tosdemo.c
	@mkdir -p $(@D)
	$(CC) -m68000 -Os -ffreestanding -fomit-frame-pointer -Wall -Wextra -Werror -std=gnu11 -c $< -o $@

.PHONY: check-tos-damage
check: check-tos-damage
check-tos-damage:
	@mkdir -p build/tests
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -Iinclude tests/test_tos_damage.c -o build/tests/test_tos_damage
	build/tests/test_tos_damage

# Flashback (REminiscence) port - apps/flashback/README_PL.md
.PHONY: flashback flashback-install
flashback:
	$(MAKE) -C apps/flashback CROSS=$(CROSS)
flashback-install:
	$(MAKE) -C apps/flashback CROSS=$(CROSS) install IMG=$(CURDIR)/work/atos-v2.img DATA="$(FLASHBACK_DATA)"

.PHONY: check-gui-frame check-hires-clock
check-gui-frame:
	python3 tests/test_gui_frame.py
check-hires-clock:
	python3 tests/test_hires_clock.py

.PHONY: check-arkanoid-render
check-arkanoid-render:
	@mkdir -p build/tests
	$(HOSTCC) -O2 -std=c11 -Wall -Wextra -Werror -Wno-unused-variable -Wno-unused-function -fsanitize=address,undefined -Iinclude tests/test_arkanoid_render.c system/game.c -o build/tests/test_arkanoid_render
	build/tests/test_arkanoid_render

.PHONY: modplay
modplay: build/system/MODPLAY.APP
build/system/kernel_pcm.o: system/kernel_pcm.c include/kernel_pcm.h system/pcm_lut.h
	$(CC) $(CFLAGS) -MMD -MP -ffunction-sections -fdata-sections -c $< -o $@
build/system/pcm_irq.o: system/pcm_irq.S
	$(CC) $(ASFLAGS) -c $< -o $@
build/system/modplay.o: system/modplay.c include/app_abi.h include/gui_app.h include/modplay_sound.h
	$(CC) $(GUI_APP_CFLAGS) -m68020 -Ithird_party/micromod -Ithird_party/atos_sound_client -MMD -MP -c $< -o $@
build/system/micromod.o: third_party/micromod/micromod.c
	$(CC) $(GUI_APP_CFLAGS) -m68020 -Ithird_party/micromod -Ithird_party/atos_sound_client -MMD -MP -c $< -o $@
MOD_SOUND_OBJS := build/modsound/modplay_sound.o build/modsound/wire.o build/modsound/client.o
build/modsound/modplay_sound.o: system/modplay_sound.c include/modplay_sound.h
	@mkdir -p $(@D)
	$(CC) $(GUI_APP_CFLAGS) -m68020 -Ithird_party/atos_sound_client -idirafter ../PRE2/atos/include -MMD -MP -c $< -o $@
build/modsound/%.o: third_party/atos_sound_client/%.c
	@mkdir -p $(@D)
	$(CC) $(GUI_APP_CFLAGS) -m68020 -DAS_FAST_CRC32 -Ithird_party/atos_sound_client -idirafter ../PRE2/atos/include -MMD -MP -c $< -o $@
-include $(MOD_SOUND_OBJS:.o=.d)
build/system/MODPLAY.APP: build/system/gui_app_runtime.o build/system/modplay.o build/system/micromod.o build/system/app_memory.o build/system/app_math.o $(MOD_SOUND_OBJS) linker/app-sys.ld
	$(CC) $(GUI_APP_CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/modplay_app.elf $(filter %.o,$^) -lgcc
	$(OBJCOPY) -O binary build/system/modplay_app.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/kernel_sound_nf.o: system/kernel_sound_nf.c include/kernel_sound_nf.h include/app_abi.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

build/system/kernel_sound_nf_probe.o: system/kernel_sound_nf_probe.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

.PHONY: check-pcm
check-pcm:
	@mkdir -p build/tests
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -DPSG_HOST_TEST -DKPCM_HOST_TEST -Iinclude tests/test_kernel_pcm.c system/kernel_pcm.c system/kernel_audio.c system/irq.c hw/psg.c -o build/tests/test_kernel_pcm
	build/tests/test_kernel_pcm
.PHONY: check-modplay-pcm
check-modplay-pcm:
	@mkdir -p build/tests
	$(HOSTCC) -std=c11 -O1 -Wall -Wextra -fsanitize=address,undefined -Iinclude -DMODPLAY_HOST_TEST -DAS_FAST_CRC32 -DAS_ENABLE_TRACKER -DLIBXMP_CORE_DISABLE_IT -DLIBXMP_STATIC -DHAVE_POWF=1 -Ithird_party/micromod -Ithird_party/atos_sound_client -I$(NF_SRC)/src -I$(NF_SRC)/third_party/libxmp/include tests/test_modplay_app.c system/modplay_sound.c third_party/micromod/micromod.c third_party/atos_sound_client/wire.c third_party/atos_sound_client/client.c $(NF_SRC)/src/mixer.c $(NF_SRC)/src/commands.c $(NF_SRC)/src/tracker.c $(wildcard $(NF_SRC)/third_party/libxmp/src/lite/*.c) -lm -o build/tests/test_modplay_pcm
	build/tests/test_modplay_pcm

# Rebuild an optional validated PCM cache with the same Micromod core as MODPLAY.
# gmake mod-cache MOD_INPUT=/path/song.mod PCM_OUTPUT=/path/HYMN.PCM
.PHONY: mod-cache
mod-cache:
	@mkdir -p build/tools
	$(HOSTCC) -O2 -Ithird_party/micromod third_party/micromod/atos_cache_builder.c third_party/micromod/micromod.c -o build/tools/mod_pcm_cache
	build/tools/mod_pcm_cache "$(MOD_INPUT)" "$(PCM_OUTPUT)"

.PHONY: check-settings
check-settings:
	@mkdir -p build/tests
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -Iinclude tests/test_settings_app.c -o build/tests/test_settings_app
	build/tests/test_settings_app
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -Iinclude tests/test_settings_config.c -o build/tests/test_settings_config
	build/tests/test_settings_config
