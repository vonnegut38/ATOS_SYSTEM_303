CC      = m68k-atari-mint-gcc
CFLAGS  = -mshort -O2 -Wall
LDLIBS  = -lgem
DESKTOP  = DESKTOP.PRG
all: $(DESKTOP)
$(DESKTOP): desktop_mint.c
	$(CC) $(CFLAGS) -o $@ $< $(LDLIBS)
clean:
	rm -f $(DESKTOP)
