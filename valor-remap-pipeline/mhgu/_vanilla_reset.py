"""Vanilla-reset: 主树动作全部换成原版 LMT(#2) 的动作(按 entry id 匹配),
TIML 全部换成改版 LMT(#1) 的 TIML。自定义槽位(#2 里不存在的 id, 如 141/142/143)动画保持不动。

流程:
  0. 备份 blend (外部做)
  1. 现有全部 action 打 fake_user (孤儿清理保命)
  2. 与新建动作同名的旧动作改 .pre_reset 后缀
  3. 按 #2 的 fcurveOffset 分组导入原版动作(KBRAction 同款), 按 entryNum 重接动画
  4. 按 #1 的 timlOffset 分组 processTIMLData+TIMLStructure 重建 TIML 节点
  5. 删掉失去连线的旧节点(动作本体保留)
  6. 全树导出到 scratch LMT, 逐 entry 验证: TIML md5 == #1, 动作 digest == #2
  7. 保存 blend

用法:
  blender -b MHW-Valor.blend --python mhgu/_vanilla_reset.py
"""
import bpy, importlib, hashlib, json, os, traceback
from pathlib import Path

LMT_MOD = r"E:\SteamLibrary\steamapps\common\Monster Hunter World\nativePC\hm\wp\wp03\mot\w03_00\w03_00.lmt"
LMT_VAN = r"E:\MHW-arts\chunk-extracted\chunk\hm\wp\wp03\mot\w03_00\w03_00.lmt"
SCRATCH = r"E:\MHW-arts\valor-remap-pipeline\mhgu\_vanilla_reset_export.lmt"
REPORT = r"E:\MHW-arts\valor-remap-pipeline\mhgu\_vanilla_reset_report.json"

rep = {"replaced": [], "kept_custom": [], "timl_groups": 0, "removed_nodes": 0,
       "verify": {}, "errors": []}

def log(*a):
    print("[vreset]", *a)

Lmt = importlib.import_module("MHW-FreeKinetics.struct.Lmt")
lmt_importer = importlib.import_module("MHW-FreeKinetics.blender.lmt_importer")
timl_importer = importlib.import_module("MHW-FreeKinetics.blender.timl_importer")
timl_io = importlib.import_module("MHW-FreeKinetics.operators.timl_io")
nops = importlib.import_module("MHW-FreeKinetics.blender.nodetree.freeHKNodeOps")
export_ops = importlib.import_module("MHW-FreeKinetics.operators.export_ops")

mod = Lmt.parseLMT(LMT_MOD)
van = Lmt.parseLMT(LMT_VAN)
mod_e = {a.id: a for a in mod.ActionHeaders}
van_e = {a.id: a for a in van.ActionHeaders}

def motion_digest(lmt_action):
    parts = []
    for b in lmt_action.bones:
        d = [int(b.boneId), int(getattr(b, "bufferType", -1)), bool(getattr(b, "fold", False)),
             tuple(round(float(x), 4) for x in tuple(b.basis))]
        if hasattr(b, "tail"):
            d.append(tuple(round(float(x), 4) for x in tuple(b.tail)))
        chans = tuple(tuple((round(float(v), 4), round(float(t), 4)) for v, t in ch)
                      for ch in b.channels())
        d.append(chans)
        parts.append(repr(tuple(d)))
    return hashlib.md5("\n".join(sorted(parts)).encode()).hexdigest()

van_digest = {i: motion_digest(a) for i, a in van_e.items() if a.fcurveOffset}
mod_timl_md5 = {}
for i, a in mod_e.items():
    if a.timlOffset:
        mod_timl_md5[i] = hashlib.md5(a.timl.serialize()).hexdigest()

main = bpy.data.node_groups["w03_00"]
out_node = next(n for n in main.nodes if n.bl_idname == "LMTFileNode")
entry_nodes = {n.entryNum: n for n in main.nodes if n.bl_idname == "LMTEntryNode"}
log("main tree entries:", len(entry_nodes))

# ---- 1. fake_user everything existing ----
for a in bpy.data.actions:
    a.use_fake_user = True
log("fake_user set on", len(bpy.data.actions), "actions")

# ---- 2. rename canonical-name collisions ----
renamed = 0
for a in list(bpy.data.actions):
    if a.name.startswith("LMT::w03_00::"):
        a.name = a.name + ".pre_reset"
        renamed += 1
log("renamed old canonical actions:", renamed)

# ---- 3. vanilla motion groups ----
motion_groups = {}   # fcurveOffset -> (rep_action_header, [ids])
for a in sorted(van.ActionHeaders, key=lambda x: x.id):
    if not a.fcurveOffset:
        continue
    g = motion_groups.setdefault(a.fcurveOffset, [a, []])
    g[1].append(a.id)
log("vanilla motion groups:", len(motion_groups))

# ---- 4. unlink target sockets ----
van_ids = set(van_digest)
for i, node in entry_nodes.items():
    if i in van_ids:
        inp = node.inputs["LMT Animation"]
        for l in list(inp.links):
            old = l.from_node
            act = getattr(old, "input_action", None)
            rep["replaced"].append({"entry": i, "old_action": act.name if act else None,
                                    "old_frames": act.freehk.frameCount if act else None,
                                    "new_frames": van_e[i].frameCount})
            main.links.remove(l)
    if i in mod_timl_md5:
        tinp = node.inputs["TIML Data"]
        for l in list(tinp.links):
            main.links.remove(l)

# custom slots (no vanilla counterpart) keep animation
for i in sorted(set(entry_nodes) - van_ids):
    node = entry_nodes[i]
    inp = node.inputs["LMT Animation"]
    act = None
    if inp.links:
        act = getattr(inp.links[0].from_node, "input_action", None)
    rep["kept_custom"].append({"entry": i, "action": act.name if act else None})
log("replaced:", len(rep["replaced"]), "kept custom:", len(rep["kept_custom"]))

# ---- 5. create + link vanilla action nodes ----
mgr = nops.uiNodeManager(main, hide=True)
scene = bpy.context.scene
for off, (rep_hdr, ids) in sorted(motion_groups.items()):
    act = lmt_importer.KBRAction(Path(LMT_VAN).stem, rep_hdr)
    targets = [entry_nodes[i] for i in ids if i in entry_nodes]
    mgr.createLMTActionNode(act, outputs=targets)
    scene.frame_end = max(scene.frame_end, int(act.freehk.frameCount))
log("vanilla action nodes created:", len(motion_groups))

# ---- 6. rebuild TIML from mod LMT ----
timl_groups = {}   # timlOffset -> (rep_header, [ids])
for a in sorted(mod.ActionHeaders, key=lambda x: x.id):
    if not a.timlOffset:
        continue
    g = timl_groups.setdefault(a.timlOffset, [a, []])
    g[1].append(a.id)
mapper, rmapper = timl_io.parseRemap(
    type("S", (), {p: d for p, d in zip(
        ["transx", "transy", "transz", "rotx", "roty", "rotz", "sclx", "scly", "sclz"],
        "m8E8AFE06 mF98DCE90 m60849F2A mF105BBE3 m86028B75 m1F0BDACF m9486DF23 mE381EFB5 m7A88BE0F".split())})())
remapper = lambda typing: (mapper, rmapper)
linked_timl = 0
for off, (rep_hdr, ids) in sorted(timl_groups.items()):
    targets = [entry_nodes[i] for i in ids if i in entry_nodes]
    if not targets:
        continue
    data = timl_importer.processTIMLData(rep_hdr.timl, remapper)
    timl_importer.TIMLStructure([data], mgr, targets)
    linked_timl += len(targets)
rep["timl_groups"] = len(timl_groups)
log("timl groups rebuilt:", len(timl_groups), "entries linked:", linked_timl)

# ---- 7. remove orphaned old nodes ----
def has_outputs(n):
    return any(s.links for s in n.outputs)

for _ in range(3):
    for n in list(main.nodes):
        if n.bl_idname in ("LMTActionNode", "TIMLActionNode", "TIMLDataNode") and not has_outputs(n):
            main.nodes.remove(n)
            rep["removed_nodes"] += 1
log("removed orphaned nodes:", rep["removed_nodes"])

# ---- 8. export full tree to scratch & verify ----
try:
    scene.freehk_tether = bpy.data.objects["m_mesh Armature"]
    old_fp, old_inject = out_node.filepath, bool(out_node.inject)
    out_node.filepath = SCRATCH
    out_node.inject = False
    addon = bpy.context.preferences.addons[export_ops.__package__.split(".")[0]]
    export_ops.exportNodeToFile(out_node, addon.preferences)
    out_node.filepath, out_node.inject = old_fp, old_inject
    log("export ok ->", SCRATCH)

    exp = Lmt.parseLMT(SCRATCH)
    exp_e = {a.id: a for a in exp.ActionHeaders}
    v_timl_ok, v_timl_bad, v_mot_ok, v_mot_bad = 0, [], 0, []
    for i, md5w in mod_timl_md5.items():
        if i not in exp_e or not exp_e[i].timlOffset:
            v_timl_bad.append((i, "missing"))
            continue
        if hashlib.md5(exp_e[i].timl.serialize()).hexdigest() == md5w:
            v_timl_ok += 1
        else:
            v_timl_bad.append((i, "md5 differs"))
    for i, dg in van_digest.items():
        if i not in exp_e or not exp_e[i].fcurveOffset:
            v_mot_bad.append((i, "missing"))
            continue
        if motion_digest(exp_e[i]) == dg:
            v_mot_ok += 1
        else:
            v_mot_bad.append((i, "digest differs"))
    rep["verify"] = {"timl_ok": v_timl_ok, "timl_bad": v_timl_bad,
                     "motion_ok": v_mot_ok, "motion_bad": v_mot_bad,
                     "export_entries": len(exp.ActionHeaders)}
    log("VERIFY timl ok/bad: %d/%d  motion ok/bad: %d/%d" %
        (v_timl_ok, len(v_timl_bad), v_mot_ok, len(v_mot_bad)))
    for b in v_timl_bad[:20]:
        log("  TIML BAD", b)
    for b in v_mot_bad[:20]:
        log("  MOTION BAD", b)
except Exception:
    rep["errors"].append(traceback.format_exc()[-2000:])
    traceback.print_exc()

with open(REPORT, "w", encoding="utf-8") as f:
    json.dump(rep, f, ensure_ascii=False, indent=1, default=str)

bpy.ops.wm.save_mainfile()
log("blend saved.")
