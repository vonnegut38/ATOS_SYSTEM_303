# Manifest audio — round3-autorun

Data: 2026-10-11 10:58:27

## Obraz

| plik | rozmiar | SHA-256 |
|---|---|---|
| work/audio-baseline/atos-audio-round3.img | 805306880 | `9d57e6cdd88234fb61262cca84531cf3f106ab064399334e83d8284d9497122a` |

## Artefakty na obrazie i ich źródła (ELF)

| plik na obrazie | rozmiar | SHA-256 | ELF | SHA-256 ELF |
|---|---|---|---|---|
| build/system/KERNEL.SYS | 456726 | `4fc6ccf0a044731281fdaafbc67332d4daedbff64ebe8e6b44296c11b5befe19` | build/system/kernel.elf | `28ec23bbfb80182465393eeae47f669e2972799d273e83d2432e1dd0f4655c88` |
| build/bios_mste.bin | 262144 | `e128088ddad9575cec280505e0b24364958b161beec4426896b69beb8d575e90` | build/bios_mste.elf | `6fddbf3d04c18c493416c5e84780b32ccec3dda78e707654b988e418b0f793e8` |
| build/system/TOSAUDIO.PRG | 1058 | `e8cfe44e0123592529a4f237dd8f7047e7fa18dd6bdd619b90a54f3c27cf7bcd` | build/system/tos_audio.elf | `98bcc79a7607e9dbb7340c830c129808946434ebbe1faeb6cf5c0b34e6b6d9d5` |

## Uruchomienie

```
/Users/xxx/Documents/Deepseek_test/atari-030-port/hatari-tf536
```

## JSON

```json
{
  "tag": "round3-autorun",
  "created": "2026-10-11 10:58:27",
  "image": {
    "path": "work/audio-baseline/atos-audio-round3.img",
    "present": true,
    "size": 805306880,
    "mtime": "2026-10-11 10:58:27",
    "sha256": "9d57e6cdd88234fb61262cca84531cf3f106ab064399334e83d8284d9497122a"
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
        "size": 1058,
        "mtime": "2026-10-11 10:51:57",
        "sha256": "e8cfe44e0123592529a4f237dd8f7047e7fa18dd6bdd619b90a54f3c27cf7bcd"
      },
      "elf": {
        "path": "build/system/tos_audio.elf",
        "present": true,
        "size": 10148,
        "mtime": "2026-10-11 10:51:57",
        "sha256": "98bcc79a7607e9dbb7340c830c129808946434ebbe1faeb6cf5c0b34e6b6d9d5"
      }
    }
  ]
}
```
