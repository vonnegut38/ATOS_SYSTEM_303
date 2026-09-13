include Makefile
.PHONY: medium-test
medium-test: work/bios_mste_medium_test.bin
work/shifter_medium_test.o: work/shifter_medium_test.c
	$(CC) $(CFLAGS) -ffunction-sections -fdata-sections -c $< -o $@
work/bios_mste_medium_test.elf: $(filter-out build/hw/shifter.o,$(OBJ)) work/shifter_medium_test.o
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -Wl,-T,linker/mste-rom.ld -Wl,--gc-sections -o $@ $^ -lgcc
work/bios_mste_medium_test.bin: work/bios_mste_medium_test.elf
	$(OBJCOPY) -O binary $< $@
	python3 tools/padrom.py $@ 262144
