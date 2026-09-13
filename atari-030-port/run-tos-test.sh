#!/bin/sh
#
# Uruchamia ASF BIOS + TF536 + ET4000 i po dojsciu do shella wpisuje
# "run test.prg", zeby pokazac persony TOS (T1) w oknie Hatari.
#
#   ./run-tos-test.sh              # okno
#   SDL_VIDEODRIVER=dummy ./run-tos-test.sh --run-vbls 1200   # bez okna (test)
#
set -e

HERE=$(cd "$(dirname "$0")" && pwd)
PROJECT=$(cd "$HERE/../files-pasted-by-the-user-projekt" && pwd)
HATARI="$HERE/hatari-tf536"
IMAGE="$HERE/disk/work.img"
BIOS="$PROJECT/build/bios_mste.bin"
FIFO="/tmp/asf_tos_$$"

if [ ! -x "$HATARI" ] || [ ! -f "$BIOS" ] || [ ! -f "$IMAGE" ]; then
	echo "Brak hatari-tf536 / bios_mste.bin / work.img" >&2
	exit 1
fi

# Wylacz ACSI z konfiguracji uzytkownika - boot z IDE.
NOACSI=""
for id in 0 1 2 3 4 5 6 7; do
	NOACSI="$NOACSI --acsi $id=none"
done

rm -f "$FIFO"

# shellcheck disable=SC2086
"$HATARI" \
	--machine megast --cpulevel 3 --cpuclock 32 --memsize 4 --ttram 64 \
	--tf536 on --graphics-card vofa --gfx-chip et4000 --mono --zoom 2 \
	$NOACSI --ide-master "$IMAGE" --tos "$BIOS" \
	--cmd-fifo "$FIFO" "$@" &
PID=$!

# Poczekaj az Hatari utworzy FIFO.
i=0
while [ $i -lt 150 ] && [ ! -p "$FIFO" ]; do sleep 0.1; i=$((i+1)); done
if [ ! -p "$FIFO" ]; then
	echo "Hatari nie utworzyl FIFO" >&2
	kill "$PID" 2>/dev/null || true
	exit 1
fi

# Po ~24 s (shell gotowy) wpisz "run test.prg" + Enter.
(
	sleep 24
	for c in r u n; do printf 'hatari-event keypress %s\n' "$c"; sleep 0.25; done
	printf 'hatari-event keypress 0x39\n'; sleep 0.25
	for c in t e s t; do printf 'hatari-event keypress %s\n' "$c"; sleep 0.25; done
	printf 'hatari-event keypress 0x34\n'; sleep 0.25
	for c in p r g; do printf 'hatari-event keypress %s\n' "$c"; sleep 0.25; done
	sleep 0.2
	printf 'hatari-event keypress 0x1c\n'
	if [ -n "$TOS_TEST_SHOT" ]; then
		sleep 4
		printf 'hatari-debug screenshot %s\n' "$TOS_TEST_SHOT"
	fi
) > "$FIFO" &
FEEDER=$!

wait "$PID" || true
wait "$FEEDER" 2>/dev/null || true
rm -f "$FIFO"
