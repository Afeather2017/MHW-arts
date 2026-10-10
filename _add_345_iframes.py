# -*- coding: utf-8 -*-
"""Entry 345: 0.2s i-frame window at start (12f @ 60fps), recipe = _add_312_iframes.

- i-frames = PlMotionCommon mFlag b26 (0x04000000) window [0,12) + ONE mTrigger
  2048 key at f0 (gate, position-free). 345's PMC (PlMotionCommon.348) currently
  has NO mFlag/mTrigger tracks -> pure additions, no rebuild of existing keys.
- Entry 345's TIML Data node is its own (animLength=142, motion=192_remap);
  344/140 use separate PMC actions (.435/.436) and are untouched.
"""
import bpy
import shutil
import time

B = r"E:\MHW-arts\MHW-Valor-0.0.2-140-swap-with-345.blend"
BAK = B + ".bak-" + time.strftime("%Y%m%d-%H%M%S") + "-pre-345-defense"
ENTRY = 345
IFRAME_HIGH = 1024.0     # 0x04000000 >> 16
WINDOW_OFF = 12.0        # 0.2s * 60fps; window = [0,12), off-key at 12
TRIGGER_VAL = 2048.0

shutil.copy2(B, BAK)
print("backup:", BAK, flush=True)
bpy.ops.wm.open_mainfile(filepath=B)
tree = bpy.data.node_groups["w03_00"]

def entry_node(num):
    for n in tree.nodes:
        if n.bl_idname == "LMTEntryNode" and n.entryNum == num:
            return n
    raise SystemExit("entry node %d not found" % num)

def src(nd, name):
    s = next((i for i in nd.inputs if i.name == name), None)
    return s.links[0].from_node if s and s.links else None

en = entry_node(ENTRY)
td = src(en, "TIML Data")
assert td is not None, "entry 345 has no TIML Data link"
print("TIML Data node: %r animLength=%r" % (td.name, td.animLength), flush=True)

pmc = None
for l in td.inputs["TIML Animation"].links:
    a = getattr(l.from_node, "input_action", None)
    if a and a.freehk.timelineParamRaw == "m48D6114F":
        pmc = a
assert pmc is not None, "PlMotionCommon action not found on 345"
print("PMC action: %r" % pmc.name, flush=True)

# --- guard 1: no pre-existing mFlag/mTrig on this PMC (unexpected state -> stop) ---
MFLAG, MTRIG = "FreeHKTiml.m08FD20A6", "FreeHKTiml.m99897C43"
pre = [fc for fc in pmc.fcurves if fc.data_path in (MFLAG, MTRIG)]
assert not pre, "PMC already has mFlag/mTrig fcurves: %r - inspect before overwrite" % [
    (fc.data_path, fc.array_index) for fc in pre]

# --- guard 2: PMC action must be used only by 345's TIML Data node ---
users = set()
for n in tree.nodes:
    for inp in n.inputs:
        for l in inp.links:
            a = getattr(l.from_node, "input_action", None)
            if a is pmc:
                users.add(n.name)
print("PMC action users (nodes): %r" % sorted(users), flush=True)
assert users == {td.name}, "PMC action shared beyond 345's TIML node - aborting"

def rebuild(action, path, index, keys):
    """delete + recreate one fcurve with exact CONSTANT keys"""
    for fc in list(action.fcurves):
        if fc.data_path == path and fc.array_index == index:
            action.fcurves.remove(fc)
    fc = action.fcurves.new(data_path=path, index=index)
    fc.keyframe_points.add(len(keys))
    for i, (fr, v) in enumerate(keys):
        kf = fc.keyframe_points[i]
        kf.co = (float(fr), float(v))
        kf.interpolation = 'CONSTANT'
    fc.update()
    print("  %s[%d] <- %s" % (path, index, [(int(f), int(v)) for f, v in keys]), flush=True)

print("adding defense window:", flush=True)
rebuild(pmc, MFLAG, 0, [(0.0, 0.0), (WINDOW_OFF, 0.0)])           # low16 (clean)
rebuild(pmc, MFLAG, 1, [(0.0, IFRAME_HIGH), (WINDOW_OFF, 0.0)])   # high16 = b26 window
rebuild(pmc, MTRIG, 0, [(0.0, TRIGGER_VAL)])                      # gate key inside window
rebuild(pmc, MTRIG, 1, [(0.0, 0.0)])

# readback
for fc in pmc.fcurves:
    if fc.data_path in (MFLAG, MTRIG):
        kfs = [(int(k.co[0]), int(k.co[1])) for k in sorted(fc.keyframe_points, key=lambda k: k.co[0])]
        print("  verify %s[%d] keys=%s" % (fc.data_path, fc.array_index, kfs), flush=True)

bpy.ops.wm.save_mainfile()
print("saved.", flush=True)
