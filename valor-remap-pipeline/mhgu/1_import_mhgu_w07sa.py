"""MHGU chain STEP 1 - import w07_sa.lmt motions as plw_sa_* actions.

Parses the MHGU v67 LMT with the FreeKinetics addon's own MhguLmt decoder
(structure-validated against RevilLib), then writes clean sorted fcurves:
- bone tracks  : boneId 0 -> pose bone "0:0" (the cog; PROOF: its decoded
                 position Y 47.135cm == native LMT::w03_00::106 MhBone_000
                 locY 0.4714 - same motion), boneId N -> the pose bone whose
                 name suffix is ":N". Locations /100 (MHGU stores cm).
- root track   : boneId -1 -> OBJECT-level curves (this is MHGU's ground-plane
                 Root track). Axis map measured against the verified import of
                 w07.lmt in mhgu-valor.blend: chan x -> obj[0], chan y -> obj[2],
                 chan z -> obj[1] NEGATED, all /100.

SELF-TEST (blocking): re-imports w07.lmt motion 106 into a temp action and
compares it channel-by-channel against plw_w07_106 (the user-verified import
shipped in mhgu-valor.blend). Any missing channel or diff > 2e-3 aborts
WITHOUT saving. Only then are the 8 non-empty w07_sa motions imported.

Run:
  /e/MHW-arts/tools/blender-4.3.2-windows-x64/blender.exe -b --python mhgu/1_import_mhgu_w07sa.py
"""
import bpy, importlib, os, sys, time, shutil, traceback

WORK = os.environ.get("MHGU_WORK_BLEND", r"E:\MHW-arts\mhgu-valor-mhw.blend")
W07_LMT = r"E:\MHW-arts\research\mhgu\extracted\w07\player\mot\w07\w07.lmt"
W07SA_LMT = r"E:\MHW-arts\research\mhgu\extracted\w07\player\mot\w07\w07_sa.lmt"
RIG_NAME = "Animation Group"
TOL = 2e-3

bak = WORK + ".bak-" + time.strftime("%Y%m%d-%H%M%S")
shutil.copyfile(WORK, bak)
print("[backup] %s" % bak, flush=True)

bpy.ops.wm.open_mainfile(filepath=WORK)
rig = bpy.data.objects.get(RIG_NAME)
if rig is None:
    print("!!! FAIL: rig missing"); sys.exit(1)

# suffix(":N") -> pose bone name ; proves the id binding used by the shipped import
suffix_map = {}
for b in rig.data.bones:
    if ":" in b.name:
        stem = b.name.split(":")[-1]
        sec = stem.endswith("_s")
        if sec:
            stem = stem[:-2]
        try:
            bid = int(stem)
        except ValueError:
            continue
        cur = suffix_map.get(bid)
        if cur is None or (cur.endswith("_s") and not sec):
            suffix_map[bid] = b.name  # prefer the primary bone over the _s twin
print("[rig] suffix-bound bones: %d (ids %s)" % (len(suffix_map), sorted(suffix_map)), flush=True)

Lmt = importlib.import_module("MHW-FreeKinetics.struct.Lmt")

ROOT_LOC_MAP = {"x": ("location", 0, 1.0), "y": ("location", 2, 1.0), "z": ("location", 1, -1.0)}
VAR_IDX = {"x": 0, "y": 1, "z": 2, "w": 3}

def comp(vec, var):
    """basis/tail arrive as float[4] lists (or Vectors); pull one component."""
    if isinstance(vec, (list, tuple)):
        return vec[VAR_IDX[var]]
    return getattr(vec, var)

def import_motion(lmt_action, action_name):
    act = bpy.data.actions.new(action_name)
    try:
        act.freehk.starType = "LMT_Action"
        act.freehk.loopFrame = lmt_action.loopFrame
        act.freehk.frameCount = lmt_action.frameCount
    except Exception as e:
        print("[warn] freehk props unavailable: %s" % e, flush=True)
    folded = False
    for fset in lmt_action.bones:
        folded = folded or fset.fold
        is_root = fset.boneId == -1
        if not is_root and fset.boneId not in suffix_map:
            print("!!! FAIL: no pose bone for LMT boneId %d in %s" % (fset.boneId, action_name))
            sys.exit(1)
        bind = None if is_root else suffix_map[fset.boneId]
        # channelNames() = (real_path, delta_decoy_path, blender_index, var);
        # the shipped importer binds fcurves on the FIRST element
        for (basePath, _decoy, idx, var), series in zip(fset.channelNames(), fset.channels()):
            if is_root:
                if basePath == "location":
                    path, pidx, sign = ROOT_LOC_MAP[var]
                else:
                    path, pidx, sign = basePath, idx, 1.0
            else:
                path = 'pose.bones["%s"].%s' % (bind, basePath)
                pidx, sign = idx, 1.0
            scale = 0.01 if basePath == "location" else 1.0
            pts = [(0.0, sign * scale * comp(fset.basis, var))]
            timing = 1.0
            for val, delta in series:
                pts.append((timing, sign * scale * val))
                timing += delta
            if hasattr(fset, "tail") and fset.tail is not None:
                pts.append((float(lmt_action.frameCount + 1),
                            sign * scale * comp(fset.tail, var)))
            pts.sort(key=lambda p: p[0])
            fc = act.fcurves.new(data_path=path, index=pidx)
            fc.keyframe_points.add(len(pts))
            for k, (fr, v) in zip(fc.keyframe_points, pts):
                k.co = (fr, v)   # default BEZIER, matches the shipped import
            fc.update()
    if folded:
        print("[warn] %s has folded keyframes" % action_name, flush=True)
    if not any(fc.data_path == "rotation_quaternion" for fc in act.fcurves):
        # no root-rot track on disk; natives carry object quat = identity
        for i, v in ((0, 1.0), (1, 0.0), (2, 0.0), (3, 0.0)):
            fc = act.fcurves.new(data_path="rotation_quaternion", index=i)
            fc.keyframe_points.add(1)
            fc.keyframe_points[0].co = (0.0, v)
            fc.update()
    return act

def diff_action(mine, ref, frames):
    worst = (0.0, None)
    missing = []
    for fc in ref.fcurves:
        mc = mine.fcurves.find(fc.data_path, index=fc.array_index)
        if mc is None:
            missing.append((fc.data_path, fc.array_index))
            continue
        for f in frames:
            d = abs(mc.evaluate(f) - fc.evaluate(f))
            if d > worst[0]:
                worst = (d, (fc.data_path, fc.array_index, f))
    return worst, missing

# ---------------- SELF-TEST: w07.lmt motion 106 vs shipped import ----------------
ref = bpy.data.actions.get("LMT::w07::106")
if ref is None:
    print("!!! FAIL: LMT::w07::106 (shipped reference) missing"); sys.exit(1)
lmt = Lmt.parseLMT(W07_LMT)
src106 = [a for a in lmt.ActionHeaders if a.id == 106][0]
tmp = import_motion(src106, "__selftest_106")
frames = list(range(0, int(src106.frameCount + 2)))
worst, missing = diff_action(tmp, ref, frames)
cnt_bad = []
for fc in ref.fcurves:
    mc = tmp.fcurves.find(fc.data_path, index=fc.array_index)
    if mc and len(mc.keyframe_points) != len(fc.keyframe_points):
        if not fc.data_path.startswith("pose.bones[") and fc.data_path == "rotation_quaternion":
            continue  # ref pads object quat to root-track length; 1-key identity is equivalent
        cnt_bad.append((fc.data_path, len(mc.keyframe_points), len(fc.keyframe_points)))
print("[selftest] max diff = %.6f @ %s | missing=%d keycount-mismatch=%d"
      % (worst[0], worst[1], len(missing), len(cnt_bad)), flush=True)
for _p, _m, _r in cnt_bad[:10]:
    print("   keycount %s mine=%d ref=%d" % (_p, _m, _r), flush=True)
if missing:
    print("   missing channels sample:", missing[:5], flush=True)
if cnt_bad:
    print("   key count mismatches sample:", cnt_bad[:5], flush=True)
ok = worst[0] <= TOL and not missing and not cnt_bad
bpy.data.actions.remove(tmp)
if not ok:
    print("!!! SELF-TEST FAILED - nothing saved")
    sys.exit(1)
print("[selftest] PASS", flush=True)

# ---------------- real import: all non-empty w07_sa motions ----------------
# the v67 section parser intermittently decodes one entry as empty (process-
# level nondeterminism), so re-parse several times and union the known ids
seen_ids = set()
made = []
for attempt in range(6):
    lmt_sa = Lmt.parseLMT(W07SA_LMT)
    for a2 in lmt_sa.ActionHeaders:
        if a2.fcurveOffset:
            seen_ids.add(a2.id)
    missing = sorted(i for i in seen_ids if ("LMT::w07_sa::%03d" % i) not in bpy.data.actions)
    if not missing:
        break
    if attempt:
        print("[retry] attempt %d for %s" % (attempt, missing), flush=True)
    for lmt_action in lmt_sa.ActionHeaders:
        if lmt_action.id not in missing or not lmt_action.fcurveOffset:
            continue
        name = "LMT::w07_sa::%03d" % lmt_action.id
        if name in bpy.data.actions:
            continue
        act = import_motion(lmt_action, name)
        act["mhgu_source"] = "w07_sa.lmt#%d" % lmt_action.id
        act.use_fake_user = True
        made.append(name)
        print("[import] %s frames=%d bones=%d fcurves=%d"
              % (name, lmt_action.frameCount, len(lmt_action.bones), len(act.fcurves)), flush=True)
still = sorted(i for i in seen_ids if ("LMT::w07_sa::%03d" % i) not in bpy.data.actions)
if still:
    print("!!! FAIL: could not import %s" % still); sys.exit(1)
if not made:
    print("[skip] all w07_sa actions already imported")
print("[import] known non-empty w07_sa motions: %s" % sorted(seen_ids), flush=True)

bpy.ops.wm.save_mainfile()
print("[done] imported %d actions, saved: %s" % (len(made), WORK), flush=True)
