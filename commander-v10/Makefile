CROSS ?= m68k-elf-
.DEFAULT_GOAL := all
CC := $(CROSS)gcc
OBJCOPY := $(CROSS)objcopy
SIZE := $(CROSS)size
CFLAGS := -m68000 -Os -ffreestanding -fno-builtin -fomit-frame-pointer -Wall -Wextra -Werror -std=gnu11 -Iinclude
ASFLAGS := -m68000 -ffreestanding -Iinclude
LDFLAGS := -nostdlib -nostartfiles -Wl,-T,linker/mste-rom.ld -Wl,-Map,build/bios_mste.map -Wl,--gc-sections

OBJ := build/boot/reset.o build/boot/memdetect.o build/boot/exception.o build/boot/ramjump.o build/boot/mmu030.o build/boot/mmu_boot.o build/hw/machine.o \
	build/hw/memory.o build/hw/shifter.o build/hw/ikbd.o build/hw/rtc.o build/hw/expansion.o build/hw/nova.o build/hw/acsi.o build/hw/ide.o build/hw/block_device.o build/hw/config.o \
	build/diag/tests.o \
	build/ui/font.o build/ui/console.o build/ui/post.o build/ui/setup.o \
	build/ui/diagnostics.o build/ui/hardware.o build/ui/boot.o build/ui/lang.o build/ui/exception_report.o build/main.o

OBJ += build/loader/tos.o build/loader/bootstage.o build/ui/tos.o
DEP := $(OBJ:.o=.d) build/boot/reset_st.d build/system/boot_entry.d \
	build/system/boot_main.d build/system/kernel_entry.d build/system/kernel_main.d \
	build/system/kernel_memory.d build/system/kernel_exception.d build/system/kernel_module.d build/system/kernel_graphics.d build/system/emutos_vdi_mono.d build/system/kernel_blitter.d build/system/kernel_surface.d \
	build/system/kernel_block.d \
	build/system/kernel_fat.d \
	build/system/kernel_partition.d \
	build/system/acsi_module.d build/system/core_module.d build/system/vfs_module.d build/system/proc_module.d build/system/timer_module.d build/system/event_module.d build/system/input_module.d build/system/shell_module.d build/system/shell_path.d build/system/shell_complete.d build/system/hello_app.d build/system/fault_app.d build/system/filetest_app.d build/system/seektest_app.d build/system/argtest_app.d build/system/iotest_app.d build/system/yieldtest_app.d build/system/sleeptest_app.d build/system/sysinfo_app.d build/system/dirtest_app.d build/system/cwdtest_app.d build/system/memtest_app.d build/system/proctest_app.d build/system/pinfo_app.d build/system/ptable_app.d build/system/child_app.d build/system/spawntest_app.d build/system/argchild_app.d build/system/sparg_app.d build/system/fltchild_app.d build/system/fspawn_app.d build/system/coopa_app.d build/system/coopb_app.d build/system/async_app.d build/system/waittest_app.d build/system/blockwait_app.d build/system/ctxframe_app.d build/system/ctxswitch_app.d build/system/ctxrr_app.d build/system/ctxfault_app.d build/system/sleepctx_app.d build/system/pidwait_app.d build/system/wpid_app.d build/system/rspawn_app.d build/system/orphan_app.d build/system/multi_app.d build/system/bgyield_app.d build/system/waitkey_app.d build/system/waitline_app.d build/system/procstress_app.d build/system/stackfault_app.d build/system/pointertest_app.d build/system/heapguard_app.d build/system/busfault_app.d build/system/addressfault_app.d build/system/divfault_app.d build/system/chkfault_app.d build/system/trapvfault_app.d build/system/privfault_app.d build/system/linea_app.d build/system/linef_app.d build/system/trap1fault_app.d build/system/trap15fault_app.d build/system/exceptionwait_app.d build/system/hogtest_app.d build/system/hang_app.d build/system/quota_app.d build/system/holdmem_app.d

DEP += build/system/ipctest_app.d build/system/svctest_app.d build/system/init_service_app.d build/system/gfx_service_app.d build/system/gfxtest_app.d build/system/surftest_app.d build/system/gui_service_app.d build/system/guitest_app.d build/system/guikey_app.d build/system/guistream_app.d build/system/guimod_app.d build/system/guimouse_app.d build/system/desktop_app.d
DEP += build/system/path_app.d

DEP += build/system/sysinfo.d
DEP += build/system/kernel_console.d build/system/text_terminal.d build/system/kernel_sessions.d
DEP += build/system/kernel_video.d build/system/kernel_video_boot.d build/system/vofa_et4000.d
-include $(DEP)

.PHONY: all clean check toolchain hatari hatari-st hatari-rgb hatari-mono hatari-medium
all: build/bios_mste.bin build/bios_mste_hi.bin build/bios_mste_lo.bin \
	build/bios_st.bin build/bios_st_hi.bin build/bios_st_lo.bin build/system/BOOT.SYS build/system/KERNEL.SYS build/system/ACSI.SYS build/system/CORE.SYS build/system/VFS.SYS build/system/PROC.SYS build/system/TIMER.SYS build/system/EVENT.SYS build/system/INPUT.SYS build/system/SHELL.SYS build/system/HELLO.APP build/system/FAULT.APP build/system/FILETEST.APP build/system/SEEKTEST.APP build/system/ARGTEST.APP build/system/IOTEST.APP build/system/YIELD.APP build/system/SLEEP.APP build/system/SYSINFO.APP build/system/DIRTEST.APP build/system/CWDTEST.APP build/system/MEMTEST.APP build/system/PROCTEST.APP build/system/PINFO.APP build/system/PTABLE.APP build/system/CHILD.APP build/system/SPAWN.APP build/system/ARGCHILD.APP build/system/SPARG.APP build/system/FLTCHILD.APP build/system/FSPAWN.APP build/system/COOPA.APP build/system/COOPB.APP build/system/ASYNC.APP build/system/WAITTEST.APP build/system/BWAIT.APP build/system/CTXFRAME.APP build/system/CTXSW.APP build/system/CTXRR.APP build/system/CTXFLT.APP build/system/SLPCTX.APP build/system/PIDWAIT.APP build/system/WPID.APP build/system/RSPAWN.APP build/system/ORPHAN.APP build/system/MULTI.APP build/system/BGYIELD.APP build/system/WAITKEY.APP build/system/WAITLINE.APP build/system/PROCSTR.APP build/system/STKFAULT.APP build/system/PTRTEST.APP build/system/HGUARD.APP build/system/BUSFLT.APP build/system/ADRFLT.APP build/system/DIVFLT.APP build/system/CHKFLT.APP build/system/TRAPVFLT.APP build/system/PRIVFLT.APP build/system/LINEA.APP build/system/LINEF.APP build/system/TRAP1.APP build/system/TRAP15.APP build/system/EXWAIT.APP build/system/HOGTEST.APP build/system/HANG.APP build/system/QUOTA.APP build/system/HOLDMEM.APP build/system/BADPOL.APP build/checksums.txt

all: build/system/IPCTEST.APP
all: build/system/SVCTEST.APP
all: build/system/INIT.APP
all: build/system/GFX.APP build/system/GFXTEST.APP build/system/SURFTEST.APP build/system/GUI.APP build/system/GUITEST.APP build/system/GUIKEY.APP build/system/GUISTRM.APP build/system/GUIMOD.APP build/system/GUIMOUSE.APP build/system/DESKTOP.APP
all: build/system/PATH.APP

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

build/system/kernel_graphics.o: system/kernel_graphics.c include/kernel_graphics.h include/font.h include/hardware.h
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

build/system/KERNEL.SYS: build/system/kernel_entry.o build/system/kernel_main.o build/system/kernel_memory.o build/system/kernel_exception.o build/system/kernel_module.o build/system/kernel_graphics.o build/system/text_terminal.o build/system/video_driver.o build/system/irq.o build/system/desktop_module.o build/system/mod_clock.o build/system/kernel_console.o build/system/kernel_sessions.o build/system/kernel_video.o build/system/kernel_video_boot.o build/system/vofa_et4000.o build/system/video_bus.o build/system/emutos_vdi_mono.o build/system/kernel_blitter.o build/system/kernel_surface.o build/system/wm_core.o build/system/wm_blit.o build/system/kernel_wm.o build/system/tos_vdi.o build/system/tos_prg.o build/system/tos_entry.o build/system/tos_start.o build/system/kernel_block.o build/system/kernel_fat.o build/system/kernel_partition.o build/ui/font.o linker/kernel-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/kernel-sys.ld -Wl,--gc-sections -o build/system/kernel.elf build/system/kernel_entry.o build/system/kernel_main.o build/system/kernel_memory.o build/system/kernel_exception.o build/system/kernel_module.o build/system/kernel_graphics.o build/system/text_terminal.o build/system/video_driver.o build/system/irq.o build/system/desktop_module.o build/system/mod_clock.o build/system/kernel_console.o build/system/kernel_sessions.o build/system/kernel_video.o build/system/kernel_video_boot.o build/system/vofa_et4000.o build/system/video_bus.o build/system/emutos_vdi_mono.o build/system/kernel_blitter.o build/system/kernel_surface.o build/system/wm_core.o build/system/wm_blit.o build/system/kernel_wm.o build/system/tos_vdi.o build/system/tos_prg.o build/system/tos_entry.o build/system/tos_start.o build/system/kernel_block.o build/system/kernel_fat.o build/system/kernel_partition.o build/ui/font.o -lgcc
	$(OBJCOPY) -O binary build/system/kernel.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/acsi_module.o: system/acsi_module.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

build/system/ACSI.SYS: build/system/acsi_module.o linker/module-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/module-sys.ld -Wl,--gc-sections -o build/system/acsi_module.elf build/system/acsi_module.o
	$(OBJCOPY) -O binary build/system/acsi_module.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/core_module.o: system/core_module.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

build/system/CORE.SYS: build/system/core_module.o linker/core-module-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/core-module-sys.ld -Wl,--gc-sections -o build/system/core_module.elf build/system/core_module.o
	$(OBJCOPY) -O binary build/system/core_module.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/vfs_module.o: system/vfs_module.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

build/system/VFS.SYS: build/system/vfs_module.o linker/vfs-module-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/vfs-module-sys.ld -Wl,--gc-sections -o build/system/vfs_module.elf build/system/vfs_module.o
	$(OBJCOPY) -O binary build/system/vfs_module.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/proc_module.o: system/proc_module.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

build/system/PROC.SYS: build/system/proc_module.o linker/proc-module-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/proc-module-sys.ld -Wl,--gc-sections -o build/system/proc_module.elf build/system/proc_module.o
	$(OBJCOPY) -O binary build/system/proc_module.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/timer_module.o: system/timer_module.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

build/system/TIMER.SYS: build/system/timer_module.o linker/timer-module-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/timer-module-sys.ld -Wl,--gc-sections -o build/system/timer_module.elf build/system/timer_module.o
	$(OBJCOPY) -O binary build/system/timer_module.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/event_module.o: system/event_module.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

build/system/EVENT.SYS: build/system/event_module.o linker/event-module-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/event-module-sys.ld -Wl,--gc-sections -o build/system/event_module.elf build/system/event_module.o
	$(OBJCOPY) -O binary build/system/event_module.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/input_module.o: system/input_module.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

build/system/INPUT.SYS: build/system/input_module.o linker/input-module-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/input-module-sys.ld -Wl,--gc-sections -o build/system/input_module.elf build/system/input_module.o
	$(OBJCOPY) -O binary build/system/input_module.elf $@
	python3 tools/patch_boot_checksum.py $@

build/system/shell_module.o: system/shell_module.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

build/system/SHELL.SYS: build/system/shell_module.o build/system/shell_path.o build/system/shell_complete.o linker/shell-module-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/shell-module-sys.ld -Wl,--gc-sections -o build/system/shell_module.elf build/system/shell_module.o build/system/shell_path.o build/system/shell_complete.o
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

build/system/DESKTOP.APP: build/system/desktop_app.o linker/app-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/desktop_app.elf build/system/desktop_app.o
	$(OBJCOPY) -O binary build/system/desktop_app.elf $@
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

build/bios_mste.bin: build/bios_mste.elf
	$(OBJCOPY) -O binary $< $@
	python3 tools/padrom.py $@ 262144

build/boot/reset_st.o: boot/reset.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -MMD -MP -DROM_ST=1 -c $< -o $@

build/bios_st.elf: $(filter-out build/boot/reset.o,$(OBJ)) build/boot/reset_st.o linker/st-rom.ld
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/st-rom.ld \
		-Wl,-Map,build/bios_st.map -Wl,--gc-sections -o $@ \
		build/boot/reset_st.o $(filter-out build/boot/reset.o,$(OBJ)) -lgcc
	$(SIZE) $@

build/bios_st.bin: build/bios_st.elf
	$(OBJCOPY) -O binary $< $@
	python3 tools/padrom.py $@ 196608

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

check: check-machine check-lang check-config check-exception check-irq check-video-driver check-video-backends check-cpu-ops check-desktop-module check-mod-clock
	python3 -m unittest discover -s tests -v

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
	for a in INIT GFX GUI; do python3 tools/install_boot_sys.py work/atos-v10-clean.img build/system/$$a.APP; done

clean:
	rm -rf build

# Host-side differential coverage; the boot test also exercises the 68000 path.
HOSTCC ?= cc
.PHONY: check-graphics
check-graphics:
	@mkdir -p build/tests
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -Iinclude tests/test_emutos_planar.c system/emutos_vdi_mono.c -o build/tests/test_emutos_planar
	build/tests/test_emutos_planar

# Emulator regression executable; not part of the interactive desktop build.
build/system/desktop_selftest.o: system/desktop_app.S tests/desktop_mouse_selftest.inc
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -DDESKTOP_SELFTEST -c $< -o $@

build/system/DESKTEST.APP: build/system/desktop_selftest.o linker/app-sys.ld tools/patch_boot_checksum.py
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/app-sys.ld -Wl,--gc-sections -o build/system/desktop_selftest.elf build/system/desktop_selftest.o
	$(OBJCOPY) -O binary build/system/desktop_selftest.elf $@
	python3 tools/patch_boot_checksum.py $@

# Position-independent native GUI processes; no TOS/MiNT runtime linked.
GUI_APP_CFLAGS = $(CFLAGS) -mpcrel -ffunction-sections -fdata-sections -fno-jump-tables
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
check-gui-apps: build/system/MONODESK.APP build/system/COUNT.APP build/system/NOTE.APP
	python3 tools/check_gui_apps.py

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
build/system/vofa_et4000.o: system/vofa_et4000.c
	$(CC) $(CFLAGS) -MMD -MP -ffunction-sections -fdata-sections -c $< -o $@
build/system/video_bus.o: system/video_bus.S
	$(CC) $(ASFLAGS) -c $< -o $@

.PHONY: check-video
check-video:
	@mkdir -p build/tests
	cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -Iinclude tests/test_vofa.c -o build/tests/test_vofa
	build/tests/test_vofa
	cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -Iinclude tests/test_video_output.c -o build/tests/test_video_output
	build/tests/test_video_output
	cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -Iinclude tests/test_video_boot.c -o build/tests/test_video_boot
	build/tests/test_video_boot

check-gui-apps: build/system/SYSINFO.APP

.PHONY: check-sysinfo
check-sysinfo:
	@mkdir -p build/tests
	cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -Iinclude tests/test_sysinfo.c system/text_terminal.c -o build/tests/test_sysinfo
	build/tests/test_sysinfo

# --- GEM desktop app (our desktop on the EmuTOS layer) ---
build/system/desktop_gem.o: system/desktop_gem.c
	@mkdir -p $(@D)
	$(CC) -m68000 -Os -ffreestanding -fomit-frame-pointer -Wall -Wextra -std=gnu11 -mpcrel -fno-jump-tables -Iinclude -c $< -o $@

build/system/tos_start.o: system/tos_start.S
	@mkdir -p $(@D)
	$(CC) -m68000 -ffreestanding -c $< -o $@

.PHONY: desktop-gem
desktop-gem: build/system/desktop_gem.o build/system/tos_start.o linker/tos-prg.ld tools/mk_tosprg.py
	$(CC) -m68000 -nostdlib -nostartfiles -Wl,-T,linker/tos-prg.ld -Wl,--gc-sections -Wl,--emit-relocs -o build/system/desktop_gem.elf build/system/tos_start.o build/system/desktop_gem.o
	$(OBJCOPY) -O binary build/system/desktop_gem.elf build/system/desktop_gem.bin
	python3 tools/mk_tosprg.py build/system/desktop_gem.elf build/system/DESKTOP.PRG
