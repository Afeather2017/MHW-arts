"""给 vanilla-reset 重导入的 164 个原版动作补绑骨架(= 导入时勾 "Tether to Skeleton")。
transferTether 会: BoneFunction.xxx → MhBone_XXX 重命名、Root(-1) 通道转物体级曲线
(导出时 restoreRootFromObject 还原)、记 action.freehk.tetherFrame。
绑完重新全树导出(_blend_export_f4.lmt)并全量验证(动作 vs 原版容差 + TIML vs F2 +
141-143 vs R)。"""
import bpy, importlib, hashlib

OUT = r"E:\MHW-arts\valor-remap-pipeline\mhgu\_blend_export_f4.lmt"
LMT_VAN = r"E:\MHW-arts\chunk-extracted\chunk\hm\wp\wp03\mot\w03_00\w03_00.lmt"
R_PATH = r"E:\SteamLibrary\steamapps\common\Monster Hunter World\nativePC\hm\wp\wp03\mot\w03_00\w03_00.lmt.bak-20261003-010916-valorcfg"
F2_PATH = r"E:\MHW-arts\valor-remap-pipeline\mhgu\_vanilla_firstaid_f2.lmt"

Lmt = importlib.import_module("MHW-FreeKinetics.struct.Lmt")
export_ops = importlib.import_module("MHW-FreeKinetics.operators.export_ops")
tetherOps = importlib.import_module("MHW-FreeKinetics.blender.tetherOps")

tether = bpy.data.objects["m_mesh Armature"]
main = bpy.data.node_groups["w03_00"]

# ---- 1. collect untethered vanilla-wired actions ----
van_ids = set()
seen_act = {}
targets = []
for n in main.nodes:
    if n.bl_idname != "LMTEntryNode":
        continue
    inp = n.inputs["LMT Animation"]
    if not inp.links:
        continue
    act = getattr(inp.links[0].from_node, "input_action", None)
    if act is None or act.name.endswith(".pre_reset"):
        continue
    if act.freehk.tetherFrame is None:
        if act.name not in seen_act:
            seen_act[act.name] = act
            targets.append(act)
    van_ids.add(n.entryNum)
print("[tether] untethered vanilla actions:", len(targets))
if targets:
    a0 = targets[0]
    print("[tether] before sample paths:", [fc.data_path for fc in a0.fcurves[:3]])
    tetherOps.transferTether(targets, tether)
    print("[tether] after  sample paths:", [fc.data_path for fc in a0.fcurves[:3]])
    bound = sum(1 for a in targets if a.freehk.tetherFrame is not None)
    print("[tether] tethered now:", bound, "/", len(targets))

# ---- 2. export ----
scene = bpy.context.scene
scene.freehk_tether = tether
out_node = next(n for n in main.nodes if n.bl_idname == "LMTFileNode")
old_fp, old_inject = out_node.filepath, bool(out_node.inject)
out_node.filepath = OUT
out_node.inject = False
addon = bpy.context.preferences.addons[export_ops.__package__.split(".")[0]]
export_ops.exportNodeToFile(out_node, addon.preferences)
out_node.filepath, out_node.inject = old_fp, old_inject
raw = open(OUT, "rb").read()
print("[export] ->", OUT, len(raw), "bytes, md5", hashlib.md5(raw).hexdigest())

# ---- 3. verify ----
TOL = 0.26

def chans_close(a, b):
    if len(a) != len(b): return False
    for ca, cb in zip(a, b):
        if len(ca) != len(cb): return False
        for (va, ta), (vb, tb) in zip(ca, cb):
            if abs(va - vb) > TOL or ta != tb: return False
    return True

def sig(la):
    return [(int(b.boneId), int(getattr(b, "bufferType", -1)), bool(getattr(b, "fold", False)),
             tuple(float(x) for x in tuple(b.basis)),
             tuple(tuple((float(v), float(t)) for v, t in ch) for ch in b.channels()))
            for b in la.bones]

def basis_close(x, y):
    for k, (a, b) in enumerate(zip(x, y)):
        if abs(a - b) <= TOL: continue
        if k == len(x) - 1 and abs(a - b) <= 1.0 + 1e-6: continue
        return False
    return True

def same(sa, sb):
    if len(sa) != len(sb): return False
    for x, y in zip(sa, sb):
        if x[:3] != y[:3]: return False
        if not basis_close(x[3], y[3]): return False
        if not chans_close(x[4], y[4]): return False
    return True

def tmd5(a):
    rawb = bytearray(a.timl.serialize()); rawb[0:4] = b"\0\0\0\0"
    return hashlib.md5(bytes(rawb)).hexdigest()

E4 = Lmt.parseLMT(OUT)
E4m = {a.id: a for a in E4.ActionHeaders}
Vm = {a.id: a for a in Lmt.parseLMT(LMT_VAN).ActionHeaders}
Rm = {a.id: a for a in Lmt.parseLMT(R_PATH).ActionHeaders}
F2m = {a.id: a for a in Lmt.parseLMT(F2_PATH).ActionHeaders}
print("[verify] headers:", len(E4.ActionHeaders), "entryCount:", E4.Header.entryCount)
ph = [i for i, a in E4m.items() if a.timlOffset
      and any(int(ty.timelineParameterHash) == 0x4BD36121 for ty in a.timl.types)]
print("[verify] placeholder remaining:", ph)
mot_bad = tim_bad = 0
for i, v in Vm.items():
    if not v.fcurveOffset: continue
    a = E4m.get(i)
    if a is None or not a.fcurveOffset or not same(sig(v), sig(a)):
        mot_bad += 1; print("   MOT BAD", i)
    f2 = F2m.get(i)
    if f2 is not None and a.timlOffset and f2.timlOffset and tmd5(a) != tmd5(f2):
        tim_bad += 1; print("   TIML BAD", i)
for cid in (141, 142, 143):
    a, r = E4m.get(cid), Rm.get(cid)
    if a is None or r is None or not same(sig(a), sig(r)) or tmd5(a) != tmd5(r):
        print("   CUSTOM BAD", cid); mot_bad += 1
print("[verify] motion_bad=%d timl_bad=%d" % (mot_bad, tim_bad))
print("RESULT:", "OK" if (not ph and mot_bad == 0 and tim_bad == 0) else "FAIL")

bpy.ops.wm.save_mainfile()
print("[save] blend saved.")
