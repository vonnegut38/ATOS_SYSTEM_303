# Manifest audio — round5-fault

Data: 2026-10-11 11:31:45

## Obraz

| plik | rozmiar | SHA-256 |
|---|---|---|
| work/audio-baseline/atos-audio-round3.img | 805306880 | `d30935140def7f9ab070ec3e659d54488b04b6fe84e22d769d23db0b1a85af20` |

## Artefakty na obrazie i ich źródła (ELF)

| plik na obrazie | rozmiar | SHA-256 | ELF | SHA-256 ELF |
|---|---|---|---|---|
| build/system/KERNEL.SYS | 456758 | `2cb341a1098a59546445966c947076d25420a665a140cf98cd66ea5be800946f` | build/system/kernel.elf | `a03d8d2b631c51d2af1aad938b3e063657b8883f6d3ec27b05e0bf301518beea` |
| build/bios_mste.bin | 262144 | `e128088ddad9575cec280505e0b24364958b161beec4426896b69beb8d575e90` | build/bios_mste.elf | `6fddbf3d04c18c493416c5e84780b32ccec3dda78e707654b988e418b0f793e8` |
| build/system/TOSAUDIO.PRG | 1398 | `f1d18282949fcd3871a8dd89080c4dd8dda3822a24e209d9e1a550b8a593d2fd` | build/system/tos_audio.elf | `8b7d9cdc2877f09ba78afdf611d18346601572bb9b64c92fcbca94e9e7267813` |

## Uruchomienie

```
/Users/xxx/Documents/Deepseek_test/atari-030-port/hatari-tf536
```

## JSON

```json
{
  "tag": "round5-fault",
  "created": "2026-10-11 11:31:45",
  "image": {
    "path": "work/audio-baseline/atos-audio-round3.img",
    "present": true,
    "size": 805306880,
    "mtime": "2026-10-11 11:31:45",
    "sha256": "d30935140def7f9ab070ec3e659d54488b04b6fe84e22d769d23db0b1a85af20"
  },
  "hatari": "hatari-tf536",
  "command": "/Users/xxx/Documents/Deepseek_test/atari-030-port/hatari-tf536",
  "artefacts": [
    {
      "image_file": {
        "path": "build/system/KERNEL.SYS",
        "present": true,
        "size": 456758,
        "mtime": "2026-10-11 11:31:34",
        "sha256": "2cb341a1098a59546445966c947076d25420a665a140cf98cd66ea5be800946f"
      },
      "elf": {
        "path": "build/system/kernel.elf",
        "present": true,
        "size": 538088,
        "mtime": "2026-10-11 11:31:34",
        "sha256": "a03d8d2b631c51d2af1aad938b3e063657b8883f6d3ec27b05e0bf301518beea"
      }
    },
    {
      "image_file": {
        "path": "build/bios_mste.bin",
        "present": true,
        "size": 262144,
        "mtime": "2026-10-11 11:31:36",
        "sha256": "e128088ddad9575cec280505e0b24364958b161beec4426896b69beb8d575e90"
      },
      "elf": {
        "path": "build/bios_mste.elf",
        "present": true,
        "size": 72672,
        "mtime": "2026-10-11 11:31:34",
        "sha256": "6fddbf3d04c18c493416c5e84780b32ccec3dda78e707654b988e418b0f793e8"
      }
    },
    {
      "image_file": {
        "path": "build/system/TOSAUDIO.PRG",
        "present": true,
        "size": 1398,
        "mtime": "2026-10-11 11:31:31",
        "sha256": "f1d18282949fcd3871a8dd89080c4dd8dda3822a24e209d9e1a550b8a593d2fd"
      },
      "elf": {
        "path": "build/system/tos_audio.elf",
        "present": true,
        "size": 10540,
        "mtime": "2026-10-11 11:31:31",
        "sha256": "8b7d9cdc2877f09ba78afdf611d18346601572bb9b64c92fcbca94e9e7267813"
      }
    }
  ]
}
```
