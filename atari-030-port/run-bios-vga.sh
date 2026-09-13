#!/bin/sh
#
# ASF BIOS + TF536 (68030 / 64 MB ALT-RAM / IDE) + VOFA ET4000 na Mega ST.
#
# Domyslnie BIOS startuje z dysku IDE podlaczonego do karty TF536 (bez TOS
# plyty i bez ACSI). Zeby uruchomic z ACSI zamiast IDE:
#     BOOT=acsi ./run-bios-vga.sh
# Test bez okna (zrzuty przez debugger):
#     SDL_VIDEODRIVER=dummy ./run-bios-vga.sh --run-vbls 600
#
set -e

HERE=$(cd "$(dirname "$0")" && pwd)
PROJECT=$(cd "$HERE/../files-pasted-by-the-user-projekt" && pwd)
BIOS="$PROJECT/build/bios_mste.bin"
IMAGE="$HERE/disk/work.img"
HATARI="$HERE/hatari-tf536"

if [ ! -x "$HATARI" ]; then
	echo "Blad: brak $HATARI lub nie jest wykonywalny." >&2
	echo "Zbuduj: export PATH=\"$HERE/tools/py/bin:\$PATH\" PYTHONPATH=\"$HERE/tools/py\"" >&2
	echo "        cd \"$HERE/hatari-src\" && cmake --build build" >&2
	exit 1
fi
if [ ! -f "$BIOS" ]; then
	echo "Blad: brak ROM-u $BIOS" >&2
	echo "Zbuduj: cd \"$PROJECT\" && gmake build/bios_mste.bin" >&2
	exit 1
fi
if [ ! -f "$IMAGE" ]; then
	echo "Blad: brak obrazu dysku $IMAGE" >&2
	exit 1
fi

# Wylacz wszystkie ACSI z konfiguracji uzytkownika, zeby BIOS nie znalazl
# przypadkowego dysku ACSI przed IDE.
NOACSI=""
for id in 0 1 2 3 4 5 6 7; do
	NOACSI="$NOACSI --acsi $id=none"
done

if [ "${BOOT:-ide}" = "acsi" ]; then
	# shellcheck disable=SC2086
	exec "$HATARI" \
		--machine megast --cpulevel 3 --cpuclock 32 --memsize 4 --ttram 64 \
		--tf536 on --graphics-card vofa --gfx-chip et4000 --mono --zoom 2 \
		--acsi 0="$IMAGE" \
		--tos "$BIOS" \
		"$@"
fi

# shellcheck disable=SC2086
exec "$HATARI" \
	--machine megast --cpulevel 3 --cpuclock 32 --memsize 4 --ttram 64 \
	--tf536 on --graphics-card vofa --gfx-chip et4000 --mono --zoom 2 \
	$NOACSI --ide-master "$IMAGE" \
	--tos "$BIOS" \
	"$@"
