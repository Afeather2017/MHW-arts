# -*- coding: utf-8 -*-
"""Bisect 312 crash: keep 095_remap motion, revert TIML to the old (native sheathe) family."""
import bpy
import shutil
import time

MASTER = r"E:\MHW-arts\MHW-Valor.blend"
BAK = MASTER + ".bak-" + time.strftime("%Y%m%d-%H%M%S") + "-pre-312-oldtiml"
OLD = [
    "TIML::nTimelineParam::PlMotionCommon.413",
    "TIML::nTimelineParam::PlMotionInput.197",
    "TIML::nTimelineParam::PlMotionVisual.125",
    "TIML::nTimelineParam::nWwiseTimeline::EventCollision00.243",
    "TIML::nTimelineParam::nWwiseTimeline::EventGroup01.254",
    "TIML::nTimelineParam::nWwiseTimeline::EventGroup06.219",
    "TIML::nTimelineParam::nWwiseTimeline::GameParameter.252",
]

shutil.copy2(MASTER, BAK)
print("backup:", BAK, flush=True)
bpy.ops.wm.open_mainfile(filepath=MASTER)
tree = bpy.data.node_groups["w03_00"]

def entry_node(num):
    for n in tree.nodes:
        if n.bl_idname == "LMTEntryNode" and n.entryNum == num:
            return n
    raise SystemExit("entry %d not found" % num)

# canaries
e339 = entry_node(339)
assert any(getattr(getattr(l2.from_node, "input_action", None), "freehk", None) is not None
           and l2.from_node.input_action.freehk.timelineParamRaw == "m38FF82EA"
           for l in e339.inputs["TIML Data"].links
           for l2 in l.from_node.inputs["TIML Animation"].links), "canary failed"
en = entry_node(312)
mot = en.inputs["LMT Animation"].links[0].from_node.input_action
assert mot.name == "LMT::w07::095_remap", "motion is %r" % mot.name
print("312 motion stays:", mot.name, flush=True)

dn = en.inputs["TIML Data"].links[0].from_node
for l in list(dn.inputs["TIML Animation"].links):
    tree.links.remove(l)
print("cleared .095 TIML links", flush=True)

y = 40.0
for name in OLD:
    act = bpy.data.actions.get(name)
    assert act, "missing old action %s" % name
    node = next((n for n in tree.nodes if n.bl_idname == "TIMLActionNode"
                 and n.input_action == act), None)
    if node is None:
        node = tree.nodes.new("TIMLActionNode")
        node.input_action = act
        node.location = (dn.location.x - 260.0, dn.location.y + y)
        y -= 160.0
    tree.links.new(node.outputs["TIML Animation"], dn.inputs["TIML Animation"])
    print("relinked", name, flush=True)

bpy.ops.wm.save_mainfile()
print("saved.", flush=True)
