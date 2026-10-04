# -*- coding: utf-8 -*-
"""勇气纳刀动作构建 v3 (valor sheath) — 拼接点由 2D 姿态扫描实测:
    最佳对 (tA=46, tB=131): 手 IK 目标差 2.6/3.4cm, 头 2.3cm, 重心 3.8cm,
    体骨平均 ~8°; tA 诊断线 46 处 V 形谷 (_valor_sheath_scan2d.py)。

  valor_sheath_start = 195[0..38] + 渐变[38..46] + 整个196(从匹配点131顺播:
      131..169 -> 绕回 1..130; 跳过 B@0 = 源内部 42.9° 复位帧), 循环 169 帧,
      loopFrame=46, 末帧 213; B@168->B@1 绕回缝用 8 帧渐变抹平
  valor_sheath_done  = 195[46..143] 前移到 0..97 (A 自身轨迹, 供松手衔接;
      首键 = A@46, 与 start 持有基线一致)
  TIML::nTimelineParam::PlMotionCommon = mFlag 0x04000000(b26)@f0 常开
      + mTrigger 2048@f0 —— 用户已验证防御帧配方, 无关闭键=穿透循环段

接缝处理 (MHGU 原作靠引擎混合, 单动作硬拼必须自己烤):
  - 头部 8 帧渐变 A(f) -> B@TB, 四元数逐帧归一化
  - 按骨符号对齐: dot(A@46, B@131)<0 的骨, B 段四通道整体取反
  - 位移通道 B 段加 A@46-B@131 偏移 -> 根位移连续, 且与 done 首键一致
  - 尾部 8 帧回渐变 B(f) -> B@131 -> 循环回绕无缝
  loopFrame = 46 (导出取 ENTRY 节点属性, freehk.loopFrame 仅记录)

Run: blender -b MHW-Valor.blend --python _valor_sheath_build.py
"""
import math
import shutil
import sys
import time

import bpy

BLEND = r"E:\MHW-arts\MHW-Valor.blend"
SRC_A = "LMT::w07::195_remap"
SRC_B = "LMT::w07::196_remap"
JOIN = 46                   # A 侧拼接帧 (2D 扫描实测)
TB = 131                    # B 侧入点帧
TB2 = 1                     # B 真实动画起点(跳过 B@0: 源在 B0->B1 有 42.9° 内部阶跃,
                            #   全时间线唯一一根内部缝, 004/006/007/012 族; B@169->B@0 闭合)
BW = 8                      # 渐变帧数
NAME_START = "valor_sheath_start"
NAME_DONE = "valor_sheath_done"
TIML_NAME = "TIML::nTimelineParam::PlMotionCommon"
FLAG_HASH = "m08FD20A6"     # PlMotionCommon::mFlag
TRIG_HASH = "m99897C43"     # PlMotionCommon::mTrigger
PARAM_RAW = "m48D6114F"     # nTimelineParam::PlMotionCommon
FLAG_VALUE = 0x04000000     # b26 无敌窗, 无关闭键 -> 常开穿透循环
TRIG_VALUE = 2048
EPS = 1e-4
FC_A, FC_B = 143, 169

bak = BLEND + ".bak-" + time.strftime("%Y%m%d-%H%M%S")
shutil.copyfile(BLEND, bak)
print("[backup] %s" % bak, flush=True)

bpy.ops.wm.open_mainfile(filepath=BLEND)

srcA = bpy.data.actions.get(SRC_A)
srcB = bpy.data.actions.get(SRC_B)
arm = bpy.data.objects.get("m_mesh Armature")
assert srcA and srcB and arm, "missing source actions or armature"
for s, want in ((srcA, FC_A), (srcB, FC_B)):
    last = max(k.co[0] for fc in s.fcurves for k in fc.keyframe_points)
    assert int(round(last)) == want == int(s.freehk.frameCount), (s.name, last)
END = JOIN + FC_B - TB2     # start 总长 = 46 + 169 = 214 (末键帧)
print("[src] A last=%d B last=%d curves=%d/%d" % (
    FC_A, FC_B, len(srcA.fcurves), len(srcB.fcurves)), flush=True)


def bone_of(path):
    return path.split('"')[1] if "pose.bones[" in path else None


def is_rot(path):
    return path == "rotation_quaternion" or ".rotation_quaternion" in path


def is_loc(path):
    return path == "location" or ".location" in path


def quat_map(action, t):
    out = {}
    for fc in action.fcurves:
        if is_rot(fc.data_path):
            out.setdefault(bone_of(fc.data_path), [None] * 4)[fc.array_index] = fc.evaluate(t)
    return out


Amap = {(fc.data_path, fc.array_index): fc for fc in srcA.fcurves}
Bmap = {(fc.data_path, fc.array_index): fc for fc in srcB.fcurves}

# ---- 拼接几何: 符号对齐 + 位移偏移 ----
AqJ, BqT = quat_map(srcA, JOIN), quat_map(srcB, TB)
flip_bones = set()
for bone, qa in AqJ.items():
    qb = BqT.get(bone)
    if qb is None or None in qa or None in qb:
        continue
    if sum(a * b for a, b in zip(qa, qb)) < 0:
        flip_bones.add(bone)
print("[align] sign-flip bones: %s" % sorted(flip_bones), flush=True)

off = {}
for key, bfc in Bmap.items():
    if is_loc(key[0]):
        afc = Amap.get(key)
        if afc is not None:
            off[key] = afc.evaluate(JOIN) - bfc.evaluate(TB)
print("[align] loc offsets: %d channels, rootY off=%.4f" % (
    len(off), off.get(("location", 1), 0.0)), flush=True)


def eval_hold(fc, f):
    """曲线取值; 越界区间用端点键保持(bezier 外推会漂移)."""
    kps = fc.keyframe_points
    first, last = kps[0].co[0], kps[-1].co[0]
    if f <= first:
        return kps[0].co[1]
    if f >= last:
        return kps[-1].co[1]
    return fc.evaluate(f)


def bval(path, idx, v):
    """B 段值变换: 旋转符号对齐 / 位移加偏移."""
    if is_rot(path) and bone_of(path) in flip_bones:
        v = -v
    if is_loc(path):
        v = v + off.get((path, idx), 0.0)
    return v


# ---- 值表 ----
def build_tables(sections):
    """sections: {src, lo, hi, dstoff, mode}
    mode: copy | blend_to_bt (源自身曲线 -> B@TB, 帧内 lerp)
          | fade_to_bt (源=B, -> B@TB)"""
    tables = {}
    for sec in sections:
        src, lo, hi, dstoff, mode = (sec["src"], sec["lo"], sec["hi"],
                                     sec["dstoff"], sec["mode"])
        smap = Amap if src is srcA else Bmap
        for key, fc in smap.items():
            path, idx = key
            tbl = tables.setdefault(key, {})
            if mode == "copy":
                for k in fc.keyframe_points:
                    f = k.co[0]
                    if lo - EPS <= f <= hi + EPS:
                        v = k.co[1]
                        if src is srcB:
                            v = bval(path, idx, v)
                        tbl[round(f + dstoff)] = v
            else:
                bfc = Bmap.get(key)
                if bfc is None:
                    continue
                tgt_frame = sec.get("tgt", TB)
                bt = bval(path, idx, eval_hold(bfc, tgt_frame))
                for f in range(lo, hi + 1):
                    w = (f - lo) / float(hi - lo)
                    # 两侧各自变换到输出空间后再 lerp; 不得二次 bval
                    # (曾致位移通道偏移双倍: 尾段漂移 -0.219 -> -0.437)
                    v0 = eval_hold(fc, f)
                    if mode.startswith("fade"):
                        v0 = bval(path, idx, v0)
                    v = v0 * (1 - w) + bt * w
                    tbl[round(f + dstoff)] = v
    return tables


def write_action(name, tables):
    old = bpy.data.actions.get(name)
    if old is not None:
        bpy.data.actions.remove(old)
        print("[build] removed stale %s" % name)
    act = bpy.data.actions.new(name)
    # 旋转通道逐帧归一化(按骨)
    rot_chans = {}
    for (path, idx), tbl in tables.items():
        if is_rot(path) and bone_of(path):
            rot_chans.setdefault(bone_of(path), {})[(path, idx)] = tbl
    for bone, chans in rot_chans.items():
        frames = set()
        for tbl in chans.values():
            frames |= set(tbl)
        for f in frames:
            q = [chans[k].get(f) for k in chans]
            if any(v is None for v in q):
                continue
            n = math.sqrt(sum(v * v for v in q))
            if n > 1e-9:
                for k in chans:
                    if f in chans[k]:
                        chans[k][f] /= n
    for (path, idx), tbl in sorted(tables.items()):
        dst = act.fcurves.new(data_path=path, index=idx)
        srcfc = Amap.get((path, idx)) or Bmap.get((path, idx))
        if srcfc is not None:
            if srcfc.group:
                g = act.groups.get(srcfc.group.name) or act.groups.new(srcfc.group.name)
                dst.group = g
            for m in srcfc.modifiers:
                if m.type == "LIMITS":
                    lm = dst.modifiers.new("LIMITS")
                    lm.use_restricted_range = m.use_restricted_range
                    lm.mute = m.mute
                    lm.frame_start = m.frame_start
                    lm.frame_end = m.frame_end
                    lm.min_x = m.min_x
                    lm.min_y = m.min_y
        kps = dst.keyframe_points
        for f in sorted(tbl):
            kps.add(1)
            k = kps[-1]
            k.co = (float(f), tbl[f])
            k.interpolation = 'BEZIER'
            k.handle_left_type = k.handle_right_type = 'AUTO_CLAMPED'
        kps.sort()
        dst.update()
    bumped = 0
    for fc in act.fcurves:
        if len(fc.keyframe_points) == 1:
            f0, v0 = fc.keyframe_points[0].co
            fc.keyframe_points.add(1)
            # 末帧单键向前补, 避免把 frameCount 顶过预期
            fc.keyframe_points[1].co = (f0 - 1.0 if f0 > 0 else f0 + 1.0, v0)
            fc.keyframe_points.sort()
            fc.update()
            bumped += 1
    hi = int(round(max(k.co[0] for fc in act.fcurves for k in fc.keyframe_points)))
    act.freehk.tetherFrame = arm
    act.freehk.starType = "LMT_Action"
    act.freehk.frameCount = hi
    act.use_fake_user = True
    print("[build] %-20s curves=%d end=%d min2+=%d" % (
        name, len(act.fcurves), hi, bumped), flush=True)
    return act


tables = build_tables([
    dict(src=srcA, lo=0, hi=JOIN - BW, dstoff=0, mode="copy"),
    dict(src=srcA, lo=JOIN - BW, hi=JOIN, dstoff=0, mode="blend_to_bt"),
    dict(src=srcB, lo=TB, hi=FC_B - BW, dstoff=JOIN - TB, mode="copy"),
    dict(src=srcB, lo=FC_B - BW, hi=FC_B, dstoff=JOIN - TB, mode="fade_to_b0", tgt=TB2),
    dict(src=srcB, lo=TB2, hi=TB - 1, dstoff=JOIN + FC_B - TB, mode="copy"),
])
start = write_action(NAME_START, tables)

tables_d = {}
for key, fc in Amap.items():
    tbl = tables_d.setdefault(key, {})
    # 边界键全量补齐: 骨的 4 通道必须齐全(缺通道 -> 导出器 Quaternion 空序列崩溃)
    tbl[0] = eval_hold(fc, JOIN)
    tbl[FC_A - JOIN] = eval_hold(fc, FC_A)
    for k in fc.keyframe_points:
        f = k.co[0]
        if JOIN + EPS < f < FC_A - EPS:
            tbl[round(f - JOIN)] = k.co[1]
done = write_action(NAME_DONE, tables_d)

assert int(start.freehk.frameCount) == END == 214, start.freehk.frameCount
assert int(done.freehk.frameCount) == FC_A - JOIN == 97, done.freehk.frameCount
try:
    start.freehk.loopFrame = JOIN
    print("[loop] freehk.loopFrame=%d (记录; 导出生效在 entry 节点 loopFrame=%d)" % (
        JOIN, JOIN), flush=True)
except Exception as e:
    print("[loop] freehk.loopFrame 未写入: %s" % e, flush=True)


# ---- 接缝复检 (成品动作上评估, 体骨) ----
def seam_angles(action, fa, fb):
    qa, qb = quat_map(action, fa), quat_map(action, fb)
    rows = []
    for bone, q1 in qa.items():
        if bone is None:
            continue
        q2 = qb.get(bone)
        if q1 is None or None in q1 or q2 is None or None in q2:
            continue
        n = bone.replace("MhBone_", "")
        if n.isdigit() and 247 <= int(n) <= 254:
            continue
        d = sum(x * y for x, y in zip(q1, q2))
        ang = 2 * math.degrees(math.acos(max(-1.0, min(1.0, abs(d))))) if abs(d) > 1e-9 else 0.0
        rows.append((ang, bone))
    rows.sort(reverse=True)
    return rows


for tag, fa, fb in (("head-seam 45->47", 45, 47),
                    ("B-seam 82->86", 82, 86), ("wrap 213->46", 213, 46)):
    rows = seam_angles(start, fa, fb)
    print("[seam] %s: worst %s" % (tag, [(round(a, 1), b) for a, b in rows[:4]]), flush=True)

# ---- TIML 防御帧 action ----
old = bpy.data.actions.get(TIML_NAME)
if old is not None:
    bpy.data.actions.remove(old)
timl = bpy.data.actions.new(TIML_NAME)
timl.freehk.starType = "TIML_Action"
timl.freehk.timelineParamRaw = PARAM_RAW
timl.freehk.unkn0 = 11   # PlMotionCommon 类目分派 id: 全文件 249 个 PMC 类型均为 11,
                         # 0=未定义类目会导致游戏加载崩溃(2026-10-04 实锤)
timl.use_fake_user = True


def timl_curve(path, idx, value):
    fc = timl.fcurves.new(data_path=path, index=idx)
    fc.keyframe_points.add(1)
    k = fc.keyframe_points[0]
    k.co = (0.0, float(value))
    k.interpolation = 'CONSTANT'
    fc.update()


timl_curve("FreeHKTiml." + FLAG_HASH, 0, FLAG_VALUE & 0xFFFF)
timl_curve("FreeHKTiml." + FLAG_HASH, 1, (FLAG_VALUE >> 16) & 0xFFFF)
timl_curve("FreeHKTiml." + TRIG_HASH, 0, TRIG_VALUE & 0xFFFF)
timl_curve("FreeHKTiml." + TRIG_HASH, 1, (TRIG_VALUE >> 16) & 0xFFFF)
print("[timl] %s: mFlag=%#010x mTrigger=%d @f0 (常开, 穿透循环)" % (
    TIML_NAME, FLAG_VALUE, TRIG_VALUE), flush=True)

# ---- 节点(不接线, GUI 里连到目标 entry) ----
tree = bpy.data.node_groups.get("w03_00")
assert tree is not None
maxy = max((n.location.y for n in tree.nodes), default=0.0)
minx = min((n.location.x for n in tree.nodes), default=0.0)
for nm, act, dy in ((NAME_START, start, 0), (NAME_DONE, done, -180)):
    n = tree.nodes.new("LMTActionNode")
    n.input_action = act
    n.location = (minx - 400, maxy - dy)
    print("[node] %s -> %s" % (n.name, act.name), flush=True)
tn = tree.nodes.new("TIMLActionNode")
tn.input_action = timl
tn.location = (minx - 400, maxy + 180)
print("[node] %s -> %s (接目标 entry 的 'TIML Data')" % (tn.name, timl.name), flush=True)

# ---- 验证 ----
sys.path.append(r"C:\Users\Afeather\AppData\Roaming\Blender Foundation\Blender\4.3\scripts\addons")
import importlib
lmt_exporter = importlib.import_module("MHW-FreeKinetics.blender.lmt_exporter")
ec = importlib.import_module("MHW-FreeKinetics.error_handling.errorController")


class _EO:
    def tuple(self):
        return ("harness",)


ec.errorOwner = lambda *a, **k: _EO()


class Opt:
    error_text_level = "Verbose"
    error_log_level = "All"
    fcurve_error = "Fix"
    action_error = "Fix"
    graph_error = "Fix"
    export_hidden = True


ok = True
for nm in (NAME_START, NAME_DONE):
    a = bpy.data.actions.get(nm)
    eh = ec.ErrorHandler(a, Opt())
    eh.takeOwnership(a)
    p = lmt_exporter.LMTActionParser(a, eh)
    trs = getattr(p, "transforms", []) or []
    roots = sum(1 for t in trs
                if getattr(t, "boneFunction", getattr(t, "boneId", None)) is not None
                and getattr(t, "boneFunction", getattr(t, "boneId", None)) < 0)
    errs = getattr(eh, "log", [])
    print("[verify] %-20s valid=%s frameCount=%s transforms=%d rootTracks=%d logged=%d" % (
        nm, p.valid, getattr(p, "frameCount", "?"), len(trs), roots, len(errs)), flush=True)
    if errs:
        from collections import Counter
        print("    codes:", dict(Counter(e.code for e in errs)), flush=True)
    ok = ok and bool(p.valid)
    if a.freehk.tetherFrame is not arm or a.freehk.starType != "LMT_Action":
        print("    !!! %s 元数据错误" % nm); ok = False
    if int(a.freehk.frameCount) not in (214, 97):
        print("    !!! %s frameCount=%s" % (nm, a.freehk.frameCount)); ok = False
    single = [fc.data_path for fc in a.fcurves if len(fc.keyframe_points) < 2]
    if single:
        print("    !!! %s 单键: %s" % (nm, single[:4])); ok = False
    nan = [fc.data_path for fc in a.fcurves
           if any(k.co[1] != k.co[1] for k in fc.keyframe_points)]
    if nan:
        print("    !!! %s NaN: %s" % (nm, nan[:4])); ok = False

# 公式验证: valor_sheath_start 接点(JOIN)后根位移恒定 (=(x,y)+B(f), B 静止)
for idx, ax in enumerate("XYZ"):
    fc = start.fcurves.find("location", index=idx)
    ks = [(k.co[0], k.co[1]) for k in fc.keyframe_points if k.co[0] >= JOIN - EPS]
    ref = ks[0][1]
    drift = max(abs(v - ref) for _, v in ks)
    print("[verify] root %s @>=%d: %d keys const=%.4f maxdrift=%.5f" % (
        ax, JOIN, len(ks), ref, drift), flush=True)
    if drift > 1e-3:
        print("    !!! 根位移 %s 接点后漂移 %.4f > 1mm" % (ax, drift)); ok = False

fh = importlib.import_module("MHW-FreeKinetics.blender.nodetree.freeHKActionNodes")
teh = ec.ErrorHandler(timl, Opt())
teh.takeOwnership(timl)
struct = fh.ExportTIMLTypesNode(timl, teh).structure()
got = {}
for t in struct.transforms:
    for kf in t.keyframes:
        got["%08X" % t.datatypeHash] = int(kf.value)
print("[verify] TIML transforms=%d %s" % (struct.count, got), flush=True)
for h, v in ((FLAG_HASH[1:], FLAG_VALUE), (TRIG_HASH[1:], TRIG_VALUE)):
    if got.get(h) != v:
        print("    !!! TIML %s = %s != %s" % (h, got.get(h), v)); ok = False

print("[result] %s" % ("ALL GREEN" if ok else "FAILURES — NOT SAVING"), flush=True)
if not ok:
    sys.exit(1)

bpy.ops.wm.save_mainfile()
print("[done] saved:", BLEND, flush=True)
