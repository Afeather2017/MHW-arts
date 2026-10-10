# -*- coding: utf-8 -*-
"""Entry 105 (thrust): restore the USER-VERIFIED guard config from 105.txt
(git 11f1c38, "matches the user's earlier working experiment").
mFlag: f0=0x87400006 (b31+b26+b24+b25+b22+low6) for 18f (0.3s), then
f18=0x02000006 (b25+6), f120=6. mTrigger 2048@f0 stays. Split-channel form:
  mFlag[0] = 6 constant            mFlag[1] = [(0,34624),(18,512),(120,0)]
Current state (my previous edit): mFlag[1] = [(0,576),(60,512)]."""
import bpy
import shutil
import time

B = r"E:\MHW-arts\MHW-Valor-0.0.2-140-swap-with-345.blend"
BAK = B + ".bak-" + time.strftime("%Y%m%d-%H%M%S") + "-pre-105-guard105txt"
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
feeders = [l.to_node.entryNum for l in td.outputs["TIML Data"].links
           if l.to_node.bl_idname == "LMTEntryNode"]
assert feeders == [105], "TIML Data feeds %r" % feeders

pmcs = [l.from_node.input_action for l in td.inputs["TIML Animation"].links
        if getattr(l.from_node, "input_action", None) is not None
        and l.from_node.input_action.freehk.timelineParamRaw == "m48D6114F"]
pmc = next(a for a in pmcs if any(fc.data_path == MFLAG for fc in a.fcurves))
users = set()
for n in tree.nodes:
    for inp in n.inputs:
        for l in inp.links:
            if getattr(l.from_node, "input_action", None) is pmc:
                users.add(n.name)
assert users == {td.name}, "main PMC shared: %r" % sorted(users)
print("105 main PMC: %r" % pmc.name, flush=True)

def rebuild(index, keys):
    for fc in list(pmc.fcurves):
        if fc.data_path == MFLAG and fc.array_index == index:
            pmc.fcurves.remove(fc)
    fc = pmc.fcurves.new(data_path=MFLAG, index=index)
    fc.keyframe_points.add(len(keys))
    for i, (fr, v) in enumerate(keys):
        kf = fc.keyframe_points[i]
        kf.co = (float(fr), float(v))
        kf.interpolation = 'CONSTANT'
    fc.update()
    print("  mFlag[%d] <- %s" % (index, keys), flush=True)

hi = next(f for f in pmc.fcurves if f.data_path == MFLAG and f.array_index == 1)
cur = [(int(k.co[0]), int(k.co[1])) for k in sorted(hi.keyframe_points, key=lambda k: k.co[0])]
print("mFlag[1] before: %s" % cur, flush=True)
assert cur == [(0, 576), (60, 512)], "unexpected state"

rebuild(0, [(0.0, 6.0), (18.0, 6.0), (120.0, 6.0)])
rebuild(1, [(0.0, 34624.0), (18.0, 512.0), (120.0, 0.0)])

for idx in (0, 1):
    fc = next(f for f in pmc.fcurves if f.data_path == MFLAG and f.array_index == idx)
    print("  verify mFlag[%d] = %s" % (idx,
        [(int(k.co[0]), int(k.co[1])) for k in sorted(fc.keyframe_points, key=lambda k: k.co[0])]), flush=True)

bpy.ops.wm.save_mainfile()
print("saved.", flush=True)
