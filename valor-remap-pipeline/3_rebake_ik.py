"""STEP 3 - re-bake body + ground IK channels on the remapped actions.

MUST run AFTER 2_object_root_motion.py: the tether bake writes armature-space
values for the parentless IK bones, so any change to where root travel lives
invalidates it (baked-with-travel + object-travel = IK targets run at ~2x, up
to metres ahead of their targets).

Bakes, per plw_*_remap action, with MHW-FreeKinetics tetherOps:
- body IK   : 247->palm057, 248->palm040, 249->ankle020, 250->ankle016 (FULL),
              252->head004 (LOC). 254 is deliberately left absent - native
              LMT::w03_00 carries no 254 location (native parity).
- ground IK : 251/253 pinned to the world origin (evaluated at frame 0 where
              the object location is 0).

Run (Git Bash):
  VALOR_BLEND="E:/MHW-arts/MHW-Valor.blend" \
    /e/MHW-arts/tools/blender-4.3.2-windows-x64/blender.exe -b --python 3_rebake_ik.py
Saves only if verification passes (body IK vs target distance < 1mm on sampled
frames of every action; ground anchors flat at origin).
"""
import bpy, os, sys, shutil, time, traceback, importlib

BLEND = os.environ.get("VALOR_BLEND", r"E:\MHW-arts\MHW-Valor.blend")
DST_NAME = "m_mesh Armature"
PAIRS_VIEW = (("MhBone_247", "MhBone_057"), ("MhBone_248", "MhBone_040"),
              ("MhBone_249", "MhBone_020"), ("MhBone_250", "MhBone_016"),
              ("MhBone_252", "MhBone_004"))
MAPPING = [(247, 57, 'FULL'), (248, 40, 'FULL'),
           (249, 20, 'FULL'), (250, 16, 'FULL'), (252, 4, 'LOC')]

bak = BLEND + ".bak-" + time.strftime("%Y%m%d-%H%M%S")
shutil.copyfile(BLEND, bak)
print("[backup] %s" % bak, flush=True)

bpy.ops.wm.open_mainfile(filepath=BLEND)
dst = bpy.data.objects.get(DST_NAME)
scene = bpy.context.scene
view = bpy.context.view_layer
if not dst or dst.type != "ARMATURE":
    print("!!! FAIL: armature missing"); sys.exit(1)

tether = importlib.import_module("MHW-FreeKinetics.blender.tetherOps")
print("[tether] module loaded OK", flush=True)

remaps = sorted([a for a in bpy.data.actions
                 if a.name.startswith("plw_") and a.name.endswith("_remap")],
                key=lambda a: a.name)
print("[load] remaps=%d" % len(remaps), flush=True)

def sample_frames(a, n=5):
    lo, hi = a.frame_range
    if hi <= lo:
        return [int(lo)]
    step = (hi - lo) / (n - 1)
    return sorted({int(round(lo + i * step)) for i in range(n)})

def mismatch(a):
    """max per-pair IK-vs-target distance over sampled frames."""
    dst.animation_data_create()
    dst.animation_data.action = a
    out = {}
    for ikn, tgn in PAIRS_VIEW:
        ik, tg = dst.pose.bones[ikn], dst.pose.bones[tgn]
        worst = 0.0
        for f in sample_frames(a):
            scene.frame_set(f)
            view.update()
            worst = max(worst, (ik.matrix.translation - tg.matrix.translation).length)
        out[ikn] = round(worst, 4)
    return out

def ground_bad(a, limit=1e-4):
    for bn in ("MhBone_251", "MhBone_253"):
        for i in range(3):
            f = a.fcurves.find('pose.bones["%s"].location' % bn, index=i)
            if f is None or any(abs(k.co[1]) > limit for k in f.keyframe_points):
                return bn
    return None

# evaluate ground pins at frame 0 (object location is 0 there)
scene.frame_set(0)
view.update()

print("=== RE-BAKE ===", flush=True)
fails = []
for n, a in enumerate(remaps, 1):
    try:
        tether.recalculateBodyIKBones(dst, a, MAPPING)
        tether.recalculateGroundIKBones(dst, a)
    except Exception:
        traceback.print_exc()
        fails.append(a.name)
    if n % 20 == 0:
        print("[bake] %d/%d" % (n, len(remaps)), flush=True)
print("[bake] done, fails=%s" % fails, flush=True)

print("=== VERIFY ===", flush=True)
bad = []
worst_all = 0.0
for i, a in enumerate(remaps, 1):
    mm = mismatch(a)
    w = max(mm.values())
    worst_all = max(worst_all, w)
    gb = ground_bad(a)
    if w > 1e-3 or gb:
        bad.append((a.name, mm, gb))
    if i % 40 == 0:
        print("[verify] %d/%d worst=%.4f" % (i, len(remaps), worst_all), flush=True)
print("[verify] done: worst=%.4f bad=%d %s" % (worst_all, len(bad), bad[:4]), flush=True)

dst.animation_data.action = remaps[0]
scene.frame_set(0)
view.update()

if fails or bad:
    print("!!! NOT SAVING: fails=%d bad=%d" % (len(fails), len(bad)))
    sys.exit(1)

bpy.ops.wm.save_mainfile()
print("[done] saved:", BLEND, flush=True)
