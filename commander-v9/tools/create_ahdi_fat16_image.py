#!/usr/bin/env python3
"""Create a sparse AHDI image with 256 MiB C: and 512 MiB D: FAT16 volumes."""
import argparse
import pathlib
import struct

SECTOR = 512
PARTITIONS = ((256 * 1024 * 1024 // SECTOR, 8, b"SYSTEM     "),
              (512 * 1024 * 1024 // SECTOR, 16, b"DATA       "))


def fat_geometry(total, sectors_per_cluster):
    reserved, fats, root_entries = 1, 2, 512
    root_sectors = (root_entries * 32 + SECTOR - 1) // SECTOR
    sectors_per_fat = 1
    while True:
        data = total - reserved - fats * sectors_per_fat - root_sectors
        clusters = data // sectors_per_cluster
        updated = ((clusters + 2) * 2 + SECTOR - 1) // SECTOR
        if updated == sectors_per_fat:
            break
        sectors_per_fat = updated
    if not 4085 <= clusters < 65525:
        raise ValueError("geometry is not FAT16")
    return reserved, fats, root_entries, root_sectors, sectors_per_fat, clusters


def write_sector(handle, sector, data):
    if len(data) != SECTOR:
        raise ValueError("sector must be exactly 512 bytes")
    handle.seek(sector * SECTOR)
    handle.write(data)


def format_volume(handle, start, total, spc, label, serial):
    reserved, fats, roots, root_sectors, spf, _ = fat_geometry(total, spc)
    boot = bytearray(SECTOR)
    boot[0:3] = b"\xeb\x3c\x90"
    boot[3:11] = b"ATARIBIO"
    struct.pack_into("<H", boot, 11, SECTOR)
    boot[13] = spc
    struct.pack_into("<H", boot, 14, reserved)
    boot[16] = fats
    struct.pack_into("<H", boot, 17, roots)
    struct.pack_into("<H", boot, 19, 0)
    boot[21] = 0xF8
    struct.pack_into("<H", boot, 22, spf)
    struct.pack_into("<H", boot, 24, 32)
    struct.pack_into("<H", boot, 26, 8)
    struct.pack_into("<I", boot, 28, start)
    struct.pack_into("<I", boot, 32, total)
    boot[36] = 0x80
    boot[38] = 0x29
    struct.pack_into("<I", boot, 39, serial)
    boot[43:54] = label
    boot[54:62] = b"FAT16   "
    boot[510:512] = b"\x55\xaa"
    write_sector(handle, start, boot)
    first_fat = bytearray(SECTOR)
    first_fat[0:4] = b"\xf8\xff\xff\xff"
    for copy in range(fats):
        write_sector(handle, start + reserved + copy * spf, first_fat)
    root_start = start + reserved + fats * spf
    handle.seek(root_start * SECTOR)
    handle.write(b"\0" * (root_sectors * SECTOR))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("image", type=pathlib.Path)
    args = parser.parse_args()
    starts = []
    next_sector = 1
    for total, _, _ in PARTITIONS:
        starts.append(next_sector)
        next_sector += total
    with args.image.open("w+b") as image:
        image.truncate(next_sector * SECTOR)
        root = bytearray(SECTOR)
        for slot, ((total, _, _), start) in enumerate(zip(PARTITIONS, starts)):
            offset = 0x1C6 + slot * 12
            root[offset] = 1
            root[offset + 1:offset + 4] = b"BGM"
            struct.pack_into(">I", root, offset + 4, start)
            struct.pack_into(">I", root, offset + 8, total)
        checksum = sum(struct.unpack(">256H", root)) & 0xFFFF
        struct.pack_into(">H", root, 510, (0x1234 - checksum) & 0xFFFF)
        write_sector(image, 0, root)
        for number, ((total, spc, label), start) in enumerate(zip(PARTITIONS, starts), 1):
            format_volume(image, start, total, spc, label, 0x41530000 + number)
    print(f"created {args.image}: C=256 MiB, D=512 MiB, {next_sector} sectors")


if __name__ == "__main__":
    main()
