"""MHGU chain STEP 7 - merge a remap into the NATIVE entry action (luocha-style).

The in-game-verified reference (w03_00_luocha.lmt) is the NATIVE entry's track
layout - 82 tracks incl. empty finger tracks, native LIMITS metadata, native
frameCount - with the MHGU body motion overwritten in place.  Track-for-track
exports of standalone remap actions get their unresolvable transforms omitted
(15 tracks -> the hunter collapses in game).

For each (native_action, remap_action) pair, inside the NATIVE action:
- rebuild its own track layout (keys clipped to the remap's length, metadata kept),
- overwrite body rotations, MhBone_000 location (cog), sockets 030/047,
  IK 247-253, 254 rotation, object location/rotation (travel) from the remap,
- keep native fingers/101-104 and native empty tracks,
- set freehk.frameCount to the remap length.
Native layout channels the remap does not carry stay native.

The vanilla native action is preserved once as <name>_vanilla.  Idempotent.

Pairs via MERGE_PAIRS env: "native;;remap|native;;remap ..."
(default: LMT::w03_00::103 ;; LMT::w07_sa::151_remap)
"""
import bpy, os, sys, re, shutil, time

WORK = os.environ.get("MHGU_WORK_BLEND", r"E:\MHW-arts\mhgu-valor-mhw.blend")
PAIRS = os.environ.get("MERGE_PAIRS", "LMT::w03_00::103;;LMT::w07_sa::151_remap")
# user-verified recipe: keep the NATIVE entry's IK curves (247-253).  Our baked
# IK is exact in Blender but collapses in game (space convention?); native IK
# with an MHGU motion was verified acceptable ("都是对的,除了刀位").
# default OFF: our baked IK (cm-scaled by the exporter) is in-game verified
# (2026-09-27, entries 103/104 - feet/hands follow the ported motion itself).
# Set PRESERVE_NATIVE_IK=1 to fall back to the native entry's own IK curves.
PRESERVE_NATIVE_IK = os.environ.get("PRESERVE_NATIVE_IK", "0") != "0"
# LMT IK position unit = centimetres (native 249 loc span is exactly 100x the
# Blender values on every axis); scale our metre-space bake when importing it.
# (IK_CM removed: the exporter does the m->cm conversion for encoded loc tracks)
pairs = [p.split(";;") for p in PAIRS.split("|") if p.strip()]

bak = WORK + ".bak-" + time.strftime("%Y%m%d-%H%M%S")
shutil.copyfile(WORK, bak)
print("[backup] %s" % bak, flush=True)

bpy.ops.wm.open_mainfile(filepath=WORK)

def group_key(data_path):
    m = re.match(r'pose\.bones\["([^"]+)"\]\.(\w+)$', data_path)
    return (m.group(1), m.group(2)) if m else ("__obj__", data_path)

def ensure_min_two_keys(action, fc_count=None):
    """bounds() drops the first keyframe: a 1-key constant transform crashes the
    exporter with an empty maxima/minima zip. Give every 1-key curve a second
    (equal-value) key."""
    n = 0
    for fc in action.fcurves:
        kp = fc.keyframe_points
        if len(kp) == 1:
            f0, v0 = kp[0].co[0], kp[0].co[1]
            f1 = f0 + 1.0
            if fc_count is not None and f1 > fc_count:
                f1 = max(f0 - 1.0, 0.0)
            k = len(kp)
            kp.add(1)
            kp[k].co = (f1, v0)
            kp[k].interpolation = kp[0].interpolation
            kp.sort()
            fc.update()
            n += 1
    return n

for native_name, remap_name in pairs:
    N = bpy.data.actions.get(native_name)
    R = bpy.data.actions.get(remap_name)
    if N is None or R is None:
        print("!!! FAIL: missing %s or %s" % (native_name, remap_name)); sys.exit(1)
    if bpy.data.actions.get(native_name + "_vanilla") is None:
        v = N.copy()
        v.name = native_name + "_vanilla"
        v.use_fake_user = True
        print("[merge] vanilla snapshot: %s" % v.name, flush=True)

    bumped = ensure_min_two_keys(R)
    R_lo, R_hi = R.frame_range
    if bumped:
        print("[merge] %s: %d single-key curves bumped to 2" % (remap_name, bumped), flush=True)
    R_groups = {}
    for fc in R.fcurves:
        R_groups.setdefault(group_key(fc.data_path), []).append(fc)

    # ---- snapshot from the VANILLA base when present (fresh native each run;
    # re-merging must not inherit earlier merges' IK/finger overwrites) ----
    vanilla = bpy.data.actions.get(native_name + "_vanilla")
    base = vanilla if vanilla is not None else N
    snap = {}
    for fc in list(base.fcurves):
        bone, prop = group_key(fc.data_path)
        mods = [(m.type, m.min_x, m.min_y, m.use_restricted_range, m.mute,
                 m.frame_start, m.frame_end) for m in fc.modifiers]
        keys = [(k.co[0], k.co[1], k.interpolation) for k in fc.keyframe_points
                if k.co[0] <= R_hi + 1e-6]
        snap[(bone, prop, fc.array_index)] = (keys, mods)
    while len(N.fcurves):
        N.fcurves.remove(N.fcurves[0])
    for (bone, prop, idx), (keys, mods) in snap.items():
        if not keys:
            continue  # nothing left in range; skip rather than write an empty curve
        path = ("pose.bones[\"%s\"].%s" % (bone, prop)) if bone != "__obj__" else prop
        fc = N.fcurves.new(data_path=path, index=idx)
        fc.keyframe_points.add(len(keys))
        for k, (fr, v, interp) in zip(fc.keyframe_points, keys):
            k.co = (fr, v)
            k.interpolation = interp
        fc.update()
        for (mtype, min_x, min_y, urr, mute, fs, fe) in mods:
            if mtype == "LIMITS":
                l = fc.modifiers.new("LIMITS")
                l.use_restricted_range = urr
                l.mute = mute
                l.frame_start, l.frame_end = fs, fe
                l.min_x, l.min_y = min_x, min_y

    # ---- overwrite channel groups that exist in the native layout ----
    imported_groups = []
    for (bone, prop), fcs in sorted(R_groups.items()):
        if prop == "scale" and bone not in ("MhBone_030", "MhBone_047"):
            continue                      # native carries scale only on sockets
        is_ik = bone in ("MhBone_247","MhBone_248","MhBone_249",
                         "MhBone_250","MhBone_251","MhBone_252","MhBone_253")
        if PRESERVE_NATIVE_IK and is_ik:
            continue                      # keep the native entry's own IK curves
        # LMT stores IK (247-253) positions in CENTIMETRES while Blender uses
        # metres: the exporter only converts parented bones, so IK positions
        # must be scaled x100 here (proven in game, 2026-09-27 entries 103/104).
        if is_ik and prop == "location":
            pass  # scaling applied to pts below
        if all((bone, prop, fc.array_index) not in snap for fc in fcs):
            continue                      # channel the native layout doesn't track
        path = ("pose.bones[\"%s\"].%s" % (bone, prop)) if bone != "__obj__" else prop
        for fc in fcs:
            tgt = N.fcurves.find(path, index=fc.array_index)
            if tgt is None:
                continue
            pts = [(k.co[0], k.co[1]) for k in fc.keyframe_points
                   if R_lo - 1e-6 <= k.co[0] <= R_hi + 1e-6]
            # NOTE: no unit scaling here - the exporter converts location
            # tracks m->cm itself for properly LIMITS-tagged fcurves (verified:
            # native cog 0.87 m exports as 87.46; IK bake metres stay metres).
            if not pts:
                continue
            kp = tgt.keyframe_points
            kp.clear()
            kp.add(len(pts))
            for k, (fr, v) in zip(kp, pts):
                k.co = (fr, v)
            tgt.update()
        imported_groups.append((bone, prop))
    print("[merge] %s <- %s: %d channel groups overwritten" % (native_name, remap_name, len(imported_groups)), flush=True)

    # ---- normalise keys to the native convention: every channel keyed at
    # frame 0 and at frameCount (the parser warns/errors on missing basis /
    # last-frame keys), integer frames only.  Quaternion buffer types pack the
    # frame delta in 4 bits (max 15), so long gaps get split with equal-value
    # keys ----
    fc_count = int(round(R_hi))
    MAX_STEP = 15
    fixed = 0
    for fc in N.fcurves:
        kp = fc.keyframe_points
        if not len(kp):
            continue
        kp[0].co[0] = 0.0
        kp[-1].co[0] = fc_count
        for k in kp:
            k.co[0] = float(round(k.co[0]))
        kp.sort()  # single-key curves become [351, 0] after the two lines above
        # hold-to-end key if the curve ends early
        if kp[-1].co[0] < fc_count:
            v, interp = kp[-1].co[1], kp[-1].interpolation
            n0 = len(kp)
            kp.add(1)
            kp[n0].co = (fc_count, v)
            kp[n0].interpolation = interp
            fixed += 1
        # basis key if the curve starts late
        if kp[0].co[0] > 0.0:
            v0 = kp[0].co[1]
            interp0 = kp[1].interpolation if len(kp) > 1 else 'BEZIER'
            n0 = len(kp)
            kp.add(1)
            kp[n0].co = (0.0, v0)
            kp[n0].interpolation = interp0
            fixed += 1
        kp.sort()  # basis/hold appends leave the array unsorted; the splitter needs order
        # split long gaps with equal-value keys LAST (4-bit frame delta limit
        # on quaternion buffer types): must run after hold/basis keys exist
        changed = True
        while changed:
            changed = False
            for i in range(len(kp) - 1):
                gap = kp[i + 1].co[0] - kp[i].co[0]
                if gap > MAX_STEP:
                    v = kp[i].co[1]
                    n0 = len(kp)
                    kp.add(1)
                    kp[n0].co = (kp[i].co[0] + MAX_STEP, v)
                    kp.sort()
                    fixed += 1
                    changed = True
                    break
        kp.sort()
        fc.update()
    try:
        N.freehk.frameCount = fc_count
        N.freehk.loopFrame = R.freehk.loopFrame
    except Exception:
        pass
    print("[merge] normalised keys: %d hold/basis keys added, frameCount=%d" % (fixed, fc_count), flush=True)
    n2 = ensure_min_two_keys(N, fc_count)
    if n2:
        print("[merge] %s: %d constant curves bumped to 2 keys" % (native_name, n2), flush=True)

bpy.ops.wm.save_mainfile()
print("[done] saved:", WORK, flush=True)