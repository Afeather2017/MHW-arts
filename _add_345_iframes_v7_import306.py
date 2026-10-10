# -*- coding: utf-8 -*-
"""Entry 345 v7: import VANILLA entry 306's embedded TIML straight out of
w03_00-origin.lmt (Lmt.py-style: pre-seeked stream + static marshallTIML,
absolute offsets base=0), build fresh actions/nodes via the addon importer
path (processTIML+TIMLStructure), wire them onto 345 as fresh datablocks,
re-arm 345's attack frames (CTO.052 + EC00.198 copies), adopt vanilla block
header fields (animLength clamped to motion 142f). Old 345 TIML -> fake_user."""
import bpy
import sys
import shutil
import time
import importlib

B = r"E:\MHW-arts\MHW-Valor-0.0.2-140-swap-with-345.blend"
ORIGIN = r"E:\SteamLibrary\steamapps\common\Monster Hunter World\nativePC\hm\wp\wp03\mot\w03_00\w03_00-origin.lmt"
BAK = B + ".bak-" + time.strftime("%Y%m%d-%H%M%S") + "-pre-345-vanilla306import"
CTO_RAW, EC00_RAW = "m38FF82EA", "m0CFD985C"
MOTION_FRAMES = 142

class RawStream(object):
    def __init__(self, data):
        self.data = data
        self.cursor = 0
    def read(self, length=1):
        d = self.data[self.cursor:self.cursor+length]
        self.cursor += length
        return d
    def seek(self, pos):
        self.cursor = pos
    def skip(self, length):
        self.read(length)
    def tell(self):
        return self.cursor

shutil.copy2(B, BAK)
print("backup:", BAK, flush=True)
bpy.ops.wm.open_mainfile(filepath=B)

sys.path.insert(0, r"E:\MHW-arts")
import dump_timl_frames as D
TIMLmod = importlib.import_module("MHW-FreeKinetics.struct.TIML")
ti = importlib.import_module("MHW-FreeKinetics.blender.timl_importer")
nops = importlib.import_module("MHW-FreeKinetics.blender.nodetree.freeHKNodeOps")

buf = open(ORIGIN, "rb").read()
n, acts = D.parse_lmt(buf)
off = acts[306]["timlOffset"]
stream = RawStream(buf)
stream.seek(off)
data306 = TIMLmod.TIML.marshallTIML(off, stream)
assert data306 is not None, "marshallTIML returned None"
print("vanilla 306: animLen=%r dataIx=(%r,%r) loop=(%r,%r) types=%d" % (
    data306.animationLength, data306.dataIx0, data306.dataIx1,
    data306.loopStartPoint, data306.loopControl, len(data306.types)), flush=True)

for ty in data306.types:
    print("  type m%08X count=%d off=0x%x" % (ty.timelineParameterHash, ty.count, ty.offset), flush=True)
    for tr in ty.transforms:
        print("    xf m%08X count=%d off=0x%x dataType=%r keys=%d" % (
            tr.datatypeHash, tr.count, tr.offset, tr.dataType, len(getattr(tr,'keyframes',[]))), flush=True)
wrap = TIMLmod.TIML()
wrap.data = [data306]
raw = ti.processTIML(wrap, ti.remapAnalyzer)
for ty in data306.types:
    print("  POST m%08X xfs=%d" % (ty.timelineParameterHash, len(ty.transforms)), flush=True)

tree = bpy.data.node_groups["w03_00"]
mgr = nops.uiNodeManager(tree)
dataNodes = ti.TIMLStructure(raw, mgr, entry=[])
van_dn = dataNodes[0]
new_nodes = [l.from_node for l in van_dn.inputs["TIML Animation"].links]
print("imported %d TIML action nodes" % len(new_nodes), flush=True)

def entry(num):
    return next(nn for nn in tree.nodes if nn.bl_idname == "LMTEntryNode" and nn.entryNum == num)

def src(nd, name):
    s = next(i for i in nd.inputs if i.name == name)
    return s.links[0].from_node if s.links else None

e345 = entry(345)
td345 = src(e345, "TIML Data")
sock = next(i for i in td345.inputs if i.name == "TIML Animation")

old = {}
for l in list(sock.links):
    a = l.from_node.input_action
    old.setdefault(a.freehk.timelineParamRaw, a)
for a in old.values():
    a.use_fake_user = True
act_cto, act_ec00 = old[CTO_RAW], old[EC00_RAW]
print("345 old anims: %d raws; attack CTO=%r EC00=%r" % (len(old), act_cto.name, act_ec00.name), flush=True)

for l in list(sock.links):
    tree.links.remove(l)

# vanilla 306 carries TWO PlMotionCommon types (main defense PMC + flag4/aux PMC)
def high_keys(action):
    for fc in action.fcurves:
        if fc.data_path == "FreeHKTiml.m08FD20A6" and fc.array_index == 1:
            return [(int(k.co[0]), int(k.co[1])) for k in sorted(fc.keyframe_points, key=lambda k: k.co[0])]
    return None

defense_pmcs = [nd for nd in new_nodes
                if nd.input_action.freehk.timelineParamRaw == "m48D6114F"
                and high_keys(nd.input_action) == [(0, 576), (10, 832), (15, 832)]]
assert len(defense_pmcs) == 1, "defense PMC not found among imported types"
print("defense PMC action: %r" % defense_pmcs[0].input_action.name, flush=True)

for nd in new_nodes:
    r = nd.input_action.freehk.timelineParamRaw
    if r == EC00_RAW:
        print("  skip vanilla EC00 (345's own attack EC00 wins)", flush=True)
        continue
    tree.links.new(nd.outputs["TIML Animation"], sock)
    print("  wired %s <- %r" % (r, nd.input_action.name), flush=True)

for r, act in ((CTO_RAW, act_cto), (EC00_RAW, act_ec00)):
    na = act.copy()
    na.use_fake_user = True
    mgr.createTIMLActionNode(na, outputs=[td345])
    print("  attack %s <- %r (copy)" % (r, na.name), flush=True)

td345.unkn1 = data306.dataIx0
td345.unkn2 = data306.dataIx1
td345.loopStartPoint = int(data306.loopStartPoint)
td345.loopControl = str(int(data306.loopControl))
td345.animLength = min(int(data306.animationLength), MOTION_FRAMES)
print("345 block header -> animLength=%d dataIx=(%r,%r) loop=(%r,%s)" % (
    td345.animLength, td345.unkn1, td345.unkn2, td345.loopStartPoint, td345.loopControl), flush=True)

tree.nodes.remove(van_dn)

final = {}
for l in td345.inputs["TIML Animation"].links:
    a = l.from_node.input_action
    final.setdefault(a.freehk.timelineParamRaw, []).append(a.name)
print("345 TIML now: %r" % sorted(final.items()), flush=True)

bpy.ops.wm.save_mainfile()
print("saved.", flush=True)
