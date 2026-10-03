"""取证: 用重置前的备份(MHW-Valor.blend.bak-20261003-124129)还原"乱掉"的真相。
把备份树整体导出到 scratch, 与原版 #2 逐 entry 容差比对:
  A. 名字配对: entry 上接的 action 名(LMT::w03_00::NNN)与 entryNum 是否一致
  B. 内容配对: 导出后的动作与原版 #2 同 id 动作是否一致(结构精确+数值容差)
只读备份, 不保存。"""
import bpy, importlib, json, re

BAK = r"E:\MHW-arts\MHW-Valor.blend.bak-20261003-124129"
LMT_VAN = r"E:\MHW-arts\chunk-extracted\chunk\hm\wp\wp03\mot\w03_00\w03_00.lmt"
SCRATCH = r"E:\MHW-arts\valor-remap-pipeline\mhgu\_backup_forensic_export.lmt"

Lmt = importlib.import_module("MHW-FreeKinetics.struct.Lmt")
export_ops = importlib.import_module("MHW-FreeKinetics.operators.export_ops")

van = Lmt.parseLMT(LMT_VAN)
van_e = {a.id: a for a in van.ActionHeaders}

main = bpy.data.node_groups["w03_00"]
out_node = next(n for n in main.nodes if n.bl_idname == "LMTFileNode")
entry_nodes = {n.entryNum: n for n in main.nodes if n.bl_idname == "LMTEntryNode"}

# ---- A. name pairing in backup ----
name_bad = []
deliberate = []
wired = 0
for i, node in sorted(entry_nodes.items()):
    inp = node.inputs["LMT Animation"]
    act = getattr(inp.links[0].from_node, "input_action", None) if inp.links else None
    if act is None:
        continue
    wired += 1
    m = re.match(r"^LMT::w03_00::(\d+)$", act.name)
    if m:
        if int(m.group(1)) != i:
            name_bad.append((i, act.name))
    else:
        deliberate.append((i, act.name))
print("A. wired=%d name-vs-entry mismatch=%d custom-name=%d" %
      (wired, len(name_bad), len(deliberate)))
print("   name mismatches:", name_bad[:30])
print("   custom/remap names:", deliberate[:30])

# ---- B. content compare via full export ----
bpy.context.scene.freehk_tether = bpy.data.objects.get("m_mesh Armature")
old_fp, old_inject = out_node.filepath, bool(out_node.inject)
out_node.filepath = SCRATCH
out_node.inject = False
addon = bpy.context.preferences.addons[export_ops.__package__.split(".")[0]]
export_ops.exportNodeToFile(out_node, addon.preferences)
out_node.filepath, out_node.inject = old_fp, old_inject
print("exported ->", SCRATCH)

exp = Lmt.parseLMT(SCRATCH)
exp_e = {a.id: a for a in exp.ActionHeaders}

TOL = 0.26

def basis_close(x, y):
    for k, (a, b) in enumerate(zip(x, y)):
        if abs(a - b) <= TOL:
            continue
        if k == len(x) - 1 and abs(a - b) <= 1.0 + 1e-6:
            return True if k == 3 else False
    return True

def chans_close(a, b):
    if len(a) != len(b):
        return False
    for ca, cb in zip(a, b):
        if len(ca) != len(cb):
            return False
        for (va, ta), (vb, tb) in zip(ca, cb):
            if abs(va - vb) > TOL or ta != tb:
                return False
    return True

def sig(la):
    return [(int(b.boneId), int(getattr(b, "bufferType", -1)), bool(getattr(b, "fold", False)),
             tuple(float(x) for x in tuple(b.basis)),
             tuple(tuple((float(v), float(t)) for v, t in ch) for ch in b.channels()))
            for b in la.bones]

def same(sa, sb):
    if len(sa) != len(sb):
        return False
    for x, y in zip(sa, sb):
        if x[:3] != y[:3]:
            return False
        bx = list(x[3]); by = list(y[3])
        if not basis_close(bx, by):
            return False
        if not chans_close(x[4], y[4]):
            return False
    return True

custom_pat = re.compile(r"remap|divide|plw_|192|095|151", re.I)
content_bad, content_bad_custom, missing = [], [], []
for i, v in van_e.items():
    if not v.fcurveOffset:
        continue
    node = entry_nodes.get(i)
    act = None
    if node and node.inputs["LMT Animation"].links:
        act = getattr(node.inputs["LMT Animation"].links[0].from_node, "input_action", None)
    is_custom_slot = act is not None and custom_pat.search(act.name or "")
    if i not in exp_e or not exp_e[i].fcurveOffset:
        (missing if not is_custom_slot else []).append(i)
        continue
    if same(sig(v), sig(exp_e[i])):
        continue
    if is_custom_slot:
        content_bad_custom.append(i)
    else:
        content_bad.append(i)
print("B. vanilla-slot content WRONG (non-deliberate): %d %s" % (len(content_bad), content_bad[:40]))
print("   deliberate(custom-named) differing slots: %d %s" % (len(content_bad_custom), content_bad_custom[:20]))
print("   vanilla slots missing from export: %s" % missing[:20])
with open(r"E:\MHW-arts\valor-remap-pipeline\mhgu\_backup_forensic.json", "w", encoding="utf-8") as f:
    json.dump({"name_mismatch": name_bad, "custom_named": deliberate,
               "content_bad": content_bad, "content_bad_custom": content_bad_custom,
               "missing": missing}, f, indent=1, default=str)
