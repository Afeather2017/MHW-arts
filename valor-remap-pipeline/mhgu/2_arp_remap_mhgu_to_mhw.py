"""MHGU chain STEP 2 - ARP batch remap MHGU 'Animation Group' -> MHW rig.

PURE batch ARP pass (user-verified baseline). The shipped mhgu-to-mhw.bmap
is used VERBATIM: 0:0 -> waist MhBone_013 (rotation only), reference ->
MhBone_000 (root). No repointing: MhBone_000 gets NO rotation (native MHW
never rotates it - routing 0:0's quat there tilted the whole body) and no
location (height lives at 0 in this baseline; the cog location transfer is a
separate later step). MHGU Root-track travel stays parked in TRAVEL::* actions
(extracted below so the source object never moves during the bake).

Run:
  /e/MHW-arts/tools/blender-4.3.2-windows-x64/blender.exe -b --python mhgu/2_arp_remap_mhgu_to_mhw.py
"""
import bpy, addon_utils, sys, traceback, shutil, time, os

ONLY = os.environ.get("MHGU_ONLY", "")   # surgical mode: remap this one action only

WORK = os.environ.get("MHGU_WORK_BLEND", r"E:\MHW-arts\mhgu-valor-mhw.blend")
BMAP = r"E:\MHW-arts\mhgu-to-mhw.bmap"
SRC_NAME = "Animation Group"      # MHGU skeleton, owns the plw_ actions
DST_NAME = "m_mesh Armature"      # MHW skeleton (MhBone_xxx)
FILTER = ""   # unused for selection; marking goes via the mhgu_source prop

def die(msg):
    print("!!! FAIL:", msg)
    sys.exit(1)

bak = WORK + ".bak-" + time.strftime("%Y%m%d-%H%M%S")
shutil.copyfile(WORK, bak)
print("[backup] %s" % bak, flush=True)

# ---------------------------------------------------------------- enable ARP
loaded_default, loaded_state = addon_utils.check("auto_rig_pro")
if not loaded_state:
    addon_utils.enable("auto_rig_pro", default_set=True, persistent=True)
loaded_default, loaded_state = addon_utils.check("auto_rig_pro")
print("[arp] enabled:", loaded_state)
try:
    bpy.ops.arp.retarget.get_rna_type()
except Exception as e:
    die("arp.retarget operator unavailable: %s" % e)

bpy.ops.wm.open_mainfile(filepath=WORK)
scn = bpy.context.scene
src = bpy.data.objects.get(SRC_NAME)
dst = bpy.data.objects.get(DST_NAME)
if not src or src.type != "ARMATURE": die("source object missing: " + SRC_NAME)
if not dst or dst.type != "ARMATURE": die("target object missing: " + DST_NAME)
src_bones = set(b.name for b in src.data.bones)
dst_bones = set(b.name for b in dst.data.bones)
print(f"[rigs] {SRC_NAME}: {len(src_bones)} bones | {DST_NAME}: {len(dst_bones)} bones")

# ---------------------------------------------------------------- parse bmap
raw = open(BMAP, encoding="utf8").read().splitlines()
records, cur = [], None
for line in raw:
    line = line.rstrip("\r\n")
    if line.strip() == "":
        continue
    if "%" in line:
        cur = [line]
        records.append(cur)
    else:
        if cur is None:
            die("bmap: data line before any header: %r" % line)
        cur.append(line)

entries = []
for rec in records:
    parts = rec[0].rstrip("%").split("%")
    entries.append({
        "target": parts[0],
        "source": rec[1],
        "root":  (rec[2].strip() == "True") if len(rec) > 2 else False,
        "ik":    (rec[3].strip() == "True") if len(rec) > 3 else False,
        "pole":  rec[4].strip() if len(rec) > 4 else "",
        "settings": parts[1:],
    })
print(f"[bmap] records: {len(entries)}")

# bmap used VERBATIM (user-verified mapping: 0:0 -> waist MhBone_013,
# reference -> MhBone_000 root). No repointing: cog rotation must go to
# the waist (native MHW never rotates MhBone_000 - it tilted the body).

miss_src = sorted({e["source"] for e in entries if e["source"] not in src_bones})
miss_dst = sorted({e["target"] for e in entries if e["target"] not in dst_bones})
if miss_src: die("bmap source bones missing on %s: %s" % (SRC_NAME, miss_src))
if miss_dst: die("bmap target bones missing on %s: %s" % (DST_NAME, miss_dst))
seen_t, seen_s = {}, {}
for e in entries:
    seen_t[e["target"]] = seen_t.get(e["target"], 0) + 1
    seen_s[e["source"]] = seen_s.get(e["source"], 0) + 1
dupes = {k: v for k, v in seen_t.items() if v > 1}
if dupes: die("bmap duplicate target bones after repoint: %s" % dupes)
dupes = {k: v for k, v in seen_s.items() if v > 1}
if dupes: die("bmap duplicate source bones: %s" % dupes)
print(f"[bmap] coverage OK: {len(entries)} pairs, all bones present")

# ---------------------------------------------------------------- extract Root-track object curves
# The MHGU Root track (ground travel) lives as OBJECT-level fcurves on the
# source actions. If they stay there during retarget, the SOURCE OBJECT moves
# while ARP samples it and the travel gets baked INTO MhBone_000's location
# (measured: rem cog y 0.816 = cog 0.043 + object 0.773 at f60). Extract them
# into a sibling <name>_travel action; 4_object_root_motion_mhgu.py consumes
# them from there. gm_* GLB sources have no object curves - nothing to do.
extracted = 0
for a in list(bpy.data.actions):
    if not ("mhgu_source" in a and not a.name.endswith("_remap")):
        continue
    obj_fcs = [fc for fc in a.fcurves if not fc.data_path.startswith("pose.bones[")]
    if not obj_fcs:
        continue
    travel = bpy.data.actions.get("TRAVEL::" + a.name)
    if travel is None:
        travel = bpy.data.actions.new("TRAVEL::" + a.name)
        travel.use_fake_user = True
    for fc in obj_fcs:
        old = travel.fcurves.find(fc.data_path, index=fc.array_index)
        if old is not None:
            travel.fcurves.remove(old)
        nf = travel.fcurves.new(data_path=fc.data_path, index=fc.array_index)
        nf.keyframe_points.add(len(fc.keyframe_points))
        for k, sk in zip(nf.keyframe_points, fc.keyframe_points):
            k.co = sk.co
        nf.update()
        a.fcurves.remove(fc)
        extracted += 1
print(f"[travel] extracted {extracted} object fcurves into *_travel actions", flush=True)

# ---------------------------------------------------------------- set rigs
if not (src.animation_data and src.animation_data.action):
    src.animation_data_create()
    first = next((a for a in bpy.data.actions if a.name.startswith(FILTER)), None)
    if not first: die("no source action to assign")
    src.animation_data.action = first
scn.source_rig = SRC_NAME
scn.target_rig = DST_NAME
print(f"[scene] source_rig={scn.source_rig} target_rig={scn.target_rig} global_scale={scn.global_scale}")

# ---------------------------------------------------------------- bones map
scn.arp_remap_allow_root_update = False
bm = scn.bones_map_v2
while len(bm):
    bm.remove(0)
for e in entries:
    it = bm.add()
    it.source_bone = e["source"]
    it.name = e["target"]
    it.set_as_root = e["root"]
    it.ik = e["ik"]
    it.ik_pole = e["pole"]
    if len(e["settings"]) == 8:
        s = e["settings"]
        it.location = (s[0] == "True")
        it.ik_auto_pole = s[1]
        it.rot_add = tuple(float(x) for x in s[2].split(","))
        it.loc_add = tuple(float(x) for x in s[3].split(","))
        it.loc_mult = float(s[4])
        it.ik_create_constraints = (s[5] == "True")
        it.ik_world = (s[6] == "True")
        it.IK_axis_correc = s[7]
scn.arp_remap_allow_root_update = True
scn.bones_map_index = 0
print(f"[scene] bones_map_v2 populated: {len(bm)} items")

# ---------------------------------------------------------------- mark actions
marked = 0
for a in bpy.data.actions:
    want = ("mhgu_source" in a and not a.name.endswith("_remap")
            and (not ONLY or a.name == ONLY))
    a["arp_remap"] = want
    marked += int(want)
print(f"[actions] marked for remap: {marked}/{len(bpy.data.actions)} (filter '{FILTER}')")
if marked == 0: die("no actions marked")

scn.batch_retarget = True
scn.arp_remap_only_containing = ''   # empty = no name filter; the arp_remap flag decides

# ---------------------------------------------------------------- selection
for ob in bpy.context.view_layer.objects:
    ob.select_set(False)
for ob in (src, dst):
    ob.hide_viewport = False
    ob.hide_set(False)
    ob.select_set(True)
bpy.context.view_layer.objects.active = dst

# ---------------------------------------------------------------- run
src_lens = [bpy.data.actions[a.name[:-len("_remap")]].frame_range[1]
            for a in bpy.data.actions if a.name.startswith(FILTER)
            and a.name[:-len("_remap")] in bpy.data.actions] if False else []
maxlen = 1
for a in bpy.data.actions:
    if "mhgu_source" in a and not a.name.endswith("_remap"):
        maxlen = max(maxlen, a.frame_range[1])
scn.frame_start = 0
scn.frame_end = int(maxlen) + 1
print(f"[scene] frame range pinned to 0..{scn.frame_end} (longest source action)", flush=True)
print("[run] arp.retarget batch ...", flush=True)
try:
    bpy.ops.arp.retarget('EXEC_DEFAULT', frame_start=0, frame_end=1)
except Exception:
    traceback.print_exc()
    die("arp.retarget raised")

# ---------------------------------------------------------------- verify
import re
def is_mhgu_remap(a):
    return a.name.endswith("_remap") and "mhgu_source" in bpy.data.actions.get(a.name[:-len("_remap")], "")
remaps = sorted([a for a in bpy.data.actions if is_mhgu_remap(a)], key=lambda a: a.name)
print(f"[verify] remap actions created: {len(remaps)} (expected {marked})")
bad = [a.name for a in remaps if "mhgu_source" not in a]
if bad: print("[verify] WARN non-plw remap actions:", bad[:5])
sample = remaps[0] if remaps else None
if sample:
    bones_hit = set()
    for fc in sample.fcurves:
        m = re.match(r'pose\.bones\["([^"]+)"\]', fc.data_path)
        if m: bones_hit.add(m.group(1))
    print(f"[verify] sample '{sample.name}': frames {sample.frame_range[0]:.0f}..{sample.frame_range[1]:.0f}, "
          f"fcurves={len(sample.fcurves)}, target bones used={len(bones_hit)} sample={sorted(bones_hit)[:8]}")
    non_mhw = [b for b in bones_hit if not b.startswith("MhBone")]
    if non_mhw: print("[verify] WARN non-MhBone channels:", non_mhw[:8])
if len(remaps) < marked:
    print(f"[verify] WARN expected {marked}, got {len(remaps)}")

# ---------------------------------------------------------------- trim to source range
# ARP bakes every remap over the SCENE frame range (master file leaves
# frame_end=122), so short actions get extrapolated junk keys past their own
# length; and ARP-created actions carry no freehk metadata (frameCount=-1),
# which would corrupt LMT export lengths. Trim + copy metadata per remap.
trimmed = 0
for a in remaps:
    src = bpy.data.actions.get(a.name[:-len("_remap")])
    if src is None:
        print("[trim] WARN no source for", a.name)
        continue
    a["mhgu_source"] = src.get("mhgu_source", "")
    hi = src.frame_range[1]
    for fc in a.fcurves:
        ks = fc.keyframe_points
        dead = [k for k in ks if k.co[0] > hi + 1e-6]
        for k in dead:
            ks.remove(k)
        if dead:
            fc.update()
            trimmed += 1
    for attr in ("starType", "frameCount", "fold", "loopFrame"):
        try:
            setattr(a.freehk, attr, getattr(src.freehk, attr))
        except AttributeError:
            pass
        except Exception as e:
            print("[trim] WARN freehk.%s copy failed on %s: %s" % (attr, a.name, e))
    a.use_fake_user = True
print(f"[trim] curves trimmed on {trimmed} fcurves; freehk metadata copied", flush=True)

# (no cog anchor check in pure-ARP mode: 000 carries no location by design)

bpy.ops.wm.save_mainfile()
print("[done] saved:", WORK, flush=True)
