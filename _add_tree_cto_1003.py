# -*- coding: utf-8 -*-
"""Add tree-side CollisionTimelineObject (damage window) to entries 339/340/343.

Mirrors entry 140 (刚1) exactly: its CTO lives in the blend tree as a
TIML::nTimelineParam::CollisionTimelineObject action
(freehk.starType=TIML_Action, timelineParamRaw=m38FF82EA) with two transforms
- FreeHKTiml.mB296CA66 [0] = node id 63 (vanilla 見切り攻撃 col row, the same
row working 刚1 routes to) and FreeHKTiml.m08FD20A6 (mFlag, split 16/16)
carrying the damage window. 刚1 boots + damages with this structure, so the
same construction on the three broken entries is boot-safe by construction.

Windows (user closed intervals -> on-key at a, off-key at b+1):
  339 一文字短 [17,22]; 340 一文字长 [29,34]; 343 无双斩 [31,39]+[58,70].
"""
import bpy
import shutil
import time

MASTER = r"E:\MHW-arts\MHW-Valor.blend"
BAK = MASTER + ".bak-" + time.strftime("%Y%m%d-%H%M%S") + "-pre-tree-cto"
PLAN = {
    339: [(17, 23)],
    340: [(29, 35)],
    343: [(31, 40), (58, 71)],
}
NODE_ID = 63.0
PARAM = "m38FF82EA"          # CollisionTimelineObject timeline-param hash (same as entry 140)
NODEID_PATH = "FreeHKTiml.mB296CA66"
MFLAG_PATH = "FreeHKTiml.m08FD20A6"

shutil.copy2(MASTER, BAK)
print("backup:", BAK, flush=True)
bpy.ops.wm.open_mainfile(filepath=MASTER)
tree = bpy.data.node_groups["w03_00"]

# entry node -> TIML Data node map
entry_dn = {}
for n in tree.nodes:
    if n.bl_idname != "LMTEntryNode" or n.entryNum not in PLAN:
        continue
    links = n.inputs["TIML Data"].links
    assert links, "entry %d has no TIML Data link" % n.entryNum
    dn = links[0].from_node
    assert dn.bl_idname == "TIMLDataNode"
    entry_dn[n.entryNum] = dn
assert set(entry_dn) == set(PLAN), entry_dn

for eid, wins in sorted(PLAN.items()):
    dn = entry_dn[eid]
    # refuse to duplicate
    have = [l.from_node for l in dn.inputs["TIML Animation"].links
            if getattr(getattr(l.from_node, "input_action", None), "freehk", None) is not None
            and l.from_node.input_action.freehk.timelineParamRaw == PARAM]
    assert not have, "entry %d already has a CTO action: %s" % (eid, have)

    act = bpy.data.actions.new("TIML::nTimelineParam::CollisionTimelineObject")
    act.freehk.starType = "TIML_Action"
    act.freehk.timelineParamRaw = PARAM
    try:
        act.freehk.timelineParam = PARAM
    except TypeError:
        pass
    act.freehk.unkn0 = 0

    fc_node = act.fcurves.new(data_path=NODEID_PATH, index=0)
    fc_node.keyframe_points.add(1)
    kf = fc_node.keyframe_points[0]
    kf.co = (0.0, NODE_ID)
    kf.interpolation = 'CONSTANT'
    fc_node.update()

    k0 = [(0.0, 0.0)]
    for on_f, off_f in wins:
        k0.append((float(on_f), 1.0))
        k0.append((float(off_f), 0.0))
    k1 = [(f, 0.0) for f, _ in k0]
    for index, keys in ((0, k0), (1, k1)):
        fc = act.fcurves.new(data_path=MFLAG_PATH, index=index)
        fc.keyframe_points.add(len(keys))
        for i, (co, v) in enumerate(keys):
            kf = fc.keyframe_points[i]
            kf.co = (co, v)
            kf.interpolation = 'CONSTANT'
        fc.update()

    node = tree.nodes.new("TIMLActionNode")
    node.input_action = act
    node.location = (dn.location.x - 260.0, dn.location.y - 40.0 - 220.0 * (eid % 3))
    tree.links.new(node.outputs["TIML Animation"], dn.inputs["TIML Animation"])
    print("entry %d: CTO action %r added, node %r, windows %s, node_id %g" %
          (eid, act.name, node.name, wins, NODE_ID), flush=True)

bpy.ops.wm.save_mainfile()
print("saved.", flush=True)
