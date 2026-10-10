# -*- coding: utf-8 -*-
"""Entry 105 (突刺): strip the b26 i-frame bit so the already-present defense
config can present. Current PMC mFlag[1] = [(0,1600),(60,576)]: 1600 = b26
(i-frames) + b22+b25 (defense base) -> no-hit wins, plays as silent i-frames.
Reference entry 112 (SAME motion LMT::w03_00::105, user-verified defense):
mFlag[1] = [(0,576),(80,512)] - no b26. New: [(0,576),(60,512)] = defense
window [0,60) then b25 base only, mirroring 112's shape. mTrig 2048@f0 stays."""
import bpy
import shutil
import time

B = r"E:\MHW-arts\MHW-Valor-0.0.2-140-swap-with-345.blend"
BAK = B + ".bak-" + time.strftime("%Y%m%d-%H%M%S") + "-pre-105-defense"
MFLAG = "FreeHKTiml.m08FD20A6"

shutil.copy2(B, BAK)
print("backup:", BAK, flush=True)
bpy.ops.wm.open_mainfile(filepath=B)
tree = bpy.data.node_groups["w03_00"]

def entry(num):
    return next(n for n in tree.nodes if n.bl_idname == "LMTEntryNode" and n.entryNum == num)

def src(nd, name):
    s = next(i for i in nd.inputs if i.name == name)
    return s.links[0].from_node if s.links else None

en = entry(105)
td = src(en, "TIML Data")
pmcs = [l.from_node.input_action for l in td.inputs["TIML Animation"].links
        if getattr(l.from_node, "input_action", None) is not None
        and l.from_node.input_action.freehk.timelineParamRaw == "m48D6114F"]
assert len(pmcs) == 2, "expected main+aux PMC on 105, got %d" % len(pmcs)
pmc = next(a for a in pmcs
           if any(fc.data_path == MFLAG for fc in a.fcurves))
print("105 main PMC: %r" % pmc.name, flush=True)

# guard: main PMC used only by 105's TIML Data node
users = set()
for n in tree.nodes:
    for inp in n.inputs:
        for l in inp.links:
            if getattr(l.from_node, "input_action", None) is pmc:
                users.add(n.name)
assert users == {td.name}, "main PMC shared beyond 105: %r" % sorted(users)

# guard: TIML Data node feeds only entry 105
feeders = [l.to_node.entryNum for l in td.outputs["TIML Data"].links
           if l.to_node.bl_idname == "LMTEntryNode"]
assert feeders == [105], "TIML Data feeds %r" % feeders

fc = next(f for f in pmc.fcurves if f.data_path == MFLAG and f.array_index == 1)
cur = [(int(k.co[0]), int(k.co[1])) for k in sorted(fc.keyframe_points, key=lambda k: k.co[0])]
print("mFlag[1] before: %s" % cur, flush=True)
assert cur == [(0, 1600), (60, 576)], "unexpected mFlag[1] state"

for kf in fc.keyframe_points:
    if int(kf.co[0]) == 0:
        kf.co = (0.0, 576.0)     # b26 off -> defense base only
    elif int(kf.co[0]) == 60:
        kf.co = (60.0, 512.0)    # window closes, b25 base remains (112 shape)
fc.update()

after = [(int(k.co[0]), int(k.co[1])) for k in sorted(fc.keyframe_points, key=lambda k: k.co[0])]
print("mFlag[1] after: %s" % after, flush=True)
assert after == [(0, 576), (60, 512)]

bpy.ops.wm.save_mainfile()
print("saved.", flush=True)
