# -*- coding: utf-8 -*-
"""E342 攻击帧植入 (headless): 深拷贝 E104 的 TIML 簇接到 E342, 攻击窗改 [34,42]。

背景: E342 (valor_sheath_divide) 只挂了一个空 TIML Data 节点 (0 tim), 没有攻击判定。
做法 (user 指定): TIML 从 E104 (気刃斬り1) 整簇复制 —— action.copy() 深拷贝
(freehk.timelineParamRaw 类型码 + fcurve 键全在新 datablock 上), 不与 E104 共享任何
引用, 改副本绝不波及 104。唯一编辑 = 副本 EventCollision00 的 m08FD20A6 大标志轨:
窗口从 104 的 [54,62]/[109,116]/[130,137] 改为单窗口 [34,42] (值 2 与 104 首窗一致)。
E342 复用自己的 Data 节点 (头 30/60/132/-1/0 保持 342 槽位身份), 只灌入 14 条 tim。

断言: E104 簇前后逐键快照全等; 其余全部 entry 接线快照不变; 副本与原版仅两条被改
fcurve 不同; 导出 pre/post 两个 LMT 供 _bytediff_lmt.py 验证 (期望仅 342 区域差异)。
备份 blend 后保存。

用法: PRE=... POST=... blender -b MHW-valor-blender/MHW-Valor-0.0.2-342-divide.blend \
        --python _copy_e104_timl_to_e342.py
"""
import bpy, os, importlib, shutil

PRE = os.environ.get("PRE", r"E:\MHW-arts\valor-remap-pipeline\mhgu\_e342_pre_export.lmt")
POST = os.environ.get("POST", r"E:\MHW-arts\valor-remap-pipeline\mhgu\_e342_post_export.lmt")
ENTRY_SRC, ENTRY_DST = 104, 342
WINDOW = (34.0, 42.0)   # 攻击窗 (TIML 60fps 帧)
WINDOW_VAL = 2.0        # 与 104 首窗同值 (ReqNo B)

def out(s):
    print(s, flush=True)

out("FILE: " + bpy.data.filepath)
tree = bpy.data.node_groups["w03_00"]

# ---------------- 快照工具 ----------------
def idprop_ser(v):
    if hasattr(v, "items"):  # IDPropertyGroup (4.3 起类名不在 bpy.types, 按 mapping 判定)
        return {k: idprop_ser(x) for k, x in v.items()}
    if v is None or isinstance(v, (int, float, str, bool)):
        return v
    try:
        return list(v)
    except TypeError:
        return repr(v)

def fc_snap(fc):
    return (fc.data_path, fc.array_index, fc.mute,
            tuple((round(k.co[0], 5), round(k.co[1], 5),
                   round(k.handle_left[0], 5), round(k.handle_left[1], 5),
                   round(k.handle_right[0], 5), round(k.handle_right[1], 5),
                   k.interpolation, k.back, k.period,
                   k.handle_left_type, k.handle_right_type)
                  for k in fc.keyframe_points))

def act_snap(a):
    if a is None:
        return None
    return {"name": a.name, "fs": a.frame_start, "fe": a.frame_end,
            "freehk": idprop_ser(a.freehk),
            "fcs": sorted(fc_snap(fc) for fc in a.fcurves)}

def tims_of(data_node):
    ls = [l.from_node for l in tree.links
          if l.to_node == data_node and l.to_socket.name == "TIML Animation"]
    return sorted(ls, key=lambda n: -n.location.y)  # y 降序 = 创建序

def cluster_snap(entry):
    dat = [l.from_node for l in tree.links
           if l.to_node == entry and l.to_socket.name == "TIML Data"]
    lmt = [l.from_node.input_action.name if l.from_node.input_action else None
           for l in tree.links if l.to_node == entry and l.to_socket.name == "LMT Animation"]
    cs = []
    for d in dat:
        cs.append({"data": d.name,
                   "hdr": (d.get("unkn1"), d.get("unkn2"), d.get("animLength"),
                           d.get("loopStartPoint"), d.get("loopControl")),
                   "tims": [(t.name, round(t.location.x, 2), round(t.location.y, 2),
                             act_snap(t.input_action)) for t in tims_of(d)]})
    return {"dat": cs, "lmt": tuple(lmt)}

entries = {n.entryNum: n for n in tree.nodes if n.bl_idname == "LMTEntryNode"}
e_src, e_dst = entries[ENTRY_SRC], entries[ENTRY_DST]
snap_all_before = {num: cluster_snap(e) for num, e in entries.items()}
snap_src = snap_all_before[ENTRY_SRC]

# 104 簇现状硬校验 (与探针一致才继续)
assert len(snap_src["dat"]) == 1
src_d = snap_src["dat"][0]
assert src_d["hdr"] == (53, 102, 142, -1, 0), src_d["hdr"]
assert len(src_d["tims"]) == 14, len(src_d["tims"])
src_tims = tims_of([l.from_node for l in tree.links
                    if l.to_node == e_src and l.to_socket.name == "TIML Data"][0])
src_acts = [t.input_action for t in src_tims]
assert all(a is not None for a in src_acts)
src_cname = next(a.name for a in src_acts
                 if a.name.startswith("TIML::nTimelineParam::nWwiseTimeline::EventCollision00"))
src_cact = next(a for a in src_acts if a.name == src_cname)
for ix in (0, 1):
    fc = next(fc for fc in src_cact.fcurves
              if fc.data_path == "FreeHKTiml.m08FD20A6" and fc.array_index == ix)
    assert len(fc.keyframe_points) == 7, (ix, len(fc.keyframe_points))
out(f"[src] E{ENTRY_SRC} data '{src_d['data']}' hdr={src_d['hdr']} nTIM=14 CTO-act='{src_cname}'")

# E342 现状: 空 data 节点
dst_snap = snap_all_before[ENTRY_DST]
assert len(dst_snap["dat"]) == 1 and len(dst_snap["dat"][0]["tims"]) == 0, \
    f"E342 应恰 1 个空 data 节点, 实际 {dst_snap}"
dst_d = [l.from_node for l in tree.links
         if l.to_node == e_dst and l.to_socket.name == "TIML Data"][0]
dst_hdr = (dst_d.get("unkn1"), dst_d.get("unkn2"), dst_d.get("animLength"),
           dst_d.get("loopStartPoint"), dst_d.get("loopControl"))
out(f"[dst] E{ENTRY_DST} data '{dst_d.name}' hdr={dst_hdr} nTIM=0 LMT<-{dst_snap['lmt']}")
assert dst_d.outputs["TIML Data"].links and \
    all(l.to_node == e_dst for l in dst_d.outputs["TIML Data"].links), \
    "342 的 data 节点还接着别的 entry, 不能复用"

# ---------------- PRE 导出 (改前基线) ----------------
scn = bpy.context.scene
scn.freehk_tether = bpy.data.objects["m_mesh Armature"]
fnode = [n for n in tree.nodes if n.bl_idname == "LMTFileNode"][0]
fp_orig = fnode.filepath
eo = importlib.import_module("MHW-FreeKinetics.operators.export_ops")
addon = bpy.context.preferences.addons.get(eo.__package__.split(".")[0])
fnode.filepath = PRE
eo.exportNodeToFile(fnode, addon.preferences)
out("[export-pre] " + PRE)

# ---------------- 深拷贝 14 条 tim ----------------
importlib.import_module("MHW-FreeKinetics.struct.Lmt")  # 预热 addon 模块搜索路径
_nops = importlib.import_module("MHW-FreeKinetics.blender.nodetree.freeHKNodeOps")
mgr = _nops.uiNodeManager(tree, False)
new_pairs = []  # (new_action, new_node)
for t in src_tims:
    na = t.input_action.copy()
    node = mgr.createTIMLActionNode(na, outputs=[dst_d])  # 正规落位+接 "TIML Animation"
    new_pairs.append((na, node))
out(f"[copy] {len(new_pairs)} tim actions 深拷贝并接入 E{ENTRY_DST} data")

# ---------------- 改副本攻击窗 [34,42] ----------------
dst_cname = next(na.name for na, _ in new_pairs
                 if na.name.startswith("TIML::nTimelineParam::nWwiseTimeline::EventCollision00"))
dst_cact = next(na for na, _ in new_pairs if na.name == dst_cname)
for ix, v in ((0, WINDOW_VAL), (1, 0.0)):
    fc = next(fc for fc in dst_cact.fcurves
              if fc.data_path == "FreeHKTiml.m08FD20A6" and fc.array_index == ix)
    kp = fc.keyframe_points
    assert len(kp) == 7, (ix, len(kp))
    while len(kp) > 3:            # 109/116/130/137 窗全部移除 (每次重取引用, 防陈旧包装)
        kp.remove(kp[3])
    kp[1].co = (WINDOW[0], v)     # 54 -> 34
    kp[2].co = (WINDOW[1], 0.0)   # 62 -> 42
    fc.update()
    got = [(round(k.co[0], 3), round(k.co[1], 3)) for k in fc.keyframe_points]
    assert got == [(0.0, 0.0), (WINDOW[0], v), (WINDOW[1], 0.0)], (ix, got)
out(f"[edit] '{dst_cname}' m08FD20A6 ix0={[round(k.co[0]) for k in next(fc for fc in dst_cact.fcurves if fc.data_path=='FreeHKTiml.m08FD20A6' and fc.array_index==0).keyframe_points]}")

# ---------------- 断言 ----------------
# 1) 104 簇逐键未动
assert cluster_snap(e_src) == snap_src, "E104 簇被意外改动!"
out("[assert] E104 簇快照前后全等 (14 tim 逐键/属性/坐标)")
# 2) 副本与原版: 除名字与被改的 m08FD20A6 外逐项相同
d = act_snap(dst_cact)
s = act_snap(src_cact)
assert d["freehk"] == s["freehk"], "副本 freehk 类型码与原版不一致!"
edited = [fc for fc in d["fcs"] if fc[:2] == ("FreeHKTiml.m08FD20A6", 0) or
          fc[:2] == ("FreeHKTiml.m08FD20A6", 1)]
untouched = [fc for fc in d["fcs"] if fc not in edited]
assert all(fc in s["fcs"] for fc in untouched), "副本存在被编辑轨道之外的差异!"
out(f"[assert] 副本 vs 原版: freehk 全等, fcurve {len(untouched)}/{len(d['fcs'])} 条逐键全等, 仅 m08FD20A6 ix0/ix1 按窗改动")
# 3) 新 action 无二次引用; 104 原 action 引用计数不变
all_tim_nodes = [n for n in tree.nodes if n.bl_idname == "TIMLActionNode"]
for na, node in new_pairs:
    users = [t for t in all_tim_nodes if t.input_action == na]
    assert users == [node], f"副本 {na.name} 被异常引用!"
assert [t.input_action for t in tims_of(dst_d)] == [na for na, _ in new_pairs]
assert [t.input_action.name for t in tims_of([l.from_node for l in tree.links
        if l.to_node == e_src and l.to_socket.name == "TIML Data"][0])] == \
    [a.name for a in src_acts]
out("[assert] 14 个副本各恰 1 个节点引用; 104 的原 action 引用原样")
# 4) 其余 entry 全部不变
snap_all_after = {n.entryNum: cluster_snap(n)
                  for n in tree.nodes if n.bl_idname == "LMTEntryNode"}
for num in snap_all_after:
    if num == ENTRY_DST:
        continue
    assert snap_all_after[num] == snap_all_before[num], f"E{num} 接线被意外改动!"
out(f"[assert] 全部 {len(snap_all_after)-1} 个其他 entry 快照不变")
# 5) E342 终态
after_dst = snap_all_after[ENTRY_DST]
assert len(after_dst["dat"]) == 1 and after_dst["dat"][0]["hdr"] == dst_hdr \
    and len(after_dst["dat"][0]["tims"]) == 14 and after_dst["lmt"] == dst_snap["lmt"]
out(f"[assert] E342 终态: data 头 {dst_hdr} 保持, tim=14, LMT 动作线未动")

# ---------------- POST 导出 + 备份 + 保存 ----------------
fnode.filepath = POST
eo.exportNodeToFile(fnode, addon.preferences)
out("[export-post] " + POST)
fnode.filepath = fp_orig  # 还原 GUI 导出路径再存盘
blend = bpy.data.filepath
bak = blend + ".bak-20261011-pre342timl"
if not os.path.exists(bak):
    shutil.copy2(blend, bak)
    out("[bak] " + bak)
bpy.ops.wm.save_mainfile()
out("[saved] " + bpy.data.filepath)
out("[done]")
