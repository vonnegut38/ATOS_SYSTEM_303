# Manifest audio — round6-autorun

Data: 2026-10-11 11:45:31

## Obraz

| plik | rozmiar | SHA-256 |
|---|---|---|
| work/audio-baseline/atos-audio-round6.img | 805306880 | `818c926e760f8a57bd02e8f233820498ba6c5b9a3a6e0db785b511f1285b9e9a` |

Odczyt z FAT: odczytany przez tools/img_get.py

## Artefakty: co jest w obrazie vs co zbudowano

| plik w FAT | w obrazie | SHA-256 (obraz) | build | SHA-256 (build) | zgodne |
|---|---|---|---|---|---|
| SYSTEM/KERNEL.SYS | 457498 | `d492303d9262f1f15d28e4d4fa442643bdb416f73ad8d810999a5b4afa14ec49` | 457498 | `d492303d9262f1f15d28e4d4fa442643bdb416f73ad8d810999a5b4afa14ec49` | TAK |
| SYSTEM/APP/DESKTOP.APP | 29962 | `cd74fd194cc711f82d77f307f97be3d01a2e210f6a882b9aa7a780914f010a85` | 29732 | `463ae13ecf66e9674d3c90c0548785b85a29b38112f2fac6402245c6c17d15b2` | NIE |
| SYSTEM/APP/TOSAUDIO.PRG | 1425 | `3f43583f0de8bfd6810ce0bf7a9f3a1ce03cb69adcfd5c1a716be77b0813bb70` | 1425 | `3f43583f0de8bfd6810ce0bf7a9f3a1ce03cb69adcfd5c1a716be77b0813bb70` | TAK |

## ELF, z których powstały artefakty

| plik | SHA-256 |
|---|---|
| build/system/kernel.elf | `16f858a250d904038462a682b7c3470d29c4df7ff0cc184f59f60607de930dd6` |
| build/system/desktop_wm.elf | `955c87b0aca6753f2cc6f125a53ac0f38515a9a5d1cdd6d8b6d726bb7314f987` |
| build/system/tos_audio.elf | `266c8c33992a67b144c1612ea37a51844c81f09dc4bd98f015f71f45636346f6` |

## Uruchomienie

```
/Users/xxx/Documents/Deepseek_test/atari-030-port/hatari-tf536 --machine megast --cpulevel 3 --cpuclock 32 --memsize 8 --ttram 64 --tf536 on --graphics-card vofa --gfx-chip et4000 --mono --sound 44100 --ide-master work/audio-baseline/atos-audio-round6.img
```

## JSON

```json
{
  "tag": "round6-autorun",
  "created": "2026-10-11 11:45:31",
  "image": {
    "path": "work/audio-baseline/atos-audio-round6.img",
    "present": true,
    "size": 805306880,
    "mtime": "2026-10-11 11:45:31",
    "sha256": "818c926e760f8a57bd02e8f233820498ba6c5b9a3a6e0db785b511f1285b9e9a"
  },
  "hatari": "hatari-tf536",
  "command": "/Users/xxx/Documents/Deepseek_test/atari-030-port/hatari-tf536 --machine megast --cpulevel 3 --cpuclock 32 --memsize 8 --ttram 64 --tf536 on --graphics-card vofa --gfx-chip et4000 --mono --sound 44100 --ide-master work/audio-baseline/atos-audio-round6.img",
  "artefacts": [
    {
      "fat_path": "SYSTEM/KERNEL.SYS",
      "build": {
        "path": "build/system/KERNEL.SYS",
        "present": true,
        "size": 457498,
        "mtime": "2026-10-11 11:44:22",
        "sha256": "d492303d9262f1f15d28e4d4fa442643bdb416f73ad8d810999a5b4afa14ec49"
      },
      "elf": {
        "path": "build/system/kernel.elf",
        "present": true,
        "size": 539056,
        "mtime": "2026-10-11 11:44:22",
        "sha256": "16f858a250d904038462a682b7c3470d29c4df7ff0cc184f59f60607de930dd6"
      },
      "in_image": {
        "size": 457498,
        "sha256": "d492303d9262f1f15d28e4d4fa442643bdb416f73ad8d810999a5b4afa14ec49"
      },
      "match": true
    },
    {
      "fat_path": "SYSTEM/APP/DESKTOP.APP",
      "build": {
        "path": "build/system/DESKTOP.APP",
        "present": true,
        "size": 29732,
        "mtime": "2026-10-11 11:31:36",
        "sha256": "463ae13ecf66e9674d3c90c0548785b85a29b38112f2fac6402245c6c17d15b2"
      },
      "elf": {
        "path": "build/system/desktop_wm.elf",
        "present": true,
        "size": 44420,
        "mtime": "2026-10-11 11:31:36",
        "sha256": "955c87b0aca6753f2cc6f125a53ac0f38515a9a5d1cdd6d8b6d726bb7314f987"
      },
      "in_image": {
        "size": 29962,
        "sha256": "cd74fd194cc711f82d77f307f97be3d01a2e210f6a882b9aa7a780914f010a85"
      },
      "match": false
    },
    {
      "fat_path": "SYSTEM/APP/TOSAUDIO.PRG",
      "build": {
        "path": "build/system/TOSAUDIO.PRG",
        "present": true,
        "size": 1425,
        "mtime": "2026-10-11 11:44:50",
        "sha256": "3f43583f0de8bfd6810ce0bf7a9f3a1ce03cb69adcfd5c1a716be77b0813bb70"
      },
      "elf": {
        "path": "build/system/tos_audio.elf",
        "present": true,
        "size": 10568,
        "mtime": "2026-10-11 11:44:50",
        "sha256": "266c8c33992a67b144c1612ea37a51844c81f09dc4bd98f015f71f45636346f6"
      },
      "in_image": {
        "size": 1425,
        "sha256": "3f43583f0de8bfd6810ce0bf7a9f3a1ce03cb69adcfd5c1a716be77b0813bb70"
      },
      "match": true
    }
  ],
  "fat_read": "odczytany przez tools/img_get.py"
}
```
