# Manifest audio — TREE

Data: 2026-10-12 11:56:12

## Obraz

| plik | rozmiar | SHA-256 |
|---|---|---|
| work/atos-tree-full.img | 805306880 | `3386255158bf833b53873568c0ff0b91d2e8862f788aebc577694ee3401f6dd6` |

Odczyt z FAT: odczytany przez tools/img_get.py

## Artefakty: co jest w obrazie vs co zbudowano

| plik w FAT | w obrazie | SHA-256 (obraz) | build | SHA-256 (build) | zgodne |
|---|---|---|---|---|---|
| SYSTEM/KERNEL.SYS | 458844 | `2c7aa606e001d6959900e9d4ee13b8d583ccc9f98d45b519295dd7dee2f236af` | 458844 | `2c7aa606e001d6959900e9d4ee13b8d583ccc9f98d45b519295dd7dee2f236af` | TAK |
| SYSTEM/APP/DESKTOP.APP | 29914 | `0ddecb997d60436345a7cfb923acb694b2511c7e2e1f71ce1738cc6f026f0cb0` | 29914 | `0ddecb997d60436345a7cfb923acb694b2511c7e2e1f71ce1738cc6f026f0cb0` | TAK |
| SYSTEM/APP/TOSAUDIO.PRG | 1558 | `454238e16b2f4e7e943a20d1cc392a31fc4074f5ad71342b104df70d0bf13efe` | 1558 | `454238e16b2f4e7e943a20d1cc392a31fc4074f5ad71342b104df70d0bf13efe` | TAK |
| SYSTEM/APP/T.PRG | 1558 | `454238e16b2f4e7e943a20d1cc392a31fc4074f5ad71342b104df70d0bf13efe` | 1558 | `454238e16b2f4e7e943a20d1cc392a31fc4074f5ad71342b104df70d0bf13efe` | TAK |
| SYSTEM/APP/ARKANOID.APP | 18534 | `f42bf95ccaa76ae3a7cf57995cac67d6c8a11bec6d66390ffd8edc896bf8a9b9` | 18534 | `f42bf95ccaa76ae3a7cf57995cac67d6c8a11bec6d66390ffd8edc896bf8a9b9` | TAK |

## ELF, z których powstały artefakty

| plik | SHA-256 |
|---|---|
| build/system/kernel.elf | `5ff88651d65a2fe58390bceb8442ac4d9631598c41080ac09db622443c892d60` |
| build/system/desktop_wm.elf | `0eae0b6863f936343a696b87df5088d06bcc32175e16e0417651c601c3353d46` |
| build/system/tos_audio.elf | `2881a72a587e2bda7cb581423a624fc8ffc78d2436be92e934b19acae930a51d` |
| build/system/tos_audio.elf | `2881a72a587e2bda7cb581423a624fc8ffc78d2436be92e934b19acae930a51d` |
| build/system/arkanoid.elf | `417be039b39b7b406c6904e6af428081daf613048dda5f904810d53fb5631f34` |

## Uruchomienie

```
/Users/xxx/Documents/Deepseek_test/atari-030-port/hatari-tf536
```

## JSON

```json
{
  "tag": "TREE",
  "created": "2026-10-12 11:56:12",
  "image": {
    "path": "work/atos-tree-full.img",
    "present": true,
    "size": 805306880,
    "mtime": "2026-10-12 11:56:12",
    "sha256": "3386255158bf833b53873568c0ff0b91d2e8862f788aebc577694ee3401f6dd6"
  },
  "hatari": "hatari-tf536",
  "command": "/Users/xxx/Documents/Deepseek_test/atari-030-port/hatari-tf536",
  "artefacts": [
    {
      "fat_path": "SYSTEM/KERNEL.SYS",
      "build": {
        "path": "build/system/KERNEL.SYS",
        "present": true,
        "size": 458844,
        "mtime": "2026-10-12 10:54:13",
        "sha256": "2c7aa606e001d6959900e9d4ee13b8d583ccc9f98d45b519295dd7dee2f236af"
      },
      "elf": {
        "path": "build/system/kernel.elf",
        "present": true,
        "size": 540852,
        "mtime": "2026-10-12 10:54:13",
        "sha256": "5ff88651d65a2fe58390bceb8442ac4d9631598c41080ac09db622443c892d60"
      },
      "in_image": {
        "size": 458844,
        "sha256": "2c7aa606e001d6959900e9d4ee13b8d583ccc9f98d45b519295dd7dee2f236af"
      },
      "match": true
    },
    {
      "fat_path": "SYSTEM/APP/DESKTOP.APP",
      "build": {
        "path": "build/system/DESKTOP.APP",
        "present": true,
        "size": 29914,
        "mtime": "2026-10-12 10:54:13",
        "sha256": "0ddecb997d60436345a7cfb923acb694b2511c7e2e1f71ce1738cc6f026f0cb0"
      },
      "elf": {
        "path": "build/system/desktop_wm.elf",
        "present": true,
        "size": 44604,
        "mtime": "2026-10-12 10:54:13",
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
        "size": 1558,
        "mtime": "2026-10-11 19:55:46",
        "sha256": "454238e16b2f4e7e943a20d1cc392a31fc4074f5ad71342b104df70d0bf13efe"
      },
      "elf": {
        "path": "build/system/tos_audio.elf",
        "present": true,
        "size": 10688,
        "mtime": "2026-10-11 19:55:46",
        "sha256": "2881a72a587e2bda7cb581423a624fc8ffc78d2436be92e934b19acae930a51d"
      },
      "in_image": {
        "size": 1558,
        "sha256": "454238e16b2f4e7e943a20d1cc392a31fc4074f5ad71342b104df70d0bf13efe"
      },
      "match": true
    },
    {
      "fat_path": "SYSTEM/APP/T.PRG",
      "build": {
        "path": "build/system/TOSAUDIO.PRG",
        "present": true,
        "size": 1558,
        "mtime": "2026-10-11 19:55:46",
        "sha256": "454238e16b2f4e7e943a20d1cc392a31fc4074f5ad71342b104df70d0bf13efe"
      },
      "elf": {
        "path": "build/system/tos_audio.elf",
        "present": true,
        "size": 10688,
        "mtime": "2026-10-11 19:55:46",
        "sha256": "2881a72a587e2bda7cb581423a624fc8ffc78d2436be92e934b19acae930a51d"
      },
      "in_image": {
        "size": 1558,
        "sha256": "454238e16b2f4e7e943a20d1cc392a31fc4074f5ad71342b104df70d0bf13efe"
      },
      "match": true
    },
    {
      "fat_path": "SYSTEM/APP/ARKANOID.APP",
      "build": {
        "path": "build/system/ARKANOID.APP",
        "present": true,
        "size": 18534,
        "mtime": "2026-10-12 10:54:13",
        "sha256": "f42bf95ccaa76ae3a7cf57995cac67d6c8a11bec6d66390ffd8edc896bf8a9b9"
      },
      "elf": {
        "path": "build/system/arkanoid.elf",
        "present": true,
        "size": 30012,
        "mtime": "2026-10-12 10:54:13",
        "sha256": "417be039b39b7b406c6904e6af428081daf613048dda5f904810d53fb5631f34"
      },
      "in_image": {
        "size": 18534,
        "sha256": "f42bf95ccaa76ae3a7cf57995cac67d6c8a11bec6d66390ffd8edc896bf8a9b9"
      },
      "match": true
    }
  ],
  "fat_read": "odczytany przez tools/img_get.py"
}
```
