"""MHGU chain STEP 2 (exact) - math retarget MHGU 'Animation Group' -> MHW rig.

Replaces the ARP pass for this source rig. Measured motivation: the same ARP
setup transferred the Rise rig with millimetre fidelity (baseline measured on
mhrs-valor-mod-motions-remapped.blend: torso/hands 0.000 m), but on the raw
MHGU GLB armature it diverges 0.3-0.7 m on limb ends - its generic axis
correction does not fit the GLB bone axes.

Method (world-delta embedding, exact when rest geometries align - verified:
MHGU `1:17` rest head == MhBone_018 rest head, cog channels proven identical):

    Q_dst(f) = Q_src(f) @ Rest_src^-1 @ Rest_dst
    L_dst(f) = Rest_dst^-1 @ Q_dst(f)          (local quat written to action)

- rotations: 19 anatomical pairs (bmap bodies, `0:0` -> MhBone_000)
- location : cog pair only, verbatim axis-identity copy (0.4713 == native
  0.4714 proof); every other MHGU track is rotation-only
- freehk metadata + fake_user copied; keys baked per integer frame 0..N+1
  (LMT 60fps density, same as the imported sources)

Run:
  /e/MHW-arts/tools/blender-4.3.2-windows-x64/blender.exe -b --python mhgu/2_exact_retarget_mhgu.py
"""
import bpy, os, sys, shutil, time

WORK = os.environ.get("MHGU_WORK_BLEND", r"E:\MHW-arts\mhgu-valor-mhw.blend")
SRC_NAME = "Animation Group"
DST_NAME = "m_mesh Armature"
FILTER = "plw_"
COG_SRC, COG_DST = "0:0", "MhBone_000"
PAIRS = [(COG_SRC, COG_DST), ("7:1", "MhBone_001"), ("8:2", "MhBone_002"),
         ("9:3", "MhBone_003"), ("10:4", "MhBone_004"),
         ("15:5", "MhBone_005"), ("16:6", "MhBone_006"), ("17:7", "MhBone_007"),
         ("18:8", "MhBone_008"), ("11:9", "MhBone_009"), ("12:10", "MhBone_010"),
         ("13:11", "MhBone_011"), ("14:12", "MhBone_012"),
         ("4:14", "MhBone_014"), ("5:15", "MhBone_015"), ("6:16", "MhBone_016"),
         ("1:17", "MhBone_018"), ("2:18", "MhBone_019"), ("3:19", "MhBone_020")]

def die(msg):
    print("!!! FAIL:", msg)
    sys.exit(1)

bak = WORK + ".bak-" + time.strftime("%Y%m%d-%H%M%S")
shutil.copyfile(WORK, bak)
print("[backup] %s" % bak, flush=True)

bpy.ops.wm.open_mainfile(filepath=WORK)
scn = bpy.context.scene
view = bpy.context.view_layer
src_rig = bpy.data.objects.get(SRC_NAME)
dst_rig = bpy.data.objects.get(DST_NAME)
if not src_rig or not dst_rig:
    die("rigs missing")

# drop ARP-era remaps if present
doomed = [a for a in bpy.data.actions if a.name.endswith("_remap")]
for a in doomed:
    bpy.data.actions.remove(a)
if doomed:
    print("[purge] removed %d old remap actions" % len(doomed), flush=True)

rest_src = {s: src_rig.data.bones[s].matrix_local.to_quaternion() for s, _ in PAIRS}
rest_dst = {d: dst_rig.data.bones[d].matrix_local.to_quaternion() for _, d in PAIRS}
rest_dst_inv = {d: q.inverted() for d, q in rest_dst.items()}
embed = {s: rest_src[s].inverted() @ rest_dst[d] for s, d in PAIRS}

srcs = sorted([a for a in bpy.data.actions
               if a.name.startswith(FILTER) and not a.name.endswith("_remap")],
              key=lambda a: a.name)
print("[load] %d source actions" % len(srcs), flush=True)
if not srcs:
    die("no source actions")

for a in srcs:
    name = a.name + "_remap"
    remap = bpy.data.actions.new(name)
    remap.use_fake_user = True
    for attr in ("starType", "frameCount", "fold"):
        try:
            setattr(remap.freehk, attr, getattr(a.freehk, attr))
        except Exception:
            pass
    remap["mhgu_source"] = a.get("mhgu_source", "")

    n_frames = int(round(a.frame_range[1]))
    # collect per-bone local quats + cog location
    src_rig.animation_data.action = a
    quat_keys = {d: [] for _, d in PAIRS}
    loc_keys = []
    for f in range(0, n_frames + 1):
        scn.frame_set(f)
        view.update()
        for s, d in PAIRS:
            q_world = src_rig.pose.bones[s].matrix.to_quaternion()
            q_dst_world = q_world @ embed[s]
            l_dst = rest_dst_inv[d] @ q_dst_world
            l_dst.normalize()
            quat_keys[d].append((f, l_dst))
        loc = src_rig.pose.bones[COG_SRC].location
        loc_keys.append((f, (loc.x, loc.y, loc.z)))
    # write
    for d, keys in quat_keys.items():
        for i in range(4):
            fc = remap.fcurves.new(data_path='pose.bones["%s"].rotation_quaternion' % d, index=i)
            fc.keyframe_points.add(len(keys))
            for k, (fr, q) in zip(fc.keyframe_points, keys):
                k.co = (fr, q[i])
            fc.update()
    for i in range(3):
        fc = remap.fcurves.new(data_path='pose.bones["%s"].location' % COG_DST, index=i)
        fc.keyframe_points.add(len(loc_keys))
        for k, (fr, v) in zip(fc.keyframe_points, loc_keys):
            k.co = (fr, v[i])
        fc.update()
    print("[retarget] %s frames=%d" % (name, n_frames), flush=True)

scn.frame_set(0)
view.update()

# sanity: cog anchor + one sampled fidelity number on the identity motion
remap = bpy.data.actions["plw_w07_106_remap"]
fy = remap.fcurves.find('pose.bones["%s"].location' % COG_DST, index=1)
print("[verify] plw_w07_106_remap cog locY[0] = %.4f (native 106 = 0.4714)"
      % fy.keyframe_points[0].co[1], flush=True)
src_act = bpy.data.actions["plw_w07_106"]
src_rig.animation_data.action = src_act
dst_rig.animation_data.action = remap
scn.frame_set(30)
view.update()
errs = [(d, round((src_rig.pose.bones[s].matrix.translation - dst_rig.pose.bones[d].matrix.translation).length, 4))
        for s, d in PAIRS]
worst = sorted(errs, key=lambda kv: -kv[1])[:5]
print("[verify] f30 src-vs-remap worst:", worst, flush=True)

bpy.ops.wm.save_mainfile()
print("[done] saved:", WORK, flush=True)
