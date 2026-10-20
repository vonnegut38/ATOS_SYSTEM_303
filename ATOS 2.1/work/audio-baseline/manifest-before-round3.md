# Manifest audio — before-round3

Data: 2026-10-11 10:51:01

## Obraz

| plik | rozmiar | SHA-256 |
|---|---|---|
| work/audio-baseline/atos-audio-test.img | 805306880 | `581855d08d420fd2191f1d079b0672e3701e1e2d824f0d4422a0f8b4078d6e93` |

## Artefakty na obrazie i ich źródła (ELF)

| plik na obrazie | rozmiar | SHA-256 | ELF | SHA-256 ELF |
|---|---|---|---|---|
| build/system/KERNEL.SYS | 456726 | `4fc6ccf0a044731281fdaafbc67332d4daedbff64ebe8e6b44296c11b5befe19` | build/system/kernel.elf | `28ec23bbfb80182465393eeae47f669e2972799d273e83d2432e1dd0f4655c88` |
| build/bios_mste.bin | 262144 | `e128088ddad9575cec280505e0b24364958b161beec4426896b69beb8d575e90` | build/bios_mste.elf | `6fddbf3d04c18c493416c5e84780b32ccec3dda78e707654b988e418b0f793e8` |
| build/system/TOSAUDIO.PRG | 614 | `66deb7cba7216e9bf7e4dcd39c6204e6348ef262c37a7c05b7f49632654e3201` | build/system/tos_audio.elf | `975ce55c5b32271ddd8e763f070a3f0ac07cb37d25b1adc3ac13fcb2c7efefd2` |

## Uruchomienie

```
/Users/xxx/Documents/Deepseek_test/atari-030-port/hatari-tf536
```

## JSON

```json
{
  "tag": "before-round3",
  "created": "2026-10-11 10:51:01",
  "image": {
    "path": "work/audio-baseline/atos-audio-test.img",
    "present": true,
    "size": 805306880,
    "mtime": "2026-10-10 23:21:05",
    "sha256": "581855d08d420fd2191f1d079b0672e3701e1e2d824f0d4422a0f8b4078d6e93"
  },
  "hatari": "hatari-tf536",
  "command": "/Users/xxx/Documents/Deepseek_test/atari-030-port/hatari-tf536",
  "artefacts": [
    {
      "image_file": {
        "path": "build/system/KERNEL.SYS",
        "present": true,
        "size": 456726,
        "mtime": "2026-10-11 10:49:34",
        "sha256": "4fc6ccf0a044731281fdaafbc67332d4daedbff64ebe8e6b44296c11b5befe19"
      },
      "elf": {
        "path": "build/system/kernel.elf",
        "present": true,
        "size": 537988,
        "mtime": "2026-10-11 10:49:34",
        "sha256": "28ec23bbfb80182465393eeae47f669e2972799d273e83d2432e1dd0f4655c88"
      }
    },
    {
      "image_file": {
        "path": "build/bios_mste.bin",
        "present": true,
        "size": 262144,
        "mtime": "2026-10-11 10:46:00",
        "sha256": "e128088ddad9575cec280505e0b24364958b161beec4426896b69beb8d575e90"
      },
      "elf": {
        "path": "build/bios_mste.elf",
        "present": true,
        "size": 72672,
        "mtime": "2026-10-11 10:45:58",
        "sha256": "6fddbf3d04c18c493416c5e84780b32ccec3dda78e707654b988e418b0f793e8"
      }
    },
    {
      "image_file": {
        "path": "build/system/TOSAUDIO.PRG",
        "present": true,
        "size": 614,
        "mtime": "2026-10-10 23:15:39",
        "sha256": "66deb7cba7216e9bf7e4dcd39c6204e6348ef262c37a7c05b7f49632654e3201"
      },
      "elf": {
        "path": "build/system/tos_audio.elf",
        "present": true,
        "size": 9420,
        "mtime": "2026-10-10 23:15:39",
        "sha256": "975ce55c5b32271ddd8e763f070a3f0ac07cb37d25b1adc3ac13fcb2c7efefd2"
      }
    }
  ]
}
```
