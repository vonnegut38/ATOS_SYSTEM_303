# Manifest audio — r13-final

Data: 2026-10-11 18:46:21

## Obraz

| plik | rozmiar | SHA-256 |
|---|---|---|
| work/audio-baseline/atos-audio-r13.img | 805306880 | `5d90b35885060cd54b78c7b1fbee1647eaa18f03293f57f4be2e0134dbc86c16` |

Odczyt z FAT: odczytany przez tools/img_get.py

## Artefakty: co jest w obrazie vs co zbudowano

| plik w FAT | w obrazie | SHA-256 (obraz) | build | SHA-256 (build) | zgodne |
|---|---|---|---|---|---|
| SYSTEM/KERNEL.SYS | 457816 | `93bbb14461388b24a3edffb6bf6de1a02e6c0ada40736d239e230d817ff2cd3f` | 457816 | `93bbb14461388b24a3edffb6bf6de1a02e6c0ada40736d239e230d817ff2cd3f` | TAK |
| SYSTEM/APP/DESKTOP.APP | 29914 | `0ddecb997d60436345a7cfb923acb694b2511c7e2e1f71ce1738cc6f026f0cb0` | 29914 | `0ddecb997d60436345a7cfb923acb694b2511c7e2e1f71ce1738cc6f026f0cb0` | TAK |
| SYSTEM/APP/TOSAUDIO.PRG | 1442 | `071c4b094e48838784d2ec378edbf3689c723428796715d7ec6c86108b304c7f` | 1442 | `071c4b094e48838784d2ec378edbf3689c723428796715d7ec6c86108b304c7f` | TAK |
| SYSTEM/APP/T.PRG | 1442 | `071c4b094e48838784d2ec378edbf3689c723428796715d7ec6c86108b304c7f` | 1442 | `071c4b094e48838784d2ec378edbf3689c723428796715d7ec6c86108b304c7f` | TAK |
| SYSTEM/APP/ARKANOID.APP | 18448 | `5a8d89179fb55442a4352ae58342e1a5455321af8966c3bec03d18341deb4eb1` | 18448 | `5a8d89179fb55442a4352ae58342e1a5455321af8966c3bec03d18341deb4eb1` | TAK |

## ELF, z których powstały artefakty

| plik | SHA-256 |
|---|---|
| build/system/kernel.elf | `05d3d5af9f722664dfe874118e68c3f4da649b1a5fe4fdf2c04aa8677132a62e` |
| build/system/desktop_wm.elf | `0eae0b6863f936343a696b87df5088d06bcc32175e16e0417651c601c3353d46` |
| build/system/tos_audio.elf | `66dc9849923d56db764e6ca722d3614e209536702057f5f84c7b4f86581d20aa` |
| build/system/tos_audio.elf | `66dc9849923d56db764e6ca722d3614e209536702057f5f84c7b4f86581d20aa` |
| build/system/arkanoid.elf | `dba8b0346aafaa60153795786eb0fa619df1db3945d02fc29a3ed07b373a61af` |

## Uruchomienie

```
/Users/xxx/Documents/Deepseek_test/atari-030-port/hatari-tf536
```

## JSON

```json
{
  "tag": "r13-final",
  "created": "2026-10-11 18:46:21",
  "image": {
    "path": "work/audio-baseline/atos-audio-r13.img",
    "present": true,
    "size": 805306880,
    "mtime": "2026-10-11 18:46:21",
    "sha256": "5d90b35885060cd54b78c7b1fbee1647eaa18f03293f57f4be2e0134dbc86c16"
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
