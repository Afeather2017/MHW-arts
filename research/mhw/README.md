# MHW Long Sword animation inspection

## Verified Blender workflow

MHW uses a newer LMT format than MHGU. Revil Toolset can convert the MHGU
animations, but it does not load World's `w03_00.lmt`. Use the dedicated
Blender 2.79 tools instead:

- `AsteriskAmpersand/Mod3-MHW-Importer`
- `Strackeror/MHW-LMT-Loader`

The portable Blender used for this workflow is under:

```text
E:\MHW-arts\tools\blender-2.79-windows64
```

Import the MHW body model with **Import Skeleton = Animation Armature**:

```text
E:\MHW-arts\chunk-extracted\chunk\pl\f_equip\pl001_0000\body\mod\f_body001_0000.mod3
```

The MOD3 importer's default is `Empty Tree`. An Empty Tree is visually similar
to a skeleton but cannot receive an LMT animation. Attempting to import an LMT
in that state causes the loader to fail with:

```text
UnboundLocalError: local variable 'armature_obj' referenced before assignment
```

A clean, verified armature-mode Blender file was generated at:

```text
research\mhw\skeleton\mhw_hunter_armature.blend
```

The raw MHW LMT was successfully loaded with this armature. Confirmed visual
identifications:

```text
Animation 001 -> Long Sword idle
Animation 002 -> draw Long Sword
```

## LMT slot comparison

All three tested LMT files contain 464 slots. Valor and Spirit Reckoning replace
existing entries; neither adds entries beyond the original slot table.

Valor changes these slots relative to the extracted original MHW LMT:

```text
10, 11, 12, 13,
101, 106, 107, 109, 110,
140, 154, 156, 157, 158, 162, 170, 174,
306, 307, 308, 309, 310, 311, 312,
332, 334, 335, 338, 339, 340, 341, 342, 344, 346, 351,
451
```

Spirit Reckoning changes:

```text
108, 109, 115, 170,
306, 308, 309, 310, 311,
338, 339, 340, 341,
357, 364, 367
```

The first inspection target is Valor slots `306-312`, because they form a
compact consecutive block and substantially overlap the Spirit Reckoning
replacement set. Inspect them individually in Blender before deciding which
three slots will host Sakura Slash's backward jump, dash/slash, and sheath.

Confirmed visual identification of the Valor block so far:

```text
306 -> Special Sheathe
307 -> Iai draw attack
308 -> Spirit Roundslash followed by normal sheathing
309 -> visually empty
310 -> visually empty
311 -> visually empty
312 -> Special Sheathe timeout transition to regular sheathing
```

Special Sheathe can be held for approximately five seconds; slot `312` handles
the transition back to regular sheathing after that timeout. Slots `309-311`
look empty in Blender, but they are not empty LMT entries: each contains 82
bone tracks and 14 event groups. They belong to the same active Special
Sheathe/state block and must not be assumed safe based on their appearance.

Slots `306-312` should not be treated as safe Sakura Slash replacement slots.

The repeatable comparison script is:

```text
research\mhw\compare_lmt_slots.py
```
