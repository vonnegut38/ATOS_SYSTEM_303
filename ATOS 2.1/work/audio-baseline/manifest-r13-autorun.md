# Manifest audio — r13-autorun

Data: 2026-10-11 18:34:06

## Obraz

| plik | rozmiar | SHA-256 |
|---|---|---|
| work/audio-baseline/atos-audio-r13.img | 805306880 | `24dbe321fa3018740269c73da4f81050273b80c4d388fddfeec44f39a809e528` |

Odczyt z FAT: odczytany przez tools/img_get.py

## Artefakty: co jest w obrazie vs co zbudowano

| plik w FAT | w obrazie | SHA-256 (obraz) | build | SHA-256 (build) | zgodne |
|---|---|---|---|---|---|
| SYSTEM/KERNEL.SYS | 457816 | `93bbb14461388b24a3edffb6bf6de1a02e6c0ada40736d239e230d817ff2cd3f` | 457816 | `93bbb14461388b24a3edffb6bf6de1a02e6c0ada40736d239e230d817ff2cd3f` | TAK |
| SYSTEM/APP/DESKTOP.APP | 29914 | `0ddecb997d60436345a7cfb923acb694b2511c7e2e1f71ce1738cc6f026f0cb0` | 29914 | `0ddecb997d60436345a7cfb923acb694b2511c7e2e1f71ce1738cc6f026f0cb0` | TAK |
| SYSTEM/APP/TOSAUDIO.PRG | 1454 | `e19b1d856393a5424229e5525d485de960910402faa0fc03fdaa815b36552730` | 1454 | `e19b1d856393a5424229e5525d485de960910402faa0fc03fdaa815b36552730` | TAK |
| SYSTEM/APP/T.PRG | 1454 | `e19b1d856393a5424229e5525d485de960910402faa0fc03fdaa815b36552730` | 1454 | `e19b1d856393a5424229e5525d485de960910402faa0fc03fdaa815b36552730` | TAK |
| SYSTEM/APP/ARKANOID.APP | 18448 | `5a8d89179fb55442a4352ae58342e1a5455321af8966c3bec03d18341deb4eb1` | 18448 | `5a8d89179fb55442a4352ae58342e1a5455321af8966c3bec03d18341deb4eb1` | TAK |

## ELF, z których powstały artefakty

| plik | SHA-256 |
|---|---|
| build/system/kernel.elf | `05d3d5af9f722664dfe874118e68c3f4da649b1a5fe4fdf2c04aa8677132a62e` |
| build/system/desktop_wm.elf | `0eae0b6863f936343a696b87df5088d06bcc32175e16e0417651c601c3353d46` |
| build/system/tos_audio.elf | `e4261f665bd35e200c7764e1a7c4efb3d7e078f36a8cc1e322ffe6418ad0f8ef` |
| build/system/tos_audio.elf | `e4261f665bd35e200c7764e1a7c4efb3d7e078f36a8cc1e322ffe6418ad0f8ef` |
| build/system/arkanoid.elf | `dba8b0346aafaa60153795786eb0fa619df1db3945d02fc29a3ed07b373a61af` |

## Uruchomienie

```
/Users/xxx/Documents/Deepseek_test/atari-030-port/hatari-tf536
```

## JSON

```json
{
  "tag": "r13-autorun",
  "created": "2026-10-11 18:34:06",
  "image": {
    "path": "work/audio-baseline/atos-audio-r13.img",
    "present": true,
    "size": 805306880,
    "mtime": "2026-10-11 18:34:05",
    "sha256": "24dbe321fa3018740269c73da4f81050273b80c4d388fddfeec44f39a809e528"
  },
  "hatari": "hatari-tf536",
  "command": "/Users/xxx/Documents/Deepseek_test/atari-030-port/hatari-tf536",
  "artefacts": [
    {
      "fat_path": "SYSTEM/KERNEL.SYS",
      "build": {
        "path": "build/system/KERNEL.SYS",
        "present": true,
        "size": 457816,
        "mtime": "2026-10-11 18:33:59",
        "sha256": "93bbb14461388b24a3edffb6bf6de1a02e6c0ada40736d239e230d817ff2cd3f"
      },
      "elf": {
        "path": "build/system/kernel.elf",
        "present": true,
        "size": 539608,
        "mtime": "2026-10-11 18:33:59",
        "sha256": "05d3d5af9f722664dfe874118e68c3f4da649b1a5fe4fdf2c04aa8677132a62e"
      },
      "in_image": {
        "size": 457816,
        "sha256": "93bbb14461388b24a3edffb6bf6de1a02e6c0ada40736d239e230d817ff2cd3f"
      },
      "match": true
    },
    {
      "fat_path": "SYSTEM/APP/DESKTOP.APP",
      "build": {
        "path": "build/system/DESKTOP.APP",
        "present": true,
        "size": 29914,
        "mtime": "2026-10-11 18:34:00",
        "sha256": "0ddecb997d60436345a7cfb923acb694b2511c7e2e1f71ce1738cc6f026f0cb0"
      },
      "elf": {
        "path": "build/system/desktop_wm.elf",
        "present": true,
        "size": 44604,
        "mtime": "2026-10-11 18:34:00",
        "sha256": "0eae0b6863f936343a696b87df5088d06bcc32175e16e0417651c601c3353d46"
      },
      "in_image": {
        "size": 29914,
        "sha256": "0ddecb997d60436345a7cfb923acb694b2511c7e2e1f71ce1738cc6f026f0cb0"
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
        "mtime": "2026-10-11 18:34:00",
        "sha256": "5a8d89179fb55442a4352ae58342e1a5455321af8966c3bec03d18341deb4eb1"
      },
      "elf": {
        "path": "build/system/arkanoid.elf",
        "present": true,
        "size": 29924,
        "mtime": "2026-10-11 18:34:00",
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
