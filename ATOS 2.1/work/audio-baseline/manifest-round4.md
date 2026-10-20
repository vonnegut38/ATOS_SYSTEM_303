# Manifest audio — round4-final

Data: 2026-10-11 11:30:36

## Obraz

| plik | rozmiar | SHA-256 |
|---|---|---|
| work/audio-baseline/atos-audio-round3.img | 805306880 | `83b5ca17e1e37acd31aa45ce2351ccdb71f80fbaf169cc0bd848e5d932b198e6` |

## Artefakty na obrazie i ich źródła (ELF)

| plik na obrazie | rozmiar | SHA-256 | ELF | SHA-256 ELF |
|---|---|---|---|---|
| build/system/KERNEL.SYS | 456750 | `a76f83e0b840f6529543c48c6e9181e89f101d203b930f8d52038eaf343230cf` | build/system/kernel.elf | `3759830cfbb1e36abf60cf0375354373127463777211b571c318ff1400b753b1` |
| build/bios_mste.bin | 262144 | `e128088ddad9575cec280505e0b24364958b161beec4426896b69beb8d575e90` | build/bios_mste.elf | `6fddbf3d04c18c493416c5e84780b32ccec3dda78e707654b988e418b0f793e8` |
| build/system/TOSAUDIO.PRG | 1398 | `691d69a511f4c8a6b86b479a335d2e92ba1ca23217219a9fc7dfe5b507004587` | build/system/tos_audio.elf | `f5dbfb9bb5325ffd9aa252e918c473b99de707679aeaad2e98c2fe716ef4d12b` |

## Uruchomienie

```
/Users/xxx/Documents/Deepseek_test/atari-030-port/hatari-tf536
```

## JSON

```json
{
  "tag": "round4-final",
  "created": "2026-10-11 11:30:36",
  "image": {
    "path": "work/audio-baseline/atos-audio-round3.img",
    "present": true,
    "size": 805306880,
    "mtime": "2026-10-11 11:29:06",
    "sha256": "83b5ca17e1e37acd31aa45ce2351ccdb71f80fbaf169cc0bd848e5d932b198e6"
  },
  "hatari": "hatari-tf536",
  "command": "/Users/xxx/Documents/Deepseek_test/atari-030-port/hatari-tf536",
  "artefacts": [
    {
      "image_file": {
        "path": "build/system/KERNEL.SYS",
        "present": true,
        "size": 456750,
        "mtime": "2026-10-11 11:24:55",
        "sha256": "a76f83e0b840f6529543c48c6e9181e89f101d203b930f8d52038eaf343230cf"
      },
      "elf": {
        "path": "build/system/kernel.elf",
        "present": true,
        "size": 538048,
        "mtime": "2026-10-11 11:24:55",
        "sha256": "3759830cfbb1e36abf60cf0375354373127463777211b571c318ff1400b753b1"
      }
    },
    {
      "image_file": {
        "path": "build/bios_mste.bin",
        "present": true,
        "size": 262144,
        "mtime": "2026-10-11 11:24:57",
        "sha256": "e128088ddad9575cec280505e0b24364958b161beec4426896b69beb8d575e90"
      },
      "elf": {
        "path": "build/bios_mste.elf",
        "present": true,
        "size": 72672,
        "mtime": "2026-10-11 11:24:55",
        "sha256": "6fddbf3d04c18c493416c5e84780b32ccec3dda78e707654b988e418b0f793e8"
      }
    },
    {
      "image_file": {
        "path": "build/system/TOSAUDIO.PRG",
        "present": true,
        "size": 1398,
        "mtime": "2026-10-11 11:28:51",
        "sha256": "691d69a511f4c8a6b86b479a335d2e92ba1ca23217219a9fc7dfe5b507004587"
      },
      "elf": {
        "path": "build/system/tos_audio.elf",
        "present": true,
        "size": 10540,
        "mtime": "2026-10-11 11:28:51",
        "sha256": "f5dbfb9bb5325ffd9aa252e918c473b99de707679aeaad2e98c2fe716ef4d12b"
      }
    }
  ]
}
```
