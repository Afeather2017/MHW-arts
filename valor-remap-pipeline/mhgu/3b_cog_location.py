"""MHGU chain STEP 3b - MhBone_000.location = 0:0 - reference (user model).

User-confirmed: MHW bone 000 is the cog at navel height; MHGU `reference` sits
at the ground and its child `0:0` is the cog. So 000's location channel gets
0:0's location VERBATIM (axis identity proven: 0:0 locY 0.4713 == native
LMT::w03_00::106 MhBone_000 locY 0.4714, and both bones share the same
local->world map: localY=height, localX=side, localZ=front/back).
000 rotation is NOT touched (stays identity from pure ARP; body lean rides on
the waist 013 like the user's working ARP setup).

Idempotent. Run:
  /e/MHW-arts/tools/blender-4.3.2-windows-x64/blender.exe -b --python mhgu/3b_cog_location.py
"""
import bpy, os, sys, shutil, time

WORK = os.environ.get("MHGU_WORK_BLEND", r"E:\MHW-arts\mhgu-valor-mhw.blend")
COG_PATH = 'pose.bones["MhBone_000"].location'
SRC_PATH = 'pose.bones["0:0"].location'

bak = WORK + ".bak-" + time.strftime("%Y%m%d-%H%M%S")
shutil.copyfile(WORK, bak)
print("[backup] %s" % bak, flush=True)

bpy.ops.wm.open_mainfile(filepath=WORK)
done = 0
skipped = []
for src in sorted([a for a in bpy.data.actions if "mhgu_source" in a and not a.name.endswith("_remap")],
                  key=lambda a: a.name):
    remap = bpy.data.actions.get(src.name + "_remap")
    if remap is None:
        skipped.append(src.name)
        continue
    for i in range(3):
        old = remap.fcurves.find(COG_PATH, index=i)
        if old is not None:
            remap.fcurves.remove(old)
        sfc = src.fcurves.find(SRC_PATH, index=i)
        if sfc is None:
            continue
        fc = remap.fcurves.new(data_path=COG_PATH, index=i)
        fc.keyframe_points.add(len(sfc.keyframe_points))
        for k, sk in zip(fc.keyframe_points, sfc.keyframe_points):
            k.co = sk.co
            k.interpolation = sk.interpolation
            k.handle_left_type = sk.handle_left_type
            k.handle_right_type = sk.handle_right_type
        fc.update()
    done += 1
print("[cog] location copied for %d actions (skipped %s)" % (done, skipped or "none"), flush=True)

# verify: every remap has a cog height channel whose first key == source's
bad = []
for src in [a for a in bpy.data.actions if "mhgu_source" in a and not a.name.endswith("_remap")]:
    remap = bpy.data.actions.get(src.name + "_remap")
    fy = remap.fcurves.find(COG_PATH, index=1) if remap else None
    sfy = src.fcurves.find(SRC_PATH, index=1)
    if fy is None or sfy is None or not len(fy.keyframe_points) or not len(sfy.keyframe_points):
        bad.append((src.name, "missing")); continue
    if abs(fy.keyframe_points[0].co[1] - sfy.keyframe_points[0].co[1]) > 1e-6:
        bad.append((src.name, "value mismatch"))
print("[verify] cog problems: %s" % (bad[:6] if bad else "none"), flush=True)
if bad:
    print("!!! NOT SAVING"); sys.exit(1)

bpy.ops.wm.save_mainfile()
print("[done] saved:", WORK, flush=True)
