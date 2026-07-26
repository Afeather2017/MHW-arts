# MHGU Long Sword animation extraction

The Yuzu RomFS is already dumped at:

```text
E:\games\yuzu\user\dump\0100770008DD8000\romfs
```

## Extracted Long Sword files

The Long Sword combat archive is `arc/player/quest/w07.arc`. Its important
contents are under `extracted/w07`:

- `player/mot/w07/w07.lmt` - regular/style motions
- `player/mot/w07/w07_sa.lmt` - Hunter Art motions
- `effect/psl/player/mot/w07/w07_sa.psl` - Hunter Art effect schedule
- `player/hit/pl_we07_hitdata.hdp` - hitbox and damage data

## Standalone conversion (no 3ds Max)

Use the official current Revil Toolset:

```text
E:\MHW-arts\tools\RevilToolset-v2.10.201-win64\revil_toolset.exe
```

Convert an MHGU body model to a skeleton-bearing GLB:

```powershell
revil_toolset.exe mod_to_gltf m_body001.mod
```

`lmt_to_gltf` uses Spike batching. The GLB is the primary file and each LMT
is supplemental. Supplemental files currently need to be beside the GLB.
Use `sakura_batch.json`, then run:

```powershell
revil_toolset.exe lmt_to_gltf E:\MHW-arts\research\mhgu\sakura_batch.json
```

The generated Blender-ready file is:

```text
extracted/m001_body/player/mod/m/pl001/m_body001_out.glb
```

It contains eight non-empty animations from `w07_sa.lmt`:

```text
Motion[1]
Motion[2]
Motion[3]
Motion[51]
Motion[56]
Motion[101]
Motion[102]
Motion[151]
```

Slots 1-3 are the leading Sakura Slash I/II/III candidates, pending visual
confirmation in Blender and correlation with command/effect/hit data.

## Confirmed Blender milestone (2026-07-26)

`m_body001_out.glb` imports successfully into Blender and its MHGU Long Sword
Hunter Art animations are visible and playable. This confirms the complete
standalone path works:

```text
MHGU RomFS -> w07.arc -> w07_sa.lmt -> Revil Toolset -> animated GLB -> Blender
```

The current user is not familiar with Blender or other 3D modeling software.
Future animation-identification instructions should therefore use explicit UI
steps and avoid assuming knowledge of armatures, actions, timelines, the Dope
Sheet, or retargeting.

Blender inspection has now identified the shared Sakura Slash sequence:

```text
Motion[1] -> jump backward
Motion[2] -> forward dash
Motion[3] -> sheath/final recovery
```

Sakura Slash I, II, and III all reuse these same three animation slots. The
level differences therefore come from command/action configuration, hit data,
effect scheduling, and related gameplay parameters rather than separate body
animations.

Do not edit or retarget these motions yet. The next task is to correlate their
frame ranges with the level-specific `plcmd_w07_sa000*_lv01/lv02/lv03`
commands, `w07_sa.psl` effects, and `pl_we07_hitdata.hdp` hit entries.

## Hunter Art command decoding

The `.plbasecmd` files are XFS binaries and can be decoded directly with the
official Revil Toolset (no hex-only reverse engineering is required):

```powershell
revil_toolset.exe bin_to_xml --platform NSW <file-or-folder>
```

Decoded Long Sword Hunter Art entry actions:

```text
sa0000: level I -> action 51, level II -> action 52, level III -> action 53
sa0001: level I -> action 55, level II -> action 56, level III -> action 57
sa0002: level I -> action 58, level II -> action 59, level III -> action 60
sa0003: all levels -> action 90
```

`sa0000` is now the leading Sakura Slash family candidate: its three distinct
level actions fit the shared `Motion[1] -> Motion[2] -> Motion[3]` animation
sequence with level-specific gameplay configuration. This still needs direct
confirmation from the action/state data before being treated as final.

The hit-data file is 16 header bytes followed by 85 entries of 56 bytes each.
The known Sakura motion values are visibly present in the table, including
three 10-MV entries, three 40-MV entries, two 20-MV entries, and four 15-MV
entries in the Hunter Art portion. The next reverse-engineering step is to map
actions 51-53 to those hit-data entry indices and their activation frames.

## Event extraction

```powershell
revil_toolset.exe lmt_to_json w07_sa.lmt
```

This produces `w07_sa.json` containing animation event groups.
