#!/bin/sh
set -eu
test_dir=/tmp/atari-bios-autoboot-test
fifo_path=$test_dir/control.fifo
disk_path=${HATARI_DISK:-$test_dir/acsi.img}
mkdir -p "$test_dir"
rm -f "$fifo_path"
if [ ! -f "$disk_path" ]; then
    cp -c /Users/xxx/Downloads/hatari-2.6.1_macos/Atari3g.img "$disk_path"
fi
(
    while [ ! -p "$fifo_path" ]; do sleep 1; done
    sleep 12
    printf '%s\n' 'hatari-shortcut screenshot' > "$fifo_path"
    sleep 2
    printf '%s\n' 'hatari-shortcut quit' > "$fifo_path"
) &
exec /opt/homebrew/Cellar/hatari/2.6.1/Hatari.app/Contents/MacOS/hatari \
    --machine "${HATARI_MACHINE:-megaste}" --monitor "${HATARI_MONITOR:-mono}" \
    --cpulevel "${HATARI_CPULEVEL:-0}" --cpuclock "${HATARI_CPUCLOCK:-16}" --memsize 4 \
    --addr24 "${HATARI_ADDR24:-on}" --ttram "${HATARI_TTRAM:-0}" \
    --tos "${HATARI_ROM:-build/bios_mste.bin}" --patch-tos false \
    --acsi 0="$disk_path" --protect-hd on \
    --trace scsi_cmd,cpu_exception --trace-file /tmp/atari-bios-autoboot.log \
    --cmd-fifo "$fifo_path" --confirm-quit false
