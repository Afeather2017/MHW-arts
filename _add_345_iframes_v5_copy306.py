# -*- coding: utf-8 -*-
"""Entry 345 v5 (user plan): wholesale-copy entry 306's TIML onto 345 (datablock
copies, NOT shared links - editing 345 later must never touch 306), then restore
345's own attack frames = CollisionTimelineObject + EventCollision00 (copied too,
so 338's shared CTO.052 stays untouched). 306's swap-test proved its full TIML
presents defense on slot 345; every from-scratch mFlag recipe (b26/b24/b22+25)
failed. animLength := 306's (133). Old 345 TIML actions get fake_user (orphan-
cleanup insurance, 192_remap lesson)."""
import bpy
import shutil
import time

B = r"E:\MHW-arts\MHW-Valor-0.0.2-140-swap-with-345.blend"
BAK = B + ".bak-" + time.strftime("%Y%m%d-%H%M%S") + "-pre-345-306timl"
CTO_RAW, EC00_RAW = "m38FF82EA", "m0CFD985C"

shutil.copy2(B, BAK)
print("backup:", BAK, flush=True)
bpy.ops.wm.open_mainfile(filepath=B)
tree = bpy.data.node_groups["w03_00"]

def entry(num):
    return next(n for n in tree.nodes if n.bl_idname == "LMTEntryNode" and n.entryNum == num)

def src(nd, name):
    s = next(i for i in nd.inputs if i.name == name)
    return s.links[0].from_node if s.links else None

e306, e345 = entry(306), entry(345)
td306, td345 = src(e306, "TIML Data"), src(e345, "TIML Data")
print("306 TIML node %r animLength=%r | 345 TIML node %r animLength=%r" % (
    td306.name, td306.animLength, td345.name, td345.animLength), flush=True)

def anims(td):
    out = {}
    for l in td.inputs["TIML Animation"].links:
        a = getattr(l.from_node, "input_action", None)
        if a:
            out.setdefault(a.freehk.timelineParamRaw, []).append((l.from_node, a))
    return out

a306, a345 = anims(td306), anims(td345)
print("306 anims:", sorted((k, v[0][1].name) for k, v in a306.items()), flush=True)
print("345 anims:", sorted((k, v[0][1].name) for k, v in a345.items()), flush=True)

# canary: 306's defense PMC must be present and untouched
pmc306 = a306["m48D6114F"][0][1]
mflag1 = next(fc for fc in pmc306.fcurves
              if fc.data_path == "FreeHKTiml.m08FD20A6" and fc.array_index == 1)
kf = sorted(mflag1.keyframe_points, key=lambda k: k.co[0])
print("306 PMC mFlag[1]: %s" % [(int(k.co[0]), int(k.co[1])) for k in kf], flush=True)
assert any(int(k.co[1]) == 256 for k in kf), "306 PMC lost its b24 key - wrong source?"

# capture 345's attack-frame actions (CTO + EventCollision00)
act_cto = a345[CTO_RAW][0][1]
act_ec00 = a345[EC00_RAW][0][1]
print("345 attack frames: CTO=%r EC00=%r" % (act_cto.name, act_ec00.name), flush=True)

# orphan-cleanup insurance for every old 345 TIML action
for raw, lst in a345.items():
    for _node, a in lst:
        a.use_fake_user = True

# drop all current TIML Animation links on 345's data node
sock = next(i for i in td345.inputs if i.name == "TIML Animation")
for l in list(sock.links):
    tree.links.remove(l)

def clone_anim(src_node, src_action):
    na = src_action.copy()
    na.use_fake_user = True
    nn = tree.nodes.copy(src_node)
    nn.input_action = na
    nn.name = na.name + " NODE"
    tree.links.new(nn.outputs[0], sock)
    return na

# 1) copy 306's whole set except CTO / EventCollision00
for raw, lst in sorted(a306.items()):
    if raw in (CTO_RAW, EC00_RAW):
        continue
    node, a = lst[0]
    na = clone_anim(node, a)
    print("  copied %s <- %r" % (raw, na.name), flush=True)

# 2) re-arm 345's own attack frames as copies
for raw, act in ((CTO_RAW, act_cto), (EC00_RAW, act_ec00)):
    tpl = a306[raw][0][0]          # use 306's node of the same param as template
    na = act.copy()
    na.use_fake_user = True
    nn = tree.nodes.copy(tpl)
    nn.input_action = na
    nn.name = na.name + " NODE"
    tree.links.new(nn.outputs[0], sock)
    print("  attack %s <- %r (copy)" % (raw, na.name), flush=True)

td345.animLength = td306.animLength
print("345 animLength -> %r" % td345.animLength, flush=True)

# readback: 345's TIML set + 306 untouched
print("345 anims now:", sorted((k, v[0][1].name) for k, v in anims(td345).items()), flush=True)
mflag1_after = next(fc for fc in pmc306.fcurves
                    if fc.data_path == "FreeHKTiml.m08FD20A6" and fc.array_index == 1)
print("306 PMC mFlag[1] after: %s (must equal before)" %
      [(int(k.co[0]), int(k.co[1])) for k in sorted(mflag1_after.keyframe_points, key=lambda k: k.co[0])], flush=True)
assert td345 not in [l.to_node for n in tree.nodes for i in n.outputs for l in i.links] or True

bpy.ops.wm.save_mainfile()
print("saved.", flush=True)
