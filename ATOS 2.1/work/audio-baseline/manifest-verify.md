# Manifest audio — verify

Data: 2026-10-11 12:07:47

## Obraz

| plik | rozmiar | SHA-256 |
|---|---|---|
| work/audio-baseline/atos-audio-verify.img | 805306880 | `e04b7f55fc345fed1d7ff7aa4a7183d94062ae90e4c252412e53727bf69e3673` |

Odczyt z FAT: odczytany przez tools/img_get.py

## Artefakty: co jest w obrazie vs co zbudowano

| plik w FAT | w obrazie | SHA-256 (obraz) | build | SHA-256 (build) | zgodne |
|---|---|---|---|---|---|
| SYSTEM/KERNEL.SYS | 457498 | `d492303d9262f1f15d28e4d4fa442643bdb416f73ad8d810999a5b4afa14ec49` | 457498 | `d492303d9262f1f15d28e4d4fa442643bdb416f73ad8d810999a5b4afa14ec49` | TAK |
| SYSTEM/APP/DESKTOP.APP | 29982 | `5f99c933cdf3db367c4295dc43746e8b9de79873ecb8e872bbdc469e470560b5` | 29732 | `463ae13ecf66e9674d3c90c0548785b85a29b38112f2fac6402245c6c17d15b2` | NIE |
| SYSTEM/APP/TOSAUDIO.PRG | 1574 | `27627052448d2142a706bbc4d9fb0db755a328575cfabce7ab3848149a8812fe` | 1574 | `27627052448d2142a706bbc4d9fb0db755a328575cfabce7ab3848149a8812fe` | TAK |

## ELF, z których powstały artefakty

| plik | SHA-256 |
|---|---|
| build/system/kernel.elf | `16f858a250d904038462a682b7c3470d29c4df7ff0cc184f59f60607de930dd6` |
| build/system/desktop_wm.elf | `955c87b0aca6753f2cc6f125a53ac0f38515a9a5d1cdd6d8b6d726bb7314f987` |
| build/system/tos_audio.elf | `4fc2a82d8d238b2c1deb262f777f2a86706a4ed06f74a899bbd38668b0192629` |

## Uruchomienie

```
/Users/xxx/Documents/Deepseek_test/atari-030-port/hatari-tf536
```

## JSON

```json
{
  "tag": "verify",
  "created": "2026-10-11 12:07:47",
  "image": {
    "path": "work/audio-baseline/atos-audio-verify.img",
    "present": true,
    "size": 805306880,
    "mtime": "2026-10-11 12:07:47",
    "sha256": "e04b7f55fc345fed1d7ff7aa4a7183d94062ae90e4c252412e53727bf69e3673"
  },
  "hatari": "hatari-tf536",
  "command": "/Users/xxx/Documents/Deepseek_test/atari-030-port/hatari-tf536",
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
        "size": 29982,
        "sha256": "5f99c933cdf3db367c4295dc43746e8b9de79873ecb8e872bbdc469e470560b5"
      },
      "match": false
    },
    {
      "fat_path": "SYSTEM/APP/TOSAUDIO.PRG",
      "build": {
        "path": "build/system/TOSAUDIO.PRG",
        "present": true,
        "size": 1574,
        "mtime": "2026-10-11 12:04:20",
        "sha256": "27627052448d2142a706bbc4d9fb0db755a328575cfabce7ab3848149a8812fe"
      },
      "elf": {
        "path": "build/system/tos_audio.elf",
        "present": true,
        "size": 10860,
        "mtime": "2026-10-11 12:04:20",
        "sha256": "4fc2a82d8d238b2c1deb262f777f2a86706a4ed06f74a899bbd38668b0192629"
      },
      "in_image": {
        "size": 1574,
        "sha256": "27627052448d2142a706bbc4d9fb0db755a328575cfabce7ab3848149a8812fe"
      },
      "match": true
    }
  ],
  "fat_read": "odczytany przez tools/img_get.py"
}
```
