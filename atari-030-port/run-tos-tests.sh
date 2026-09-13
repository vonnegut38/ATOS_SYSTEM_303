#!/bin/sh
#
# Bootuje ASF BIOS + TF536 + ET4000 i uruchamia 5 testowych programow TOS
# (PROG1..PROG5) w jednej sesji. Sluzy do testu stabilnosci persony TOS (T1).
#
#   ./run-tos-tests.sh
#   SDL_VIDEODRIVER=dummy TOS_TEST_SHOT=/tmp/five.png ./run-tos-tests.sh --run-vbls 4000
#
set -e

HERE=$(cd "$(dirname "$0")" && pwd)
PROJECT=$(cd "$HERE/../files-pasted-by-the-user-projekt" && pwd)
HATARI="$HERE/hatari-tf536"
IMAGE="$HERE/disk/work.img"
BIOS="$PROJECT/build/bios_mste.bin"
FIFO="/tmp/asf_tos_tests_$$"

if [ ! -x "$HATARI" ] || [ ! -f "$BIOS" ] || [ ! -f "$IMAGE" ]; then
	echo "Brak hatari-tf536 / bios_mste.bin / work.img" >&2
	exit 1
fi

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

i=0
while [ $i -lt 150 ] && [ ! -p "$FIFO" ]; do sleep 0.1; i=$((i+1)); done
if [ ! -p "$FIFO" ]; then
	echo "Hatari nie utworzyl FIFO" >&2
	kill "$PID" 2>/dev/null || true
	exit 1
fi

(
	sleep 24
	for n in 1 2 3 4 5; do
		for c in r u n; do printf 'hatari-event keypress %s\n' "$c"; sleep 0.2; done
		printf 'hatari-event keypress 0x39\n'; sleep 0.2
		for c in p r o g; do printf 'hatari-event keypress %s\n' "$c"; sleep 0.2; done
		printf 'hatari-event keypress %s\n' "$n"; sleep 0.2
		printf 'hatari-event keypress 0x34\n'; sleep 0.2
		for c in p r g; do printf 'hatari-event keypress %s\n' "$c"; sleep 0.2; done
		sleep 0.2
		printf 'hatari-event keypress 0x1c\n'
		sleep 3
	done
	if [ -n "$TOS_TEST_SHOT" ]; then
		sleep 2
		printf 'hatari-debug screenshot %s\n' "$TOS_TEST_SHOT"
	fi
) > "$FIFO" &
FEEDER=$!

wait "$PID" || true
wait "$FEEDER" 2>/dev/null || true
rm -f "$FIFO"
