# -*- coding: utf-8 -*-
"""Entry 345 v4: clone vanilla entry 112's PROVEN defense recipe onto the [0,30)
window. 112/306/344 all share the base signature low16=6 + high16 b22+b25 + a
mTrigger 2048 gate; b24/b26 alone never present defense (v1=b26+gate played as
pure i-frames, v2/v3=b24 without gate never armed). Extras (mFlag4=4,
mWpSnowShovelCurvePower) left untouched this round - one variable group at a time.
"""
import bpy
import shutil
import time

B = r"E:\MHW-arts\MHW-Valor-0.0.2-140-swap-with-345.blend"
BAK = B + ".bak-" + time.strftime("%Y%m%d-%H%M%S") + "-pre-345-112recipe"
ENTRY = 345
LOW6 = 6.0               # low16 b1+b2, carried whole move (112/344 style)
DEF_BASE = 576.0         # 0x02400000 >> 16 = b22+b25 defense base
WINDOW_OFF = 30.0        # 0.5s @ 60fps
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
pmc = None
for l in td.inputs["TIML Animation"].links:
    a = getattr(l.from_node, "input_action", None)
    if a and a.freehk.timelineParamRaw == "m48D6114F":
        pmc = a
assert pmc is not None, "PlMotionCommon action not found on 345"
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

# v3 state check before overwriting
assert cur(MFLAG, 1) == [(0, 256), (30, 0)], "unexpected mFlag[1]: %r" % cur(MFLAG, 1)
assert cur(MTRIG, 0) == [(0, 0)], "unexpected mTrig[0]: %r" % cur(MTRIG, 0)

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

rebuild(pmc, MFLAG, 0, [(0.0, LOW6), (WINDOW_OFF, LOW6)])           # low16 6, holds past window
rebuild(pmc, MFLAG, 1, [(0.0, DEF_BASE), (WINDOW_OFF, 0.0)])        # b22+b25 window, off at 30
rebuild(pmc, MTRIG, 0, [(0.0, TRIGGER_VAL)])                        # gate, 112-style single key
rebuild(pmc, MTRIG, 1, [(0.0, 0.0)])

for path, idx in ((MFLAG, 0), (MFLAG, 1), (MTRIG, 0), (MTRIG, 1)):
    print("  verify %s[%d] keys=%s" % (path, idx, cur(path, idx)), flush=True)

bpy.ops.wm.save_mainfile()
print("saved.", flush=True)
