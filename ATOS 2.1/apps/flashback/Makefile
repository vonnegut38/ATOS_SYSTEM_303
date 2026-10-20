# FLASHBK.APP - REminiscence (Flashback engine) for ATOS on Mega ST + TF536 + ET4000.
#
#   gmake                 build build/FLASHBK.APP
#   gmake check           host tests of the port C library
#   gmake install IMG=../../work/atos-v2.img [DATA=/path/to/flashback/DATA]
#                         copy FLASHBK.APP (and the game data, if given) into the
#                         Hatari/ACSI image: \SYSTEM\APP\FLASHBK.APP,
#                         \GAMES\FLASHBK\DATA\*  (stop Hatari first)
#
# Needs a 68020+ CPU (TF536 68030); no FPU; ATOS kernel with ABI 1.2.

CROSS    ?= m68k-elf-
CC       := $(CROSS)gcc
CXX      := $(CROSS)g++
OBJCOPY  := $(CROSS)objcopy
SIZE     := $(CROSS)size
PYTHON   ?= python3
HOSTCC   ?= cc
HOSTCXX  ?= c++
HOSTSAN  ?= -fsanitize=address,undefined
ATOS     := ../..

BUILD    ?= build
APP      := $(BUILD)/FLASHBK.APP
STACK    ?= 32768

PROFILE  ?= 0
# 1 = build testowy z tonem kontrolnym przy starcie (dowod sciezki audio).
AUDIO_TEST ?= 0
VERIFY_RESTORE ?= 0
CPU      := -m68030 -msoft-float
INCLUDES := -nostdinc -Iport/include -Iport -Iengine -I$(ATOS)/include \
            -I$(ATOS)/third_party/atos_sound_client
COMMON   := $(CPU) -O2 -fomit-frame-pointer -ffunction-sections -fdata-sections -fno-common \
            -fno-asynchronous-unwind-tables -fno-unwind-tables -fno-stack-protector \
            -DATOS_PROFILE=$(PROFILE) -DATOS_VERIFY_RESTORE=$(VERIFY_RESTORE) -Wall -Wno-unused-parameter $(INCLUDES)
CXXFLAGS := $(COMMON) -DFLASHBK_AUDIO_SELFTEST=$(AUDIO_TEST) -std=c++17 -fno-exceptions -fno-rtti -fno-threadsafe-statics \
            -fno-use-cxa-atexit -nostdinc++ -Wno-class-memaccess -Wno-misleading-indentation
# The C library must never be "optimised" into calls to itself (memset loop -> memset()).
CFLAGS   := $(COMMON) -std=gnu11 -ffreestanding -fno-builtin -fno-tree-loop-distribute-patterns -Wextra
ASFLAGS  := $(CPU) -DAPP_STACK=$(STACK)
LDFLAGS  := $(CPU) -nostdlib -nostartfiles -Wl,-T,port/app-flashback.ld -Wl,--gc-sections \
            -Wl,--emit-relocs

# REminiscence sources used unchanged (except two marked ATOS lines in
# intern.h/game.cpp).  Not built: main/fs/file-helpers/util/mixer/players/
# scaler/screenshot/systemstub_sdl (replaced by port/*), staticres_controllers
# (SDL game-controller database).
ENGINE   := collision cutscene decode_mac file game graphics menu piege protection \
            resource resource_aba resource_mac resource_paq seq_player staticres unpack video
PORT_CXX := systemstub_atos fs_atos util_atos audio_atos main_atos selftest cxxrt
# Dzwiek idzie do emulowanego ESP (ASND) - ten sam klient co MODPLAY/PRE2.
ASND      := $(ATOS)/third_party/atos_sound_client
ASND_OBJS := $(BUILD)/asnd/wire.o $(BUILD)/asnd/client.o $(BUILD)/asnd/upload.o
OBJS     := $(BUILD)/crt0.o $(BUILD)/libc.o \
            $(addprefix $(BUILD)/engine/,$(addsuffix .o,$(ENGINE))) \
            $(addprefix $(BUILD)/port/,$(addsuffix .o,$(PORT_CXX))) \
            $(ASND_OBJS)

.PHONY: all check check-graphics install clean FORCE
all: $(APP)

# Rebuild when switching profiling/verification/compiler options in the same
# output directory; stale objects otherwise make timing comparisons misleading.
$(BUILD)/flags: FORCE
	@mkdir -p $(@D)
	@printf '%s\n' '$(CXXFLAGS) $(ENGINE_FLAGS) $(CFLAGS) $(ASFLAGS)' > $@.tmp
	@cmp -s $@.tmp $@ && rm $@.tmp || mv $@.tmp $@

$(OBJS): $(BUILD)/flags

# Engine: no debug()/assert() code in the hot paths (util.h turns debug() into
# nothing under NDEBUG; the port files keep their diagnostics).
ENGINE_FLAGS ?= -DNDEBUG -Wno-unused-variable -Wno-unused-but-set-variable
$(BUILD)/engine/%.o: engine/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(ENGINE_FLAGS) -MMD -MP -c $< -o $@
$(BUILD)/port/%.o: port/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@
$(BUILD)/libc.o: port/libc.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@
# Klient ASND jest C (dzielony z MODPLAY/PRE2); szybkie CRC jak w MODPLAY.
$(BUILD)/asnd/%.o: $(ASND)/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -DAS_FAST_CRC32 -I$(ASND) -MMD -MP -c $< -o $@
$(BUILD)/crt0.o: port/crt0.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -c $< -o $@
$(BUILD)/reloc0.S:
	@mkdir -p $(@D)
	printf '    .section .app_reloc,"a"\n    .long 0\n' > $@
$(BUILD)/%.o: $(BUILD)/%.S
	$(CC) $(ASFLAGS) -c $< -o $@

# Pass 1: link with an empty relocation table and collect the absolute words.
# Pass 2: link the table in.  It lives behind .data, so no relocated word moves;
# --check re-reads pass 2 and refuses a stale table.
$(BUILD)/pass1.elf: $(OBJS) $(BUILD)/reloc0.o port/app-flashback.ld
	$(CXX) $(LDFLAGS) -o $@ $(OBJS) $(BUILD)/reloc0.o -lgcc -Wl,-Map,$(BUILD)/pass1.map
$(BUILD)/reloc.S: $(BUILD)/pass1.elf tools/mkreloc.py
	$(PYTHON) tools/mkreloc.py $< $@
$(BUILD)/flashbk.elf: $(OBJS) $(BUILD)/reloc.o port/app-flashback.ld
	$(CXX) $(LDFLAGS) -o $@ $(OBJS) $(BUILD)/reloc.o -lgcc -Wl,-Map,$(BUILD)/flashbk.map
	$(PYTHON) tools/mkreloc.py --check $@ $(BUILD)/reloc.S
$(APP): $(BUILD)/flashbk.elf
	$(OBJCOPY) -O binary $< $@
	$(PYTHON) $(ATOS)/tools/patch_boot_checksum.py $@
	$(PYTHON) tools/check_app.py $@ $<
	-$(SIZE) $<

check:
	@mkdir -p $(BUILD)/host
	$(HOSTCC) -c -O1 -ffreestanding -fno-builtin -nostdinc -Iport/include -Iport -std=gnu11 port/libc.c -o $(BUILD)/host/libc_raw.o
	objcopy --prefix-symbols=t_ $(BUILD)/host/libc_raw.o $(BUILD)/host/libc_t.o
	$(HOSTCC) -O1 -g -no-pie tests/test_libc_host.c $(BUILD)/host/libc_t.o -o $(BUILD)/host/test_libc
	$(BUILD)/host/test_libc

# Regression tests for sparse damage, packed pixel expansion and the sprite
# cache. Supply the DOS DATA directory (the game assets are not in the repo).
check-graphics:
	@test -n "$(DATA)" || (echo 'Pass DATA=/path/to/full/DATA'; exit 1)
	@mkdir -p $(BUILD)/host
	$(HOSTCXX) -std=c++17 -DATOS_PROFILE=0 $(HOSTSAN) -Iengine -Iport -I$(ATOS)/include tests/test_damage.cpp -o $(BUILD)/host/test_damage
	$(BUILD)/host/test_damage
	$(HOSTCXX) -std=c++17 $(HOSTSAN) tests/test_sprite_cache.cpp -o $(BUILD)/host/test_sprite_cache
	$(BUILD)/host/test_sprite_cache $(foreach n,PERSO GLUE JUNKY MERCENAI REPLICAN,"$(DATA)/$(n).SPR" "$(DATA)/$(n).OFF")
	$(PYTHON) tests/test_bank_cache.py

IMG  ?= $(ATOS)/work/atos-v2.img
DATA ?=
install: $(APP)

	$(PYTHON) $(ATOS)/tools/install_boot_sys.py $(IMG) $(APP) --directory SYSTEM/APP
	$(PYTHON) tools/install_data.py $(IMG) --cfg port/FLASHBK.CFG $(if $(DATA),--data $(DATA))

clean:
	rm -rf $(BUILD)

-include $(OBJS:.o=.d)
