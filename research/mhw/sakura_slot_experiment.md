# Sakura Slash temporary slot experiment

Use the existing Spirit Reckoning FSM and replace only the motion data in the Long Sword LMT:

| MHW slot | Initial source |
|---:|---|
| 338 | MHGU `w07_sa` Motion 1 (backward jump) |
| 339 | MHGU `w07_sa` Motion 2 (forward dash/slash) |
| 340 | temporary duplicate of Motion 2 |
| 341 | MHGU `w07_sa` Motion 3 (sheathing recovery) |

Do not modify the FSM, hit data, or effects for this first test. Keep a backup of
`w03_00_valor.lmt` and install the test file at the same `nativePC` path.

## Export tool

`MHW-LMT-Loader` is import-only. The required Blender 2.79 exporter is Free
Hyperkinetics:

https://github.com/AsteriskAmpersand/MHW-Free-HyperKinetics

It requires Blender **2.79c**, not 2.79b. First use it to export an unchanged
MHW action (slot 338) and re-import/test that file. Only after that round-trip
works should the MHGU action be retargeted onto `mhw_hunter_armature`.

Installed locally:

- Blender: `E:/MHW-arts/tools/blender-2.79c/blender.exe`
- Free Hyperkinetics source: `E:/MHW-arts/research/MHW-Free-HyperKinetics`
- Add-on: `E:/MHW-arts/tools/blender-2.79c/2.79/scripts/addons/free_hyperkinetics`

Free Hyperkinetics, the MOD3 importer, and the older LMT loader were all
enabled and verified in Blender 2.79c.

## Successful round-trip

The old LMT loader's `Animation 338` action was exported through FreeHK into
an original MHW LMT staging file, then the encoded slot was injected into the
Valor LMT with the older loader's block serializer. The result opens in the
old loader and slot 338 visually matches the original Valor dash:

`E:/MHW-arts/research/mhw/sakura_test/w03_00_slot338_roundtrip.lmt`

FreeHK cannot directly use the Valor or Spirit Reckoning LMT as its injection
target because their embedded TIML data causes its parser to read beyond EOF.
Use an original MHW LMT as the FreeHK staging target, then transplant only the
encoded slot into the modified Valor LMT.

The round-trip output has 464 entries (valid IDs `0` through `463`) and the
older loader confirms that slot 338 still plays the original Valor dash.

## Practical Blender workflow

Use Blender 5.2 for interactive MHGU inspection and eventual retargeting. The
animated Revil GLB is smooth and complete there. Use Blender 2.79c only as a
scripted/headless MHW LMT encoder because porting FreeHK's old UI, node,
Action, FCurve, and armature APIs to Blender 5.2 would be a separate project.

```text
Blender 5.2: inspect/edit/retarget MHGU animation
    -> FBX animation bridge
Blender 2.79c + FreeHK: encode into original MHW LMT staging file
    -> transplant encoded slot
Valor LMT: preserve working FSM and all other modified slots
```

`m_body001_out.glb` already contains all eight `w07_sa` animations. The FBX
bridge must export animation (`bake_anim=True`); disabling animation produces
only the 34-bone MHGU armature and makes it appear that the LMT failed.

Tested animated bridge:

`E:/MHW-arts/research/mhgu/mhgu_hunter_armature.fbx`

It imports into Blender 2.79c with the MHGU armature and these Sakura actions:

- Motion 1: frames 1-26
- Motion 2: frames 1-11
- Motion 3: frames 1-56

Relevant repeatable scripts:

- `research/mhgu/convert_mhgu_glb_to_fbx.py`
- `research/mhw/sakura_test/run_freehk_export.py`
- `research/mhw/sakura_test/inject_slot338_into_valor.py`
