#!/bin/sh
# ---------------------------------------------------------------------------
# Flashback (REminiscence) na ATOS — uruchomienie w Hatari.
#
#   ./run-flashback.sh                      # nasz obraz z ikona na pulpicie
#   IMG=work/flash.img ./run-flashback.sh   # wariant z auto-startem gry
#
# Obraz: work/atos-v2.img  (ATOS + pulpit + FLASHBK.APP + dane demo w
#        \GAMES\FLASHBK\DATA + skrot FLASHBACK na pulpicie).
# Gra startuje KLIKNIECIEM ikony FLASHBACK.  Wariant IMG=work/flash.img
# uruchamia ja sam po ~40 s od wlaczenia.
#
# Uwaga wazna: emulator tego forka wypisuje na stdout kilkaset linii
# "VOFA-*" na minute (raportuje tryb karty).  Bez przekierowania zalewa to
# terminal, a zamkniecie terminala zabija emulator - stad wrazanie, ze
# "cale Hatari sie wywalilo".  Dlatego wyjscie emulatora idzie do pliku.
# ---------------------------------------------------------------------------
cd "$(dirname "$0")" || exit 1

HATARI="${HATARI:-/Users/xxx/Documents/Deepseek_test/atari-030-port/hatari-tf536}"
IMG="${IMG:-work/atos-v2.img}"
BIOS="${BIOS:-build/bios_mste.bin}"
LOG="${LOG:-work/hatari-flashback.log}"

[ -x "$HATARI" ] || { echo "brak emulatora: $HATARI" >&2; exit 1; }
[ -f "$IMG" ]    || { echo "brak obrazu: $IMG" >&2; exit 1; }
[ -f "$BIOS" ]   || { echo "brak ROM-u BIOS-u: $BIOS" >&2; exit 1; }

# Inna instancja Hatari trzyma obraz i nowa konczy sie natychmiast bledem
# "Locking IDE HD file for writing failed" (proces nazywa sie .../src/hatari,
# nie "hatari-tf536").  Ostrzez zawczasu.
if pgrep -f "src/hatari" >/dev/null 2>&1; then
    echo "UWAGA: dziala juz inna instancja Hatari - trzyma blokade obrazu." >&2
    echo "       Zamknij tamto okno albo: pkill -9 -x hatari" >&2
    echo "       (inaczej ta instancja zakonczy sie od razu)" >&2
    echo >&2
fi

echo "Flashback na ATOS"
echo "  obraz    : $IMG"
echo "  emulator : $HATARI"
echo "  wyjscie  : $LOG   (zeby nie zalewalo terminala)"
echo "  start    : kliknij ikone FLASHBACK na pulpicie"
echo

exec "$HATARI" \
    --machine megast --cpulevel 3 --cpuclock 32 \
    --memsize 8 --ttram 64 --tf536 on \
    --graphics-card vofa --gfx-chip et4000 --mono \
    --fast-forward off \
    --ide-master "$IMG" \
    --acsi 0=none --acsi 1=none --acsi 2=none --acsi 3=none \
    --acsi 4=none --acsi 5=none --acsi 6=none --acsi 7=none \
    --mmu off --tos "$BIOS" --patch-tos false \
    --confirm-quit false \
    --window --statusbar false --borders false --resizable false \
    --max-width 1024 --max-height 800 --zoom 1 \
    > "$LOG" 2>&1
