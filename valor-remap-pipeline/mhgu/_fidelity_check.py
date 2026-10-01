"""Fidelity spot-check for the 3 new remaps (same metric as 5_validate step A:
anatomical-pair world-position distance, source rig vs remapped pose).
READ-ONLY, saves nothing. Arg after --: action names (without _remap)."""
import bpy
import sys

WORK = r"E:\MHW-arts\mhgu-valor-mhw.blend"
bpy.ops.wm.open_mainfile(filepath=WORK)
dst = bpy.data.objects["m_mesh Armature"]
src_rig = bpy.data.objects["Animation Group"]
scn = bpy.context.scene
view = bpy.context.view_layer

PAIRS = [("0:0", "MhBone_000"), ("7:1", "MhBone_001"), ("8:2", "MhBone_002"),
         ("9:3", "MhBone_003"), ("10:4", "MhBone_004"),
         ("15:5", "MhBone_005"), ("16:6", "MhBone_006"), ("17:7", "MhBone_007"),
         ("18:8", "MhBone_008"), ("11:9", "MhBone_009"), ("12:10", "MhBone_010"),
         ("13:11", "MhBone_011"), ("14:12", "MhBone_012"),
         ("4:14", "MhBone_014"), ("5:15", "MhBone_015"), ("6:16", "MhBone_016"),
         ("1:17", "MhBone_018"), ("2:18", "MhBone_019"), ("3:19", "MhBone_020")]

def pose_of(rig, act, f):
    rig.animation_data.action = act
    scn.frame_set(f)
    view.update()
    return {b.name: b.matrix.copy() for b in rig.pose.bones}

names = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else [
    "LMT::w07::195", "LMT::w07::196", "LMT::w07::095"]

for srcname in names:
    src_act = bpy.data.actions[srcname]
    remap = bpy.data.actions[srcname + "_remap"]
    lo, hi = (int(round(x)) for x in src_act.frame_range)
    frames = sorted({f for f in (lo + 5, (lo + hi) // 3, (lo + hi) * 2 // 3, hi - 5) if lo <= f <= hi})
    errs = {d: 0.0 for _, d in PAIRS}
    for f in frames:
        ps = pose_of(src_rig, src_act, f)
        pd = pose_of(dst, remap, f)
        for s, d in PAIRS:
            errs[d] = max(errs[d], (ps[s].translation - pd[d].translation).length)
    cog0 = remap.fcurves.find('pose.bones["MhBone_000"].location', index=1)
    anchor = cog0.keyframe_points[0].co[1] if cog0 and len(cog0.keyframe_points) else float("nan")
    core = [errs[d] for _, d in PAIRS[:5]]
    limbs = [errs[d] for _, d in PAIRS[5:]]
    ok = max(core) < 0.15 and max(limbs) < 0.45
    print("%-18s frames=%s cogY0=%.4f core=%.3f limbs=%.3f -> %s" % (
        srcname, frames, anchor, max(core), max(limbs), "PASS" if ok else "FAIL"), flush=True)
    worst = sorted(errs.items(), key=lambda kv: -kv[1])[:4]
    print("    worst pairs:", [(d, round(v, 3)) for d, v in worst])
