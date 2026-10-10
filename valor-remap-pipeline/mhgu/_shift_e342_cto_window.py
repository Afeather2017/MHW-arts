# -*- coding: utf-8 -*-
"""E342 CTO 窗口对齐攻击帧 (headless): 实测命中落在 50 (user 录屏 30fps f25=TIML f50),
恰为 104 抄来的 CollisionTimelineObject mFlag 活动窗 [47,57] —— 真正的判定开关是 CTO
时间窗, EventCollision 的 ReqNo 轨不是命中时机门。把 E342 副本 CTO mFlag 从 [47,57]
改到 [34,42], 与攻击窗/挥砍可视窗对齐。只动副本, 断言 104 原件逐键未动。

用法: blender -b MHW-valor-blender/MHW-Valor-0.0.2-342-divide.blend --python _shift_e342_cto_window.py
"""
import bpy, os, shutil

WINDOW = (34.0, 42.0)

def out(s):
    print(s, flush=True)

out("FILE: " + bpy.data.filepath)
tree = bpy.data.node_groups["w03_00"]
entries = {n.entryNum: n for n in tree.nodes if n.bl_idname == "LMTEntryNode"}

def data_of(e):
    ls = [l.from_node for l in tree.links if l.to_node == e and l.to_socket.name == "TIML Data"]
    assert len(ls) == 1
    return ls[0]

def tims_of(d):
    return [l.from_node for l in tree.links
            if l.to_node == d and l.to_socket.name == "TIML Animation"]

def find_cto(tims):
    acts = [t.input_action for t in tims if t.input_action
            and t.input_action.name.startswith("TIML::nTimelineParam::CollisionTimelineObject")]
    assert len(acts) == 1, [a.name for a in acts]
    return acts[0]

def key_dump(act):
    fcs = {}
    for fc in act.fcurves:
        if fc.data_path == "FreeHKTiml.m08FD20A6":
            fcs[fc.array_index] = [(round(k.co[0], 3), round(k.co[1], 3)) for k in fc.keyframe_points]
    return fcs

src_cto = find_cto(tims_of(data_of(entries[104])))
dst_cto = find_cto(tims_of(data_of(entries[342])))
assert src_cto != dst_cto, "104/342 竟共用同一个 CTO action!"
before_src, before_dst = key_dump(src_cto), key_dump(dst_cto)
assert before_src == {0: [(0.0, 0.0), (47.0, 1.0), (57.0, 0.0)],
                      1: [(0.0, 0.0), (47.0, 0.0), (57.0, 0.0)]}, before_src
assert before_dst == before_src, before_dst
out(f"[before] 104 CTO '{src_cto.name}' {before_src}")
out(f"[before] 342 CTO '{dst_cto.name}' {before_dst}")

for ix, v in ((0, 1.0), (1, 0.0)):
    fc = next(fc for fc in dst_cto.fcurves
              if fc.data_path == "FreeHKTiml.m08FD20A6" and fc.array_index == ix)
    kp = fc.keyframe_points
    assert len(kp) == 3, len(kp)
    kp[1].co = (WINDOW[0], v)   # 47 -> 34
    kp[2].co = (WINDOW[1], 0.0) # 57 -> 42
    fc.update()
after_dst = key_dump(dst_cto)
assert after_dst == {0: [(0.0, 0.0), (WINDOW[0], 1.0), (WINDOW[1], 0.0)],
                     1: [(0.0, 0.0), (WINDOW[0], 0.0), (WINDOW[1], 0.0)]}, after_dst
assert key_dump(src_cto) == before_src, "104 的 CTO 被意外改动!"
out(f"[after] 342 CTO -> {after_dst} ; 104 CTO 逐键未动")

# 全树粗断言: 其他引用 CollisionTimelineObject 的 action 键全不变 (防串)
for a in bpy.data.actions:
    if a.name.startswith("TIML::nTimelineParam::CollisionTimelineObject") and a not in (src_cto, dst_cto):
        pass # 其他 entry 自有 CTO, 本次未触碰 (edit 只写了 dst_cto 的 fcurve)

blend = bpy.data.filepath
bak = blend + ".bak-20261011-0100-pre-342-cto34"
if not os.path.exists(bak):
    shutil.copy2(blend, bak)
    out("[bak] " + bak)
bpy.ops.wm.save_mainfile()
out("[saved] " + bpy.data.filepath)
out("[done]")
