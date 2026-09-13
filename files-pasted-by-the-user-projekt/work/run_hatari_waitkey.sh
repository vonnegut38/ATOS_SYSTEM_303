#!/bin/sh
set -eu
test_dir=/tmp/atari-bios-autoboot-test
fifo_path=$test_dir/waitkey-control-$$.fifo
rm -f "$fifo_path"
(
    while [ ! -p "$fifo_path" ]; do sleep 1; done
    sleep 32
    for key in 31 20 30 19 20 57 43 31 21 31 20 18 50 43 17 30 23 20 37 18 21 52 30 25 25 28; do
        printf 'hatari-event keypress %s\n' "$key" > "$fifo_path"
        sleep 0.10
    done
    sleep 1
    for key in 36 24 48 31 28; do
        printf 'hatari-event keypress %s\n' "$key" > "$fifo_path"
        sleep 0.10
    done
    sleep 1
    for key in 33 34 57 2 28; do
        printf 'hatari-event keypress %s\n' "$key" > "$fifo_path"
        sleep 0.10
    done
    sleep 1
    printf '%s\n' 'hatari-event keypress 45' > "$fifo_path"
    sleep 2
    printf '%s\n' 'hatari-shortcut screenshot' > "$fifo_path"
    sleep 1
    printf '%s\n' 'hatari-shortcut quit' > "$fifo_path"
) &
exec open -W -na /opt/homebrew/Cellar/hatari/2.6.1/Hatari.app --args \
    --machine megaste --monitor mono --cpulevel 0 --cpuclock 16 --memsize 4 \
    --addr24 on --ttram 0 --tos build/bios_mste.bin --patch-tos false \
    --acsi 0="$test_dir/acsi.img" --protect-hd on \
    --trace cpu_exception --trace-file "$test_dir/waitkey-trace.log" \
    --cmd-fifo "$fifo_path" --confirm-quit false
