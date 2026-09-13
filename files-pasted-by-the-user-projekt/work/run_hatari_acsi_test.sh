#!/bin/sh
set -eu
test_dir=/tmp/atari-bios-acsi-test
fifo_path=$test_dir/control.fifo
disk_path=$test_dir/acsi.img
mkdir -p "$test_dir"
rm -f "$fifo_path"
if [ ! -f "$disk_path" ]; then
    cp -c /Users/xxx/Downloads/hatari-2.6.1_macos/Atari3g.img "$disk_path"
fi
(
    while [ ! -p "$fifo_path" ]; do sleep 1; done
    sleep 1
    printf '%s\n' 'hatari-event keypress 62' > "$fifo_path"
    sleep 4
    printf '%s\n' 'hatari-shortcut quit' > "$fifo_path"
) &
exec /opt/homebrew/Cellar/hatari/2.6.1/Hatari.app/Contents/MacOS/hatari \
    --configfile "/Users/xxx/Library/Application Support/hatari/hatari.cfg" \
    --machine megaste --tos build/bios_mste.bin --patch-tos false \
    --acsi "$disk_path" \
    --protect-hd on --trace scsi_cmd \
    --trace-file /tmp/atari-bios-acsi.log --cmd-fifo "$fifo_path" \
    --confirm-quit false
