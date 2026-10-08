# -*- coding: utf-8 -*-
# 提取+恢复 per-entry 节点属性: LMTEntryNode(loopFrame/byteflag/byteflag2) +
# TIMLDataNode(unkn1/unkn2/animLength/loopStartPoint/loopControl)。
# EXTRACT=1: 从当前打开的 blend 导出到 NODEPROPS json; 否则把 json 应用到当前 blend 并保存。
# 用法:
#   提取: NODEPROPS=out.json EXTRACT=1 blender -b 0.0.1.blend --python _restore_nodeprops.py
#   恢复: NODEPROPS=...json SAVE=1 blender -b MHW-Valor.blend --python _restore_nodeprops.py
import bpy, os, json, sys

NP = os.environ.get("NODEPROPS", r"E:\MHW-arts\_nodeprops.json")
EXTRACT = os.environ.get("EXTRACT", "0") == "1"
SAVE = os.environ.get("SAVE", "0") == "1"

tree = bpy.data.node_groups["w03_00"]

def entry_nodes():
    return {int(n.entryNum): n for n in tree.nodes if n.bl_idname == "LMTEntryNode"}

def data_node_of(e):
    sock = next(i for i in e.inputs if i.name == "TIML Data")
    return sock.links[0].from_node if sock.links else None

def collect():
    snap = {}
    for en, e in entry_nodes().items():
        rec = {
            "loopFrame": int(e.loopFrame),
            "byteflag": [bool(b) for b in e.byteflag],
            "byteflag2": [bool(b) for b in e.byteflag2],
        }
        d = data_node_of(e)
        if d is not None:
            rec["data"] = {
                "unkn1": int(d.unkn1), "unkn2": int(d.unkn2),
                "animLength": int(d.animLength),
                "loopStartPoint": int(d.loopStartPoint),
                "loopControl": d.loopControl,
            }
        snap[str(en)] = rec
    return snap

if EXTRACT:
    snap = collect()
    with open(NP, "w", encoding="utf-8") as f:
        json.dump(snap, f, indent=1)
    print("WROTE", NP, "entries:", len(snap))
else:
    with open(NP, encoding="utf-8") as f:
        snap = json.load(f)
    emap = entry_nodes()
    changed, missing = [], []
    for en_s, rec in snap.items():
        e = emap.get(int(en_s))
        if e is None:
            missing.append(en_s); continue
        before = (int(e.loopFrame), [bool(b) for b in e.byteflag], [bool(b) for b in e.byteflag2])
        e.loopFrame = rec["loopFrame"]
        e.byteflag = rec["byteflag"]
        e.byteflag2 = rec["byteflag2"]
        after = (int(e.loopFrame), [bool(b) for b in e.byteflag], [bool(b) for b in e.byteflag2])
        d = data_node_of(e)
        if "data" in rec and d is not None:
            rd = rec["data"]
            dbefore = (int(d.unkn1), int(d.unkn2), int(d.animLength), int(d.loopStartPoint), str(d.loopControl))
            d.unkn1 = rd["unkn1"]; d.unkn2 = rd["unkn2"]
            d.animLength = rd["animLength"]; d.loopStartPoint = rd["loopStartPoint"]
            d.loopControl = rd["loopControl"]
            dafter = (int(d.unkn1), int(d.unkn2), int(d.animLength), int(d.loopStartPoint), str(d.loopControl))
        else:
            dbefore = dafter = None
        if before != after or dbefore != dafter:
            changed.append((int(en_s), before, after, dbefore, dafter))
    print("changed entries: %d" % len(changed))
    for en, b, a, db, da in sorted(changed):
        print("  E%-4d entry %s -> %s | data %s -> %s" % (en, b, a, db, da))
    if missing:
        print("MISSING entries in current file:", missing)
    # 终检
    bad = []
    emap2 = entry_nodes()
    for en_s, rec in snap.items():
        e = emap2.get(int(en_s))
        if e is None: continue
        if (int(e.loopFrame), [bool(x) for x in e.byteflag], [bool(x) for x in e.byteflag2]) != \
           (rec["loopFrame"], rec["byteflag"], rec["byteflag2"]):
            bad.append(en_s); continue
        d = data_node_of(e)
        if "data" in rec:
            if d is None or (int(d.unkn1), int(d.unkn2), int(d.animLength), int(d.loopStartPoint), str(d.loopControl)) != \
               (rec["data"]["unkn1"], rec["data"]["unkn2"], rec["data"]["animLength"], rec["data"]["loopStartPoint"], rec["data"]["loopControl"]):
                bad.append(en_s + "!")
    if bad:
        print("VERIFY FAILED:", bad); sys.exit(4)
    print("VERIFY OK: node props match snapshot")
    if SAVE:
        bpy.ops.wm.save_mainfile()
        print("SAVED", bpy.data.filepath)
    else:
        print("DRY-RUN (no save)")
