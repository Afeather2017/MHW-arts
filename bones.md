# A doc explain bones of MHW armature

## Axis

In blender, z is height, x,y is front/back and left/right.
While in MHW, x,z is front/back and left/right, y is height.

The explaination is in blender axis.

## Special bones

These are game-side IK/anchor channels (IK = inverse kinematics: the game reads them
as runtime *targets* — foot plants, hand-on-hilt, ground pins — and solves joints
toward them; they are not normal deform bones). The game derives them from normal
bones, they are not free to animate. When migrating an animation, do NOT retarget them
as normal bones — keep the target rig's own curves, or let the tool synthesize
them (FreeKinetics Rig Transfer does this with constraints).

Migration rule: `mhgu-to-mhw.bmap` has NO entries for 247-254 (it only maps 000-020
and 064-082), so a remapped action never contains these channels. After remap you
must either (a) preserve the native target action's 249-253 curves
(`merge_remapped_lmt_preserve_special_bones.py`), or (b) run FreeKinetics
"Recalculate All IK Bones" (Action Tools panel) to regenerate them from the remapped
pose — per-bone checkboxes; needs the action tethered to the MHW armature.
Recalculate is the safer choice when the remapped pose differs a lot from the
original MHW animation.

FreeKinetics has the same table hardcoded, numbers match this doc:

- `MHW-FreeKinetics/operators/lmt_rig_ops.py` — top comment + `platformDefaults`
- `MHW-FreeKinetics/blender/tetherOps.py` — `IK_BODY_MAP` / `IK_GROUND_MAP`

- 000: the root bone, decide the y(height) axis of player.
- 249: right leg IK, decide position of right ankle, should bind to 020.
- 250: left leg IK, decide position of left ankle, should bind to 016.
- 247: right hand IK, decide position of right wrist, should bind to 012
  (modern rigs bind to right palm 057 instead, per `tetherOps.py` `IK_BODY_MAP`).
- 248: left hand IK, decide position of left wrist, should bind to 008
  (modern rigs bind to left palm 040 instead).
- 252: head IK, position only (no rotation), should bind to 004.
- 254: neck IK, position only (no rotation), should bind to 003.
- 253: ground anchor (camera), pinned to world origin, follows (x,y) of bone 000.
- 251: upper-body anchor, pinned to world origin.

NOTE: left/right labels of legs follow FreeKinetics (249=right, 250=left).
My earlier notes had them swapped — verify once on screen before relying on it.

## Player position

The bones decide the player positions.

### Front/back and left/right

Object have X/Y Locations, Y+ means go backward, and X+ goes to left.
These are object-level fcurves, not bone curves. The working native action
carries 7 object channels: location XYZ + quaternion rotation WXYZ.

### Height?

000.Y Location is likely related to Height, MUST BE Bézier line.

FreeKinetics agrees with this split: its `nullRootY` ("Root as Ground Level"
option) zeroes bone 000's location-Y to pin the player to the ground, while
horizontal travel stays on the object-level location curves. So "how the player
moves" = object X/Y location + root 000 Y height, and that logic lives inside
FreeKinetics' Rig Transfer, not in the FSM/LMT docs.

## Weapon holding

030 by default placed to near 012 (054 is recommanded) and 047 to 030.

Skeleton binding (measured with `inspect_bind_bones.py` on `m_mesh Armature`):

- 030: leaf socket bone (no children, 0.05 long, not connected), parent = 008,
  full chain 000→013→001→002→005→006→007→008, rest head (+0.72, 0.02, 0.37).
- 047: leaf socket bone, parent = 012, chain 000→013→001→002→009→010→011→012,
  rest head (−0.72, 0.02, 0.37) — mirror of 030.
- So 030 hangs under the same arm as hand-IK 248 (→008), 047 under the same arm
  as 247 (→012). FreeKinetics defines none of this — it is pure skeleton data,
  the addon only carries whatever fcurves the action already has.

Only y scale of 030 and 047 change the binding state. x,z scale defaults to 1, they have no effect.

| 030 | 047 | Left   | Right  | Back         |
|-----|-----|--------|--------|--------------|
| 1   | 1   | None   | None   | Sheath&Sword |
| 1   | 2   | None   | Sword  | Sheath       |
| 2   | 1   | Sword  | None   | Sheath       |
| 2   | 2   | None   | Sword  | Sheath       |
| 3   | 1   | None   | Sheath | Sword        |
| 3   | 2   | Sword  | Sheath | None         |

XYZ location, XYZW rotation have some effect, not very clear.

### Not in FreeKinetics

FreeKinetics does NOT know this system at all — no grip/030/047 code anywhere in
the addon (checked by search; the only mentions are the identity map
`bmaps/mhw-to-mhw.bmap` tagging the names, and unrelated `WeaponEfc*`/`mWeaponMotion*`
TIML table fields in `timl_controller.py`). This doc + `grip-states.md` are the only
documentation.

Consequences when migrating animations:

- `mhgu-to-mhw.bmap` has no 030/047 entries, so MHGU-sourced remapped actions never
  carry grip channels.
- No default values are written either: Rig Transfer deletes scale fcurves whose keys
  are all constant 1.0 (`clearConstraints` in `lmt_rig_ops.py`), and `completeBasis`
  only ever extends the root bone's (function −1) curves — nothing else gets defaults.
- A missing 030/047 channel therefore means the game keeps the binding of the
  previously played action (proven by native slots 433/435/437 and the remapped
  `101.001`). The implicit "default" is NOT (1,1) — it is inherit-from-previous.
- Therefore: author integer Y-scale keys on MhBone_030 / MhBone_047 by hand on the
  merged target action before export, or the grip silently follows whatever the
  previously played native action left bound. Authoring recipe (integer frames/values,
  flat Bézier handles at ±1/3 segment) is at the bottom of `grip-states.md`.
