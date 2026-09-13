#!/bin/sh
set -eu
test_dir=/tmp/atari-bios-tos-test
fifo_path=$test_dir/control.fifo
mkdir -p "$test_dir"
rm -f "$fifo_path"
cp -c /Users/xxx/Downloads/hatari-2.6.1_macos/Atari3g.img "$test_dir/acsi.img"
(
    while [ ! -p "$fifo_path" ]; do sleep 1; done
    sleep 2
    printf '%s\n' 'hatari-event keypress 62' > "$fifo_path"
    sleep 4
    printf '%s\n' 'hatari-event keypress 28' > "$fifo_path"
    sleep 1
    printf '%s\n' 'hatari-event keypress 63' > "$fifo_path"
    sleep 3
    printf '%s\n' 'hatari-event keypress 80' > "$fifo_path"
    sleep 1
    printf '%s\n' 'hatari-event keypress 80' > "$fifo_path"
    sleep 1
    printf '%s\n' 'hatari-event keypress 28' > "$fifo_path"
    sleep 1
    printf '%s\n' 'hatari-event keypress 28' > "$fifo_path"
    sleep 8
    printf '%s\n' 'hatari-shortcut quit' > "$fifo_path"
) &
exec /opt/homebrew/Cellar/hatari/2.6.1/Hatari.app/Contents/MacOS/hatari \
    --machine megaste --monitor mono --cpulevel 0 --cpuclock 16 --memsize 4 \
    --tos build/bios_mste.bin --patch-tos false \
    --acsi 0="$test_dir/acsi.img" \
    --protect-hd on --trace scsi_cmd,cpu_exception \
    --trace-file /tmp/atari-bios-tos.log --cmd-fifo "$fifo_path" \
    --confirm-quit false
