#!/bin/sh
set -eu
test_dir=/tmp/atari-bios-autoboot-test
fifo_path=$test_dir/filetest-control-$$.fifo
rm -f "$fifo_path"
(
    while [ ! -p "$fifo_path" ]; do sleep 1; done
    sleep 32
    for key in 25 17 32 28; do
        printf 'hatari-event keypress %s\n' "$key" > "$fifo_path"
        sleep 0.14
    done
    sleep 1
    for key in 31 20 30 19 20 57 43 31 21 31 20 18 50 43 48 34 21 23 18 38 32 52 30 25 25 28; do
        printf 'hatari-event keypress %s\n' "$key" > "$fifo_path"
        sleep 0.18
    done
    sleep 1
    printf '%s\n' 'hatari-shortcut screenshot' > "$fifo_path"
    sleep 5
    printf '%s\n' 'hatari-shortcut screenshot' > "$fifo_path"
    sleep 2
    printf '%s\n' 'hatari-shortcut quit' > "$fifo_path"
) &
exec /opt/homebrew/Cellar/hatari/2.6.1/Hatari.app/Contents/MacOS/hatari \
    --machine megaste --monitor mono --cpulevel 0 --cpuclock 16 --memsize 4 \
    --addr24 on --ttram 0 --tos build/bios_mste.bin --patch-tos false \
    --acsi 0="$test_dir/acsi.img" --protect-hd on \
    --trace cpu_exception --trace-file "$test_dir/filetest-trace.log" \
    --cmd-fifo "$fifo_path" --confirm-quit false
