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

### Root-motion migration recipe (Rise remaps -> MHW, applied 2026-09-26)

ARP-remapped actions carry horizontal travel on MhBone_000 location X/Z (bone
space) and NO object channels. Native `LMT::w03_00::*` actions are the format
reference: object location first key is ALWAYS 0, loc[2] (height) constant 0,
rotation identity, all BEZIER. Conversion implemented in
`valor-remap-pipeline/2_object_root_motion.py` (ran on MHW-Valor.blend, 155
actions; full re-run chain + README live in `valor-remap-pipeline/`):

- `object.location[0] = boneX - x0` (x0 = first key value; native convention
  is travel starts at 0 — 126/155 remaps had nonzero first keys, e.g. -0.21667)
- `object.location[1] = z0 - boneZ` (mirror Z AND normalize to first key 0;
  131/155 had nonzero first Z too — mirror alone is not enough)
- `object.location[2]` constant 0, `rotation_quaternion` constant identity
- bone MhBone_000 loc X/Z -> constant 0 (native parity; only loc Y = height
  stays animated on the bone), loc Y keys -> BEZIER AUTO_CLAMPED
- IK channels: strip channel types no native action carries (IK-bone scale
  fcurves, 254 location). Remap IK loc/rot for 247-253 must be RE-BAKED after
  the object migration: the Aug-31 tether bake ran while travel still lived on
  bone 000, so the baked (armature-space, parentless bones) values included it
  once; moving travel to the object applied it twice -> 249/250/252 ran up to
  3.9 m ahead of their targets (looked like "x * 2"). Fix =
  `valor-remap-pipeline/3_rebake_ik.py` (reran tether.recalculateBodyIKBones with mapping
  247->057 / 248->040 / 249->020 / 250->016 / 252->004 over all 155 actions;
  254 stays absent, ground anchors 251/253 untouched). Verified IK-vs-target
  distance 0.0000 on every action afterwards. Rule: ANY change to where root
  travel lives invalidates body-IK bakes — always re-run the bake after it.
- ARP also wrote body-bone scale fcurves (all constant 1.0, 31620 in the
  file). Natives carry none; proven inert, left in place (mass
  fcurves.remove crashes Blender 4.3 in batch).

Splicing actions under this convention needs NO IK rework: travel lives on
object curves and IK values are pose-space, so a splice = time-shift B to A's
end + offset B's object location by A's end location. Standalone tool:
`valor-remap-pipeline/splice_actions.py` (paste-into-GUI; REFRESH_IK flag
re-bakes via FreeKinetics for actions NOT produced by this convention).

Rise root-motion carrier (2026-09-27): horizontal travel lives on the Rise
`Root` bone (Cog is height-only, source actions carry no object curves), in
chain-authored absolute-ish values (slot 112 starts X=-0.2166 = the "-0.21667"
first key the user first fixed by hand). The old ARP run baked Root->MhBone_000
1:1; step2's first-key normalization (boneX - x0) is relative-delta and correct,
but chain motions keep a NET displacement (112 ends (+0.15,+0.41) off) — Rise
the game evidently does not apply Root location as actor displacement.
`valor-remap-pipeline/5_close_stance_loops.py` closes object loc[0]/[1] with a
linear ramp for actions whose END POSE matches native idle LMT::w03_00::001
(IK-mean < 5 deg; 52/155 closed, rest kept open like natives — 92/154 natives
end displaced, up to 4.3 m). Verified: 112 end head (0.095,-0.211) vs native
001 (0.095,-0.21).

### Height?

000.Y Location is likely related to Height, MUST BE Bézier line.

FreeKinetics agrees with this split: its `nullRootY` ("Root as Ground Level"
option) zeroes bone 000's location-Y to pin the player to the ground, while
horizontal travel stays on the object-level location curves. So "how the player
moves" = object X/Y location + root 000 Y height, and that logic lives inside
FreeKinetics' Rig Transfer, not in the FSM/LMT docs.

## Weapon holding

030 by default placed to near 012 (049 is recommanded) and 047 to 049 too.

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

Cross-game note (2026-09-27): the Rise LS socket `L_Weapon_00` rest grip offset
(loc (0.1008, -0.0404, 0.5808), quat (0.0365, 0.0045, -0.0114, 0.9993)) equals
native MHW `LMT::w03_00::106` MhBone_030 grip values to ~1e-4 — both engines
share the same left-hand LS grip convention. MHW-Valor.blend remap actions are
authored with the static left-hand default taken from 106 (030 verbatim single
  keys @f0 incl. scale=(1,2,1); 047 constants from 106 frame-0 loc/quat with
  scale=(1,1,1) inactive; `valor-remap-pipeline/4_grip_left_from_lmt106.py`),
  replacing the per-action
Rise socket bake (Rise has NO scale state encoding — binding state in Rise is
game-side via motfsm2/weapon system).

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
