# Manifest audio — r13

Data: 2026-10-11 17:57:52

## Obraz

| plik | rozmiar | SHA-256 |
|---|---|---|
| work/audio-baseline/atos-audio-r13.img | 805306880 | `8141a00ea5c022a8aac0f500d6ca26697b68dc98e3b6a2b1bd77d6b23d6944bc` |

Odczyt z FAT: odczytany przez tools/img_get.py

## Artefakty: co jest w obrazie vs co zbudowano

| plik w FAT | w obrazie | SHA-256 (obraz) | build | SHA-256 (build) | zgodne |
|---|---|---|---|---|---|
| SYSTEM/KERNEL.SYS | 457734 | `a59ce71dc0ac61f6a52b876495c7130ae6ea9ba05254411f07417ce1adcd41ad` | 457734 | `a59ce71dc0ac61f6a52b876495c7130ae6ea9ba05254411f07417ce1adcd41ad` | TAK |
| SYSTEM/APP/DESKTOP.APP | 29876 | `f6198730cb2f922960a699e7336add3eddc89a2462e01d2fd9d3d129e44c9ce3` | 29876 | `f6198730cb2f922960a699e7336add3eddc89a2462e01d2fd9d3d129e44c9ce3` | TAK |
| SYSTEM/APP/TOSAUDIO.PRG | 1454 | `e19b1d856393a5424229e5525d485de960910402faa0fc03fdaa815b36552730` | 1454 | `e19b1d856393a5424229e5525d485de960910402faa0fc03fdaa815b36552730` | TAK |
| SYSTEM/APP/T.PRG | 1454 | `e19b1d856393a5424229e5525d485de960910402faa0fc03fdaa815b36552730` | 1454 | `e19b1d856393a5424229e5525d485de960910402faa0fc03fdaa815b36552730` | TAK |
| SYSTEM/APP/ARKANOID.APP | 18448 | `5a8d89179fb55442a4352ae58342e1a5455321af8966c3bec03d18341deb4eb1` | 18448 | `5a8d89179fb55442a4352ae58342e1a5455321af8966c3bec03d18341deb4eb1` | TAK |

## ELF, z których powstały artefakty

| plik | SHA-256 |
|---|---|
| build/system/kernel.elf | `ac73ace2be14850ad8d7767afb68f7e2f83f47eb2a017139e1d291a7fd2eab06` |
| build/system/desktop_wm.elf | `eb333354fd270e6d8275e40bd4dcde397f4c78b2aedd7d1228f81d4d6c7b9119` |
| build/system/tos_audio.elf | `e4261f665bd35e200c7764e1a7c4efb3d7e078f36a8cc1e322ffe6418ad0f8ef` |
| build/system/tos_audio.elf | `e4261f665bd35e200c7764e1a7c4efb3d7e078f36a8cc1e322ffe6418ad0f8ef` |
| build/system/arkanoid.elf | `dba8b0346aafaa60153795786eb0fa619df1db3945d02fc29a3ed07b373a61af` |

## Uruchomienie

```
/Users/xxx/Documents/Deepseek_test/atari-030-port/hatari-tf536 --machine megast --cpulevel 3 --cpuclock 32 --memsize 8 --ttram 64 --tf536 on --graphics-card vofa --gfx-chip et4000 --mono --sound 44100 --ide-master work/audio-baseline/atos-audio-r13.img
```

## JSON

```json
{
  "tag": "r13",
  "created": "2026-10-11 17:57:52",
  "image": {
    "path": "work/audio-baseline/atos-audio-r13.img",
    "present": true,
    "size": 805306880,
    "mtime": "2026-10-11 17:57:52",
    "sha256": "8141a00ea5c022a8aac0f500d6ca26697b68dc98e3b6a2b1bd77d6b23d6944bc"
  },
  "hatari": "hatari-tf536",
  "command": "/Users/xxx/Documents/Deepseek_test/atari-030-port/hatari-tf536 --machine megast --cpulevel 3 --cpuclock 32 --memsize 8 --ttram 64 --tf536 on --graphics-card vofa --gfx-chip et4000 --mono --sound 44100 --ide-master work/audio-baseline/atos-audio-r13.img",
  "artefacts": [
    {
      "fat_path": "SYSTEM/KERNEL.SYS",
      "build": {
        "path": "build/system/KERNEL.SYS",
        "present": true,
        "size": 457734,
        "mtime": "2026-10-11 17:56:10",
        "sha256": "a59ce71dc0ac61f6a52b876495c7130ae6ea9ba05254411f07417ce1adcd41ad"
      },
      "elf": {
        "path": "build/system/kernel.elf",
        "present": true,
        "size": 539472,
        "mtime": "2026-10-11 17:56:10",
        "sha256": "ac73ace2be14850ad8d7767afb68f7e2f83f47eb2a017139e1d291a7fd2eab06"
      },
      "in_image": {
        "size": 457734,
        "sha256": "a59ce71dc0ac61f6a52b876495c7130ae6ea9ba05254411f07417ce1adcd41ad"
      },
      "match": true
    },
    {
      "fat_path": "SYSTEM/APP/DESKTOP.APP",
      "build": {
        "path": "build/system/DESKTOP.APP",
        "present": true,
        "size": 29876,
        "mtime": "2026-10-11 17:57:32",
        "sha256": "f6198730cb2f922960a699e7336add3eddc89a2462e01d2fd9d3d129e44c9ce3"
      },
      "elf": {
        "path": "build/system/desktop_wm.elf",
        "present": true,
        "size": 44564,
        "mtime": "2026-10-11 17:57:32",
        "sha256": "eb333354fd270e6d8275e40bd4dcde397f4c78b2aedd7d1228f81d4d6c7b9119"
      },
      "in_image": {
        "size": 29876,
        "sha256": "f6198730cb2f922960a699e7336add3eddc89a2462e01d2fd9d3d129e44c9ce3"
      },
      "match": true
    },
    {
      "fat_path": "SYSTEM/APP/TOSAUDIO.PRG",
      "build": {
        "path": "build/system/TOSAUDIO.PRG",
        "present": true,
        "size": 1454,
        "mtime": "2026-10-11 17:55:12",
        "sha256": "e19b1d856393a5424229e5525d485de960910402faa0fc03fdaa815b36552730"
      },
      "elf": {
        "path": "build/system/tos_audio.elf",
        "present": true,
        "size": 10584,
        "mtime": "2026-10-11 17:55:12",
        "sha256": "e4261f665bd35e200c7764e1a7c4efb3d7e078f36a8cc1e322ffe6418ad0f8ef"
      },
      "in_image": {
        "size": 1454,
        "sha256": "e19b1d856393a5424229e5525d485de960910402faa0fc03fdaa815b36552730"
      },
      "match": true
    },
    {
      "fat_path": "SYSTEM/APP/T.PRG",
      "build": {
        "path": "build/system/TOSAUDIO.PRG",
        "present": true,
        "size": 1454,
        "mtime": "2026-10-11 17:55:12",
        "sha256": "e19b1d856393a5424229e5525d485de960910402faa0fc03fdaa815b36552730"
      },
      "elf": {
        "path": "build/system/tos_audio.elf",
        "present": true,
        "size": 10584,
        "mtime": "2026-10-11 17:55:12",
        "sha256": "e4261f665bd35e200c7764e1a7c4efb3d7e078f36a8cc1e322ffe6418ad0f8ef"
      },
      "in_image": {
        "size": 1454,
        "sha256": "e19b1d856393a5424229e5525d485de960910402faa0fc03fdaa815b36552730"
      },
      "match": true
    },
    {
      "fat_path": "SYSTEM/APP/ARKANOID.APP",
      "build": {
        "path": "build/system/ARKANOID.APP",
        "present": true,
        "size": 18448,
        "mtime": "2026-10-11 17:56:12",
        "sha256": "5a8d89179fb55442a4352ae58342e1a5455321af8966c3bec03d18341deb4eb1"
      },
      "elf": {
        "path": "build/system/arkanoid.elf",
        "present": true,
        "size": 29924,
        "mtime": "2026-10-11 17:56:11",
        "sha256": "dba8b0346aafaa60153795786eb0fa619df1db3945d02fc29a3ed07b373a61af"
      },
      "in_image": {
        "size": 18448,
        "sha256": "5a8d89179fb55442a4352ae58342e1a5455321af8966c3bec03d18341deb4eb1"
      },
      "match": true
    }
  ],
  "fat_read": "odczytany przez tools/img_get.py"
}
```
