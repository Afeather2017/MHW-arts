# -*- coding: utf-8 -*-
"""Entry 312 <- LMT::w07::095_remap + its .095 TIML family + 60f i-frames + f100 input window."""
import bpy
import shutil
import time

MASTER = r"E:\MHW-arts\MHW-Valor.blend"
BAK = MASTER + ".bak-" + time.strftime("%Y%m%d-%H%M%S") + "-pre-312-095wire"
ENTRY = 312
MFLAG, MTRIG = "FreeHKTiml.m08FD20A6", "FreeHKTiml.m99897C43"
IFRAME_HIGH = 1024.0           # b26 = 0x04000000 >> 16
BASE_LOW, BASE_HIGH = 6.0, 576.0   # 0x02400006 baseline
OFF = 60.0                     # i-frames [0,59]
OPEN = 100.0                   # input window opens here

# f70 stage = the native "open" acceptance set of this move
INPUT_KEYS = {
    (MFLAG, 0): [(0.0, 0.0), (OPEN, 49920.0)],
    (MFLAG, 1): [(0.0, 0.0), (OPEN, 0.0)],
    ("FreeHKTiml.m6E63FBC7", 0): [(0.0, 0.0), (OPEN, 12288.0)],
    ("FreeHKTiml.m6E63FBC7", 1): [(0.0, 0.0), (OPEN, 3072.0)],
    ("FreeHKTiml.mF76AAA7D", 0): [(0.0, 0.0), (OPEN, 1008.0)],
    ("FreeHKTiml.mF76AAA7D", 1): [(0.0, 0.0), (OPEN, 192.0)],
}

shutil.copy2(MASTER, BAK)
print("backup:", BAK, flush=True)
bpy.ops.wm.open_mainfile(filepath=MASTER)
tree = bpy.data.node_groups["w03_00"]

def entry_node(num):
    for n in tree.nodes:
        if n.bl_idname == "LMTEntryNode" and n.entryNum == num:
            return n
    raise SystemExit("entry %d not found" % num)

e339 = entry_node(339)
assert any(getattr(getattr(l2.from_node, "input_action", None), "freehk", None) is not None
           and l2.from_node.input_action.freehk.timelineParamRaw == "m38FF82EA"
           for l in e339.inputs["TIML Data"].links
           for l2 in l.from_node.inputs["TIML Animation"].links), "canary failed"
en = entry_node(ENTRY)

# ---- motion ----
act095 = bpy.data.actions["LMT::w07::095_remap"]
assert act095.freehk.tetherFrame, "no tether on 095_remap"
assert (act095.freehk.frameCount or 0) > OPEN, "frameCount %s" % act095.freehk.frameCount
node095 = next((n for n in tree.nodes if n.bl_idname == "LMTActionNode"
                and n.input_action == act095), None)
if node095 is None:
    node095 = tree.nodes.new("LMTActionNode")
    node095.input_action = act095
    node095.location = (en.location.x - 260.0, en.location.y + 120.0)
    print("created LMTActionNode for 095_remap", flush=True)
for l in list(en.inputs["LMT Animation"].links):
    print("removed old motion:", l.from_node.input_action.name, flush=True)
    tree.links.remove(l)
tree.links.new(node095.outputs["LMT Animation"], en.inputs["LMT Animation"])

# ---- TIML: rewire data node to the .095 family ----
dn = en.inputs["TIML Data"].links[0].from_node
for l in list(dn.inputs["TIML Animation"].links):
    tree.links.remove(l)
print("cleared old TIML links on", dn.name, flush=True)

fam = sorted(a.name for a in bpy.data.actions
             if a.name.startswith("TIML::") and a.name.endswith(".095"))
print("wiring %d .095 TIML actions:" % len(fam), flush=True)
y = 60.0
for name in fam:
    a = bpy.data.actions[name]
    if a.freehk.starType != "TIML_Action":
        a.freehk.starType = "TIML_Action"
        print("  %s: starType -> TIML_Action" % name, flush=True)
    if not a.freehk.timelineParamRaw:
            cat = name.split("::")[-1].rsplit(".", 1)[0]
            donor = None
            for other in bpy.data.actions:
                if (other.name.startswith("TIML::") and not other.name.endswith(".095")
                        and other.freehk.timelineParamRaw
                        and other.name.split("::")[-1].rsplit(".", 1)[0] == cat):
                    donor = other.freehk.timelineParamRaw
                    break
            assert donor, "%s: no donor param for category %s" % (name, cat)
            a.freehk.timelineParamRaw = donor
            try:
                a.freehk.timelineParam = donor
            except TypeError:
                pass
            print("  %s: timelineParamRaw -> %s (from %s family)" % (name, donor, cat), flush=True)
    assert a.freehk.timelineParamRaw, "%s has no param" % name
    node = next((n for n in tree.nodes if n.bl_idname == "TIMLActionNode"
                 and n.input_action == a), None)
    if node is None:
        node = tree.nodes.new("TIMLActionNode")
        node.input_action = a
        node.location = (dn.location.x - 260.0, dn.location.y + y)
        y -= 160.0
    tree.links.new(node.outputs["TIML Animation"], dn.inputs["TIML Animation"])
print("data node animLength %d -> 121" % dn.animLength, flush=True)
dn.animLength = 121

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

# ---- i-frame window on PlMotionCommon.095 ----
pmc = bpy.data.actions["TIML::nTimelineParam::PlMotionCommon.095"]
rebuild(pmc, MFLAG, 0, [(0.0, BASE_LOW), (OFF, BASE_LOW)])
rebuild(pmc, MFLAG, 1, [(0.0, BASE_HIGH + IFRAME_HIGH), (OFF, BASE_HIGH)])
# mTrigger 2048@f0 already present -> verify
trig = [fc for fc in pmc.fcurves if fc.data_path == MTRIG]
assert trig and any(abs(kf.co[1] - 2048) < 0.01 for fc in trig for kf in fc.keyframe_points), \
    "mTrigger 2048 missing"
print("PMC.095: b26 window [0,59] set, mTrigger 2048@f0 verified", flush=True)

# ---- input window at f100 on PlMotionInput.095 ----
pin = bpy.data.actions["TIML::nTimelineParam::PlMotionInput.095"]
for (path, ix), keys in INPUT_KEYS.items():
    rebuild(pin, path, ix, keys)
print("PIN.095: open moved to f%.0f" % OPEN, flush=True)

bpy.ops.wm.save_mainfile()
print("saved.", flush=True)
