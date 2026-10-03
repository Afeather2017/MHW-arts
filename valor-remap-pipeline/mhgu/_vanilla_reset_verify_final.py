"""最终验收 v2:
A. 重新打开保存后的 blend, 验证树接线: 164 原版槽 -> 新原版动作(starType/frames),
   141/142/143 自定义保留, 164 个 TIML 全部有连线, entry 头部 vs #2 差异。
B. 内容: scratch 导出 LMT vs #2, 逐 entry 结构+量化值对比(网格 0.25; 时间必须精确)。"""
import bpy, importlib, json

LMT_VAN = r"E:\MHW-arts\chunk-extracted\chunk\hm\wp\wp03\mot\w03_00\w03_00.lmt"
LMT_MOD = r"E:\SteamLibrary\steamapps\common\Monster Hunter World\nativePC\hm\wp\wp03\mot\w03_00\w03_00.lmt"
SCRATCH = r"E:\MHW-arts\valor-remap-pipeline\mhgu\_vanilla_reset_export.lmt"

Lmt = importlib.import_module("MHW-FreeKinetics.struct.Lmt")
van = Lmt.parseLMT(LMT_VAN)
mod = Lmt.parseLMT(LMT_MOD)
exp = Lmt.parseLMT(SCRATCH)
van_e = {a.id: a for a in van.ActionHeaders}
mod_e = {a.id: a for a in mod.ActionHeaders}
exp_e = {a.id: a for a in exp.ActionHeaders}

def q(v, g=0.25):
    return round(v / g)

TOL = 0.26

def basis_close(x, y):
    for k, (a, b) in enumerate(zip(x, y)):
        if abs(a - b) <= TOL:
            continue
        if k == len(x) - 1 and abs(a - b) <= 1.0 + 1e-6:
            continue  # loc basis w: 1.0 -> 0.0 toolchain signature
        return False
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

def sig(lmt_action):
    out = []
    for b in lmt_action.bones:
        chans = tuple(tuple((float(v), float(t)) for v, t in ch) for ch in b.channels())
        out.append((int(b.boneId), int(getattr(b, "bufferType", -1)), bool(getattr(b, "fold", False)),
                    tuple(float(x) for x in tuple(b.basis)), chans))
    return out

# ---- B. content ----
c_ok = c_bad = 0
c_badlist = []
for i, v in van_e.items():
    if not v.fcurveOffset:
        continue
    if i not in exp_e or not exp_e[i].fcurveOffset:
        c_badlist.append((i, "missing in export")); c_bad += 1; continue
    sa, sb = sig(v), sig(exp_e[i])
    same = len(sa) == len(sb) and all(
        x[:3] == y[:3] and basis_close(x[3], y[3]) and chans_close(x[4], y[4])
        for x, y in zip(sa, sb))
    if same:
        c_ok += 1
    else:
        a, b = sig(v), sig(exp_e[i])
        d = "nbones %d/%d" % (len(a), len(b))
        for k, (x, y) in enumerate(zip(a, b)):
            if x != y:
                d = "bone[%d] id %s/%s" % (k, x[0], y[0]) if x[0] != y[0] else \
                    "bone[%d](%s) meta %s/%s" % (k, x[0], x[1:3], y[1:3]) if x[1:3] != y[1:3] else \
                    "bone[%d](%s) basis %s/%s" % (k, x[0], x[3], y[3]) if x[3] != y[3] else \
                    "bone[%d](%s) chans differ" % (k, x[0])
                break
        c_badlist.append((i, d)); c_bad += 1
print("CONTENT ok=%d bad=%d %s" % (c_ok, c_bad, c_badlist[:10]))

# ---- A. blend wiring (current file already open via -b blend arg) ----
main = bpy.data.node_groups["w03_00"]
entry_nodes = {n.entryNum: n for n in main.nodes if n.bl_idname == "LMTEntryNode"}
wired = missing = 0
bad_frames = []
bad_type = []
name_bad = []
timl_missing = []
custom = []
header_diff = []
for i, node in sorted(entry_nodes.items()):
    v = van_e.get(i)
    inp = node.inputs["LMT Animation"]
    act = getattr(inp.links[0].from_node, "input_action", None) if inp.links else None
    tinp = node.inputs["TIML Data"]
    if v is None or not v.fcurveOffset:
        custom.append((i, act.name if act else None))
        continue
    if act is None:
        missing += 1
        continue
    wired += 1
    if act.freehk.frameCount != v.frameCount:
        bad_frames.append((i, act.freehk.frameCount, v.frameCount))
    if act.freehk.starType != "LMT_Action":
        bad_type.append((i, act.freehk.starType))
    if ".pre_reset" in act.name:
        name_bad.append((i, act.name))
    if i in mod_e and mod_e[i].timlOffset and not tinp.links:
        timl_missing.append(i)
    flags = sum((bool(b) << (7 - k)) for k, b in enumerate(node.byteflag))
    flags2 = sum((bool(b) << (7 - k)) for k, b in enumerate(node.byteflag2))
    if node.loopFrame != v.loopFrame or flags != v.Flags or flags2 != v.Flags2:
        header_diff.append({"id": i, "tree": [node.loopFrame, flags, flags2],
                            "van": [v.loopFrame, v.Flags, v.Flags2]})
print("WIRING wired=%d missing=%d custom=%s" % (wired, missing, custom))
print("  frame_mismatch=%s type_bad=%s pre_reset_wired=%s timl_missing=%s" %
      (bad_frames[:8], bad_type[:8], name_bad[:8], timl_missing[:8]))
print("  header_diff count=%d %s" % (len(header_diff), header_diff[:8]))
out = {"content_ok": c_ok, "content_bad": c_badlist, "wired": wired, "missing": missing,
       "custom": custom, "frame_mismatch": bad_frames, "timl_missing": timl_missing,
       "header_diff": header_diff}
with open(r"E:\MHW-arts\valor-remap-pipeline\mhgu\_vanilla_reset_verify_final.json", "w", encoding="utf-8") as f:
    json.dump(out, f, ensure_ascii=False, indent=1, default=str)
