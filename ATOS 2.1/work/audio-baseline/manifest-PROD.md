# Manifest audio — PROD-final

Data: 2026-10-11 19:03:53

## Obraz

| plik | rozmiar | SHA-256 |
|---|---|---|
| work/audio-baseline/atos-audio-prod.img | 805306880 | `364639a1d93dc7bb4b4d32fa0c911e781960c8b5e9f68e34a9113045726739df` |

Odczyt z FAT: odczytany przez tools/img_get.py

## Artefakty: co jest w obrazie vs co zbudowano

| plik w FAT | w obrazie | SHA-256 (obraz) | build | SHA-256 (build) | zgodne |
|---|---|---|---|---|---|
| SYSTEM/KERNEL.SYS | 457762 | `7ae8901a118ed2afb005b08e453d79bafbc688228b20ec6b769382713af582d4` | 457762 | `7ae8901a118ed2afb005b08e453d79bafbc688228b20ec6b769382713af582d4` | TAK |
| SYSTEM/APP/DESKTOP.APP | 29914 | `0ddecb997d60436345a7cfb923acb694b2511c7e2e1f71ce1738cc6f026f0cb0` | 29914 | `0ddecb997d60436345a7cfb923acb694b2511c7e2e1f71ce1738cc6f026f0cb0` | TAK |
| SYSTEM/APP/TOSAUDIO.PRG | 1442 | `071c4b094e48838784d2ec378edbf3689c723428796715d7ec6c86108b304c7f` | 1442 | `071c4b094e48838784d2ec378edbf3689c723428796715d7ec6c86108b304c7f` | TAK |
| SYSTEM/APP/T.PRG | 1442 | `071c4b094e48838784d2ec378edbf3689c723428796715d7ec6c86108b304c7f` | 1442 | `071c4b094e48838784d2ec378edbf3689c723428796715d7ec6c86108b304c7f` | TAK |
| SYSTEM/APP/ARKANOID.APP | 18448 | `5a8d89179fb55442a4352ae58342e1a5455321af8966c3bec03d18341deb4eb1` | 18448 | `5a8d89179fb55442a4352ae58342e1a5455321af8966c3bec03d18341deb4eb1` | TAK |

## ELF, z których powstały artefakty

| plik | SHA-256 |
|---|---|
| build/system/kernel.elf | `22cf56aa5e8bd6c39f0b991402f0ff6f4d1aa52ced0039c89af415a6fd9af075` |
| build/system/desktop_wm.elf | `0eae0b6863f936343a696b87df5088d06bcc32175e16e0417651c601c3353d46` |
| build/system/tos_audio.elf | `66dc9849923d56db764e6ca722d3614e209536702057f5f84c7b4f86581d20aa` |
| build/system/tos_audio.elf | `66dc9849923d56db764e6ca722d3614e209536702057f5f84c7b4f86581d20aa` |
| build/system/arkanoid.elf | `dba8b0346aafaa60153795786eb0fa619df1db3945d02fc29a3ed07b373a61af` |

## Uruchomienie

```
/Users/xxx/Documents/Deepseek_test/atari-030-port/hatari-tf536 --machine megast --cpulevel 3 --cpuclock 32 --memsize 8 --ttram 64 --tf536 on --graphics-card vofa --gfx-chip et4000 --mono --sound 44100 --ide-master work/audio-baseline/atos-audio-prod.img
```

## JSON

```json
{
  "tag": "PROD-final",
  "created": "2026-10-11 19:03:53",
  "image": {
    "path": "work/audio-baseline/atos-audio-prod.img",
    "present": true,
    "size": 805306880,
    "mtime": "2026-10-11 19:03:53",
    "sha256": "364639a1d93dc7bb4b4d32fa0c911e781960c8b5e9f68e34a9113045726739df"
  },
  "hatari": "hatari-tf536",
  "command": "/Users/xxx/Documents/Deepseek_test/atari-030-port/hatari-tf536 --machine megast --cpulevel 3 --cpuclock 32 --memsize 8 --ttram 64 --tf536 on --graphics-card vofa --gfx-chip et4000 --mono --sound 44100 --ide-master work/audio-baseline/atos-audio-prod.img",
  "artefacts": [
    {
      "fat_path": "SYSTEM/KERNEL.SYS",
      "build": {
        "path": "build/system/KERNEL.SYS",
        "present": true,
        "size": 457762,
        "mtime": "2026-10-11 19:03:46",
        "sha256": "7ae8901a118ed2afb005b08e453d79bafbc688228b20ec6b769382713af582d4"
      },
      "elf": {
        "path": "build/system/kernel.elf",
        "present": true,
        "size": 539496,
        "mtime": "2026-10-11 19:03:46",
        "sha256": "22cf56aa5e8bd6c39f0b991402f0ff6f4d1aa52ced0039c89af415a6fd9af075"
      },
      "in_image": {
        "size": 457762,
        "sha256": "7ae8901a118ed2afb005b08e453d79bafbc688228b20ec6b769382713af582d4"
      },
      "match": true
    },
    {
      "fat_path": "SYSTEM/APP/DESKTOP.APP",
      "build": {
        "path": "build/system/DESKTOP.APP",
        "present": true,
        "size": 29914,
        "mtime": "2026-10-11 19:03:47",
        "sha256": "0ddecb997d60436345a7cfb923acb694b2511c7e2e1f71ce1738cc6f026f0cb0"
      },
      "elf": {
        "path": "build/system/desktop_wm.elf",
        "present": true,
        "size": 44604,
        "mtime": "2026-10-11 19:03:47",
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
        "size": 1442,
        "mtime": "2026-10-11 18:42:37",
        "sha256": "071c4b094e48838784d2ec378edbf3689c723428796715d7ec6c86108b304c7f"
      },
      "elf": {
        "path": "build/system/tos_audio.elf",
        "present": true,
        "size": 10572,
        "mtime": "2026-10-11 18:42:37",
        "sha256": "66dc9849923d56db764e6ca722d3614e209536702057f5f84c7b4f86581d20aa"
      },
      "in_image": {
        "size": 1442,
        "sha256": "071c4b094e48838784d2ec378edbf3689c723428796715d7ec6c86108b304c7f"
      },
      "match": true
    },
    {
      "fat_path": "SYSTEM/APP/T.PRG",
      "build": {
        "path": "build/system/TOSAUDIO.PRG",
        "present": true,
        "size": 1442,
        "mtime": "2026-10-11 18:42:37",
        "sha256": "071c4b094e48838784d2ec378edbf3689c723428796715d7ec6c86108b304c7f"
      },
      "elf": {
        "path": "build/system/tos_audio.elf",
        "present": true,
        "size": 10572,
        "mtime": "2026-10-11 18:42:37",
        "sha256": "66dc9849923d56db764e6ca722d3614e209536702057f5f84c7b4f86581d20aa"
      },
      "in_image": {
        "size": 1442,
        "sha256": "071c4b094e48838784d2ec378edbf3689c723428796715d7ec6c86108b304c7f"
      },
      "match": true
    },
    {
      "fat_path": "SYSTEM/APP/ARKANOID.APP",
      "build": {
        "path": "build/system/ARKANOID.APP",
        "present": true,
        "size": 18448,
        "mtime": "2026-10-11 19:03:47",
        "sha256": "5a8d89179fb55442a4352ae58342e1a5455321af8966c3bec03d18341deb4eb1"
      },
      "elf": {
        "path": "build/system/arkanoid.elf",
        "present": true,
        "size": 29924,
        "mtime": "2026-10-11 19:03:47",
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
