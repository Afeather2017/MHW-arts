# -*- coding: utf-8 -*-
"""Entry 345 v2: switch the 12f defense window from silent i-frames (b26+trig2048,
v1) to super-armor presentation (b24, no trigger) per vanilla entry 306 recipe
(mFlag f10=0x03400000 -> end, no gate) - user wants the armor thud, which only
plays when the hit actually connects (b26 no-hit suppresses the interaction).
"""
import bpy
import shutil
import time

B = r"E:\MHW-arts\MHW-Valor-0.0.2-140-swap-with-345.blend"
BAK = B + ".bak-" + time.strftime("%Y%m%d-%H%M%S") + "-pre-345-armor"
ENTRY = 345
ARMOR_HIGH = 256.0       # 0x01000000 >> 16 = b24 super armor
WINDOW_OFF = 12.0        # keep v1 window shape [0,12) = 0.2s @ 60fps

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
pmc = None
for l in td.inputs["TIML Animation"].links:
    a = getattr(l.from_node, "input_action", None)
    if a and a.freehk.timelineParamRaw == "m48D6114F":
        pmc = a
assert pmc is not None, "PlMotionCommon action not found on 345"
print("PMC action: %r (users check)" % pmc.name, flush=True)

users = set()
for n in tree.nodes:
    for inp in n.inputs:
        for l in inp.links:
            if getattr(l.from_node, "input_action", None) is pmc:
                users.add(n.name)
assert users == {td.name}, "PMC action shared beyond 345: %r" % sorted(users)

MFLAG, MTRIG = "FreeHKTiml.m08FD20A6", "FreeHKTiml.m99897C43"

def cur(path, index):
    for fc in pmc.fcurves:
        if fc.data_path == path and fc.array_index == index:
            return [(int(k.co[0]), int(k.co[1])) for k in sorted(fc.keyframe_points, key=lambda k: k.co[0])]
    return None

# v1 state check before overwriting
assert cur(MFLAG, 1) == [(0, 1024), (12, 0)], "unexpected mFlag[1]: %r" % cur(MFLAG, 1)
assert cur(MTRIG, 0) == [(0, 2048)], "unexpected mTrig[0]: %r" % cur(MTRIG, 0)

def rebuild(action, path, index, keys):
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

rebuild(pmc, MFLAG, 0, [(0.0, 0.0), (WINDOW_OFF, 0.0)])          # low16 stays clean
rebuild(pmc, MFLAG, 1, [(0.0, ARMOR_HIGH), (WINDOW_OFF, 0.0)])   # b24 super armor window
rebuild(pmc, MTRIG, 0, [(0.0, 0.0)])                             # no gate (306 style: f0=0)
rebuild(pmc, MTRIG, 1, [(0.0, 0.0)])

for path, idx in ((MFLAG, 0), (MFLAG, 1), (MTRIG, 0), (MTRIG, 1)):
    print("  verify %s[%d] keys=%s" % (path, idx, cur(path, idx)), flush=True)

bpy.ops.wm.save_mainfile()
print("saved.", flush=True)
