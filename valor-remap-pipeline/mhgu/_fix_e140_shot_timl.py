# -*- coding: utf-8 -*-
"""E140 去强化射击 TIML 修复 (headless):
⚠ 历史脚本(2026-10-09 早): 针对"射击族 TIML 挂在 E140"的当时状态。此后用户重排了
140/338 接线(E140=原338射击内容, E338=刚1), 直接重跑会按旧前提误操作; 保留价值 =
"从 origin 导入原版 TIML 并接线"的完整做法范例(全流程见 SPIRIT_SHOT_Research.md)。
背景: 140/342 两 entry 的内容被对调后, 射击族 TIML (EventGroup09 7844 等) 挂在了
E140 (见切) 上 -> 局内见切播强化射击音效。本脚本:
  1. 摘掉 E140 的射击族 TIML Data 链接, 删旧 data 节点 + 8 个射击 tim 节点 (action 保留);
  2. 用 FreeKinetics 自带解析器从 w03_00-origin.lmt (原版) 导入 entry 140 的见切 TIML
     (14 tim), 走 TIMLStructure 正式代码路径建节点/动作并接到 E140;
  3. 新节点摆到旧簇坐标 (GUI 布局不变);
  4. 断言: E140 恰 1 条 TIML Data 线; E342 接线/动作完全未动;
  5. exportNodeToFile 正式导出到 OUT 供字节级验证。不保存 blend (由调用方决定)。

用法: OUT=... blender -b MHW-Valor.blend --python _fix_e140_shot_timl.py
"""
import bpy, os, sys

ORIGIN = r"E:\SteamLibrary\steamapps\common\Monster Hunter World\nativePC\hm\wp\wp03\mot\w03_00\w03_00-origin.lmt"
OUT = os.environ.get("OUT", r"E:\MHW-arts\_shotfix_verify.lmt")
ENTRY = 140

def out(s):
    print(s, flush=True)

out("FILE: " + bpy.data.filepath)

# ---------- 1. 定位 E140 与旧射击簇 ----------
tree = bpy.data.node_groups["w03_00"]
entry = next(n for n in tree.nodes if n.bl_idname == "LMTEntryNode" and n.entryNum == ENTRY)
sock = next(i for i in entry.inputs if i.name == "TIML Data")
old_links = list(sock.links)
assert len(old_links) == 1, f"E140 TIML Data 应恰 1 条线, 实际 {len(old_links)}"
old_data = old_links[0].from_node
old_tims = [l.from_node for l in tree.links
            if l.to_node == old_data and l.to_socket.name == "TIML Animation"]
old_actions = [getattr(t, "input_action", None) for t in old_tims]
out(f"[old] E140 data node '{old_data.name}' @({old_data.location.x:.0f},{old_data.location.y:.0f}) nTIM={len(old_tims)}")
for t, a in zip(old_tims, old_actions):
    out(f"      del-candidate tim @({t.location.x:.0f},{t.location.y:.0f}) act={a.name if a else None}")
assert any(a and "EventGroup09" in a.name for a in old_actions), "旧簇里没有 EventGroup09, 目标不对, 中止"

# 记录旧簇坐标 (data + tims 按 y 降序)
old_data_pos = (old_data.location.x, old_data.location.y)
old_tim_pos = [(t.location.x, t.location.y) for t in
               sorted(old_tims, key=lambda n: -n.location.y)]

# E342 状态快照 (修复后必须逐项相同)
e342 = next(n for n in tree.nodes if n.bl_idname == "LMTEntryNode" and n.entryNum == 342)
def snap_entry(e):
    lmt = [(l.from_node.name, getattr(l.from_node, "input_action", None) and l.from_node.input_action.name)
           for l in tree.links if l.to_node == e and l.to_socket.name == "LMT Animation"]
    tim = []
    for l in tree.links:
        if l.to_node == e and l.to_socket.name == "TIML Data":
            d = l.from_node
            tims = sorted((l2.from_node.input_action.name
                           for l2 in tree.links
                           if l2.to_node == d and l2.to_socket.name == "TIML Animation"
                           and l2.from_node.input_action))
            tim.append((d.name, d.get("unkn1"), d.get("unkn2"), d.get("animLength"),
                        d.get("loopStartPoint"), d.get("loopControl"), tuple(tims)))
    return (tuple(lmt), tuple(tim))
snap342_before = snap_entry(e342)

# ---------- 2. 从原版 LMT 解析 entry140 TIML ----------
# 包名带连字符, from-import 是语法错误, 只能 importlib
import importlib
_struct = importlib.import_module("MHW-FreeKinetics.struct.Lmt")
_timimp = importlib.import_module("MHW-FreeKinetics.blender.timl_importer")
_nops = importlib.import_module("MHW-FreeKinetics.blender.nodetree.freeHKNodeOps")
parseLMT = _struct.parseLMT
processTIMLData = _timimp.processTIMLData
remapAnalyzer = _timimp.remapAnalyzer
TIMLStructure = _timimp.TIMLStructure
uiNodeManager = _nops.uiNodeManager

lmt = parseLMT(ORIGIN)
# ActionHeaders 是紧凑列表, 索引≠entry id, 必须按 id 找
act = next(a for a in lmt.ActionHeaders if a.id == ENTRY)
assert act.timlOffset, "origin entry140 无 TIML"
datum = act.timl
out(f"[origin] entry{ENTRY} timl types={len(datum.types)} "
    f"dataIx0={datum.dataIx0} dataIx1={datum.dataIx1} animLen={datum.animationLength} "
    f"loopStart={datum.loopStartPoint} loopCtrl={datum.loopControl}")
assert len(datum.types) == 14, f"origin entry140 TIML type 数 {len(datum.types)} != 14 (见切应为 14)"
processed = processTIMLData(datum, remapAnalyzer)

# ---------- 3. 摘旧接新 ----------
for l in old_links:
    tree.links.remove(l)
mgr = uiNodeManager(tree, False)  # create* 方法挂在 manager 上, TIMLStructure 吃 manager
data_nodes = TIMLStructure([processed], mgr, [entry])
assert len(data_nodes) == 1
new_data = data_nodes[0]

# 新 tim 节点 = 指向 new_data 的节点, 按创建顺序 (自动布局 y 降序=自上而下) 摆到旧坐标
new_tims = [l.from_node for l in tree.links
            if l.to_node == new_data and l.to_socket.name == "TIML Animation"]
assert len(new_tims) == len(processed.types) == 14, f"新簇 tim 数 {len(new_tims)} != 14"
new_tims_sorted = sorted(new_tims, key=lambda n: -n.location.y)  # y 降序 = 自上而下 = 创建序
# 旧簇只有 8 个坐标, 多出的按 -30 网格续排 (保持 GUI 列队整齐)
if len(old_tim_pos) >= 2:
    step = old_tim_pos[0][1] - old_tim_pos[1][1]
    x0 = old_tim_pos[0][0]
    while len(old_tim_pos) < len(new_tims_sorted):
        px, py = old_tim_pos[-1]
        old_tim_pos.append((x0, py - step))
for (x, y), t in zip(old_tim_pos, new_tims_sorted):
    t.location = (x, y)
new_data.location = old_data_pos
out(f"[new] data node '{new_data.name}' props unkn1={new_data.unkn1} unkn2={new_data.unkn2} "
    f"animLength={new_data.animLength} loopStart={new_data.loopStartPoint} loopCtrl={new_data.loopControl}")
for t in new_tims_sorted:
    a = t.input_action
    out(f"      new tim @({t.location.x:.0f},{t.location.y:.0f}) act={a.name if a else None}")

# 删除旧簇节点 (action 保留, 不进任何清理)
for t in old_tims:
    tree.nodes.remove(t)
tree.nodes.remove(old_data)

# ---------- 4. 断言 ----------
links_now = list(sock.links)
assert len(links_now) == 1 and links_now[0].from_node == new_data, "E140 TIML Data 接线不唯一"
assert all(l.to_node != old_data for l in tree.links), "旧 data 节点仍有连线"
assert snap_entry(e342) == snap342_before, "E342 接线被意外改动!"
lmt_links = [l for l in tree.links if l.to_node == entry and l.to_socket.name == "LMT Animation"]
assert len(lmt_links) == 1, "E140 LMT Animation 线数异常"
out("[assert] E140 恰 1 条 TIML Data 线 -> 新见切簇; E342 逐项未动; LMT 动作线未动")

# ---------- 5. 正式导出验证 ----------
scn = bpy.context.scene
scn.freehk_tether = bpy.data.objects["m_mesh Armature"]
fnode = [n for n in tree.nodes if n.bl_idname == "LMTFileNode"][0]
fnode.filepath = OUT
eo = importlib.import_module("MHW-FreeKinetics.operators.export_ops")
addon = bpy.context.preferences.addons.get(eo.__package__.split(".")[0])
eo.exportNodeToFile(fnode, addon.preferences)
out("[export] OK -> " + OUT)

# ---------- 6. 备份 + 保存 blend ----------
import shutil
blend = bpy.data.filepath
bak = blend + ".bak-20261009-pre140shotfix"
if not os.path.exists(bak):
    shutil.copy2(blend, bak)
    out("[bak] " + bak)
bpy.ops.wm.save_mainfile()
out("[saved] " + bpy.data.filepath)
out("[done]")
