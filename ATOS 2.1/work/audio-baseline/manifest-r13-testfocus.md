# Manifest audio — r13-testfocus

Data: 2026-10-11 18:23:52

## Obraz

| plik | rozmiar | SHA-256 |
|---|---|---|
| work/audio-baseline/atos-audio-r13.img | 805306880 | `821c38e34af33def1bab69f2db73ab4e903553396e6f017f95f6be8737c7b685` |

Odczyt z FAT: odczytany przez tools/img_get.py

## Artefakty: co jest w obrazie vs co zbudowano

| plik w FAT | w obrazie | SHA-256 (obraz) | build | SHA-256 (build) | zgodne |
|---|---|---|---|---|---|
| SYSTEM/KERNEL.SYS | 457774 | `3913fcb259a4327e18be256faf64394ddea4957f07c2f61e408496661af8302b` | 457774 | `3913fcb259a4327e18be256faf64394ddea4957f07c2f61e408496661af8302b` | TAK |
| SYSTEM/APP/DESKTOP.APP | 29914 | `0ddecb997d60436345a7cfb923acb694b2511c7e2e1f71ce1738cc6f026f0cb0` | 29914 | `0ddecb997d60436345a7cfb923acb694b2511c7e2e1f71ce1738cc6f026f0cb0` | TAK |
| SYSTEM/APP/TOSAUDIO.PRG | 1454 | `e19b1d856393a5424229e5525d485de960910402faa0fc03fdaa815b36552730` | 1454 | `e19b1d856393a5424229e5525d485de960910402faa0fc03fdaa815b36552730` | TAK |
| SYSTEM/APP/T.PRG | 1454 | `e19b1d856393a5424229e5525d485de960910402faa0fc03fdaa815b36552730` | 1454 | `e19b1d856393a5424229e5525d485de960910402faa0fc03fdaa815b36552730` | TAK |
| SYSTEM/APP/ARKANOID.APP | 18448 | `5a8d89179fb55442a4352ae58342e1a5455321af8966c3bec03d18341deb4eb1` | 18448 | `5a8d89179fb55442a4352ae58342e1a5455321af8966c3bec03d18341deb4eb1` | TAK |

## ELF, z których powstały artefakty

| plik | SHA-256 |
|---|---|
| build/system/kernel.elf | `3da4454c1e3e6401a4466291029b0f1cff7d9332649f315dc4b82b831ddf72c1` |
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
  "tag": "r13-testfocus",
  "created": "2026-10-11 18:23:52",
  "image": {
    "path": "work/audio-baseline/atos-audio-r13.img",
    "present": true,
    "size": 805306880,
    "mtime": "2026-10-11 18:23:52",
    "sha256": "821c38e34af33def1bab69f2db73ab4e903553396e6f017f95f6be8737c7b685"
  },
  "hatari": "hatari-tf536",
  "command": "/Users/xxx/Documents/Deepseek_test/atari-030-port/hatari-tf536",
  "artefacts": [
    {
      "fat_path": "SYSTEM/KERNEL.SYS",
      "build": {
        "path": "build/system/KERNEL.SYS",
        "present": true,
        "size": 457774,
        "mtime": "2026-10-11 18:23:45",
        "sha256": "3913fcb259a4327e18be256faf64394ddea4957f07c2f61e408496661af8302b"
      },
      "elf": {
        "path": "build/system/kernel.elf",
        "present": true,
        "size": 539540,
        "mtime": "2026-10-11 18:23:45",
        "sha256": "3da4454c1e3e6401a4466291029b0f1cff7d9332649f315dc4b82b831ddf72c1"
      },
      "in_image": {
        "size": 457774,
        "sha256": "3913fcb259a4327e18be256faf64394ddea4957f07c2f61e408496661af8302b"
      },
      "match": true
    },
    {
      "fat_path": "SYSTEM/APP/DESKTOP.APP",
      "build": {
        "path": "build/system/DESKTOP.APP",
        "present": true,
        "size": 29914,
        "mtime": "2026-10-11 18:23:47",
        "sha256": "0ddecb997d60436345a7cfb923acb694b2511c7e2e1f71ce1738cc6f026f0cb0"
      },
      "elf": {
        "path": "build/system/desktop_wm.elf",
        "present": true,
        "size": 44604,
        "mtime": "2026-10-11 18:23:47",
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
        "mtime": "2026-10-11 18:23:47",
        "sha256": "5a8d89179fb55442a4352ae58342e1a5455321af8966c3bec03d18341deb4eb1"
      },
      "elf": {
        "path": "build/system/arkanoid.elf",
        "present": true,
        "size": 29924,
        "mtime": "2026-10-11 18:23:47",
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
