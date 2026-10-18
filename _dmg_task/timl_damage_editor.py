# -*- coding: utf-8 -*-
"""TIML 伤害引用编辑器 — FreeKinetics 扩展 (CollisionTimelineObject ノードID).

机制 (COL_AttackBox_Research 实证): CollisionTimelineObject 的 ノードID 键值
→ wp03.col names[unk4==值] → {cgp=CLGM bank(判定盒几何), move=ATK 行(伤害)}.
改引用 = 伤害与判定几何一起换 (名称表条目把两者绑在一起).

多段引用 = 同轨多键 (原版 LMT155 模式: f0=48 → f25=60, 引擎逐帧采样, 分段自动切换).
键律: 整数帧 (小数帧是已知崩溃源); 新键 interpolation/back/period 复制帧前最近键
(BEZIER 会 ValueError 崩导出, interpolationMapping 只认
CONSTANT/LINEAR/QUAD/CUBIC/QUART/EXPO/SINE).

用法 (GUI): "TIML 窗口编辑" 面板选中 CollisionTimelineObject 的 action
            → "伤害引用" 分区: 当前键(名称/MV 解析) + 设引用 + 删键.
用法 (无头): bpy.ops.freehk.timl_dmgnode_set(action_name="...", frame=0, node=89)
             bpy.ops.freehk.timl_dmgnode_del(action_name="...", frame=58)
"""
import os
import struct

import bpy
from bpy.props import IntProperty, StringProperty

CTO_NODE_HASH = 0xB296CA66   # CollisionTimelineObject ノードID[0]
CTO_BANK_HASH = 0xBF9000B3   # CollisionTimelineObject バンク[0]
DEFAULT_COL = (r"E:\SteamLibrary\steamapps\common\Monster Hunter World"
               r"\nativePC\hm\wp\wp03\collisi on\wp03.col")

# ---- wp03.col names/ATK 解析 (节点 → 名称/bank/ATK行 → Motion_Value) --------

_col_cache = {}   # path -> (mtime, {node: (name, bank, move)}, [ATK Motion_Value])


def col_names(path):
    """→ ({node: (name, bank, move)}, [MV per ATK row]); 解析失败 → ({}, [])."""
    try:
        mt = os.path.getmtime(path)
    except OSError:
        return {}, []
    hit = _col_cache.get(path)
    if hit and hit[0] == mt:
        return hit[1], hit[2]
    try:
        with open(path, "rb") as h:
            d = h.read()
        nc = struct.unpack_from("<I", d, 0x14)[0]
        o = 0x18
        names = {}
        for i in range(nc):
            e = d.index(b"\x00", o)
            nm = d[o:e].decode("utf-8", "replace")
            bank, move = struct.unpack_from("<ii", d, e + 1)
            node = struct.unpack_from("<I", d, e + 16)[0]
            names[node] = (nm, bank, move)
            o = e + 20
        q = o
        while d[q + 4:q + 8] != b"ATK\x00":
            q += 1
        mc = struct.unpack_from("<I", d, q + 12)[0]
        body = q + 20
        mvs = [struct.unpack_from("<f", d, body + k * 218 + 8)[0] for k in range(mc)]
    except Exception:
        return {}, []
    _col_cache[path] = (mt, names, mvs)
    return names, mvs


def resolve(names, mvs, node):
    """node → "名称 [bank→ATK行] MV=xx"; col 未收录 → ""."""
    info = names.get(node)
    if not info:
        return ""
    nm, bank, move = info
    mv = (" MV=%.0f" % mvs[move]) if 0 <= move < len(mvs) else ""
    return "%s [bank%d→ATK%d]%s" % (nm or "(无名)", bank, move, mv)


# ---- TIML fcurve 访问 (与 timl_window_editor 同表示: FreeHKTiml.m<hash>) ----

def _node_fcurve(action, create=False):
    path = "FreeHKTiml.m%08X" % CTO_NODE_HASH
    fc = action.fcurves.find(path, index=0)
    if fc is None and create:
        fc = action.fcurves.new(data_path=path, index=0)
    return fc


def _holding(fcurve, frame):
    """帧前最近键的 (interpolation, back, period); 空轨 → ('CONSTANT', 0, 0)."""
    best = None
    for kf in fcurve.keyframe_points:
        if kf.co[0] <= frame:
            best = kf
        else:
            break
    if best is None:
        kfs = list(fcurve.keyframe_points)
        best = kfs[0] if kfs else None
    if best is None:
        return "CONSTANT", 0, 0
    return best.interpolation, int(best.back), int(best.period)


def _insert_key(fcurve, frame, value, interp, back, period):
    kf = fcurve.keyframe_points.insert(int(frame), int(value))
    kf.interpolation = interp
    kf.back = back
    kf.period = period
    kf.co = (int(frame), int(value))  # insert 可能保留小数帧, 强制整数
    return kf


def describe_damage_ref(action, scene):
    names, mvs = col_names(scene.freehk_timl_col)
    out = []
    bfc = action.fcurves.find("FreeHKTiml.m%08X" % CTO_BANK_HASH, index=0)
    if bfc is not None and len(bfc.keyframe_points):
        out.append("バンク[0]=%d (仅记录, 路由以 col 名称表为准)"
                   % int(bfc.keyframe_points[0].co[1]))
    fc = _node_fcurve(action)
    if fc is None or not len(fc.keyframe_points):
        return out + ["(无 ノードID 键)"]
    for kf in fc.keyframe_points:
        node = int(kf.co[1])
        tag = resolve(names, mvs, node)
        out.append("f%-3d = %-3d %s" % (int(kf.co[0]), node, tag or "(col 未收录)"))
    return out


# ---- Operators ---------------------------------------------------------------

class _DmgOp:
    bl_options = {"REGISTER", "UNDO"}
    # Operator 不支持数据块 PointerProperty → 用名字引用; 空则取 WM 选择器
    action_name: StringProperty(name="TIML Action", options={'HIDDEN'})

    def _target(self, context):
        name = getattr(self, "action_name", "")
        if name:
            a = bpy.data.actions.get(name)
            if a is None:
                raise ValueError("TIML action 不存在: %s" % name)
            return a
        return context.window_manager.freehk_timl_edit


def _scene_or(context, own, scene_name, default):
    if own is not None and own >= 0:
        return int(own)
    return int(getattr(context.scene, scene_name, default))


class FREEHK_OT_timl_dmgnode_set(_DmgOp, bpy.types.Operator):
    bl_idname = "freehk.timl_dmgnode_set"
    bl_label = "设伤害引用"
    bl_description = ("设 ノードID 键值: '帧'处有键改值, 无键加键. "
                      "多键 = 分段引用 (原版 LMT155 模式, 引擎逐帧采样自动切换). "
                      "伤害与判定几何一起换 (col 名称表绑定)")
    frame: IntProperty(name="帧", default=-1)
    node: IntProperty(name="节点ID", default=-1)

    def execute(self, context):
        a = self._target(context)
        if not a:
            self.report({'ERROR'}, "未选择 TIML action")
            return {'CANCELLED'}
        if "CollisionTimelineObject" not in a.name:
            self.report({'ERROR'}, "不是 CollisionTimelineObject action: %s" % a.name)
            return {'CANCELLED'}
        try:
            frame = _scene_or(context, self.frame, "freehk_timl_start", 0)
            node = _scene_or(context, self.node, "freehk_timl_node", 0)
            if frame < 0:
                raise ValueError("帧需 >= 0, 得到 %d" % frame)
            if node < 0:
                raise ValueError("节点ID 需 >= 0, 得到 %d" % node)
            fc = _node_fcurve(a, create=True)
            hit = None
            for kf in fc.keyframe_points:
                if int(kf.co[0]) == frame:
                    hit = kf
                    break
            if hit is not None:
                hit.co[1] = float(node)
                mode = "改值"
            else:
                interp, back, period = _holding(fc, frame)
                _insert_key(fc, frame, node, interp, back, period)
                mode = "加键"
            fc.update()
        except ValueError as e:
            self.report({'ERROR'}, str(e))
            return {'CANCELLED'}
        names, mvs = col_names(context.scene.freehk_timl_col)
        self.report({'INFO'}, "%s: f%d = %d %s" % (mode, frame, node,
                                                   resolve(names, mvs, node)))
        return {'FINISHED'}


class FREEHK_OT_timl_dmgnode_del(_DmgOp, bpy.types.Operator):
    bl_idname = "freehk.timl_dmgnode_del"
    bl_label = "删伤害引用键"
    bl_description = "删除'帧'处的 ノードID 键 (至少保留 1 键, 无键 CTO 无法路由伤害)"
    frame: IntProperty(name="帧", default=-1)

    def execute(self, context):
        a = self._target(context)
        if not a:
            self.report({'ERROR'}, "未选择 TIML action")
            return {'CANCELLED'}
        if "CollisionTimelineObject" not in a.name:
            self.report({'ERROR'}, "不是 CollisionTimelineObject action: %s" % a.name)
            return {'CANCELLED'}
        try:
            frame = _scene_or(context, self.frame, "freehk_timl_start", 0)
            fc = _node_fcurve(a)
            if fc is None:
                raise ValueError("该 action 无 ノードID 轨")
            hit = None
            for kf in fc.keyframe_points:
                if int(kf.co[0]) == frame:
                    hit = kf
                    break
            if hit is None:
                raise ValueError("f%d 无键" % frame)
            if len(fc.keyframe_points) <= 1:
                raise ValueError("至少保留 1 键")
            fc.keyframe_points.remove(hit)
            fc.update()
        except ValueError as e:
            self.report({'ERROR'}, str(e))
            return {'CANCELLED'}
        self.report({'INFO'}, "已删 f%d 键" % frame)
        return {'FINISHED'}


# ---- 面板分区 (由 timl_window_editor._draw_windows_panel 的 CTO 分支调用) ----

def draw_damage_ui(box, context):
    scene = context.scene
    a = context.window_manager.freehk_timl_edit
    box.label(text="当前引用 (帧 = 节点 → 名称 [bank→ATK] 伤害)")
    for line in describe_damage_ref(a, scene):
        box.label(text=line)
    names, mvs = col_names(scene.freehk_timl_col)
    tag = resolve(names, mvs, scene.freehk_timl_node)
    box.label(text="节点 %d → %s" % (scene.freehk_timl_node, tag or "col 未收录"))
    row = box.row(align=True)
    row.prop(scene, "freehk_timl_node", text="节点ID")
    row = box.row(align=True)
    row.prop(scene, "freehk_timl_start", text="帧")
    row.operator("freehk.timl_dmgnode_set", text="设引用")
    row = box.row(align=True)
    row.operator("freehk.timl_dmgnode_del", text="删'帧'处键")
    cbox = box.box()
    cbox.prop(scene, "freehk_timl_col", text="col")
    box.label(text="↑ 即改内存TIML (Ctrl+Z撤销); 导出后才写.lmt")


_classes = [
    FREEHK_OT_timl_dmgnode_set,
    FREEHK_OT_timl_dmgnode_del,
]

_SCENE_PROPS = (
    ("freehk_timl_node", IntProperty(name="伤害引用节点ID", default=89, min=0,
                                     max=65535)),
    ("freehk_timl_col", StringProperty(name="wp03.col (名称/伤害解析)",
                                       default=DEFAULT_COL, subtype='FILE_PATH')),
)


def register():
    for c in _classes:
        bpy.utils.register_class(c)
    for name, prop in _SCENE_PROPS:
        setattr(bpy.types.Scene, name, prop)


def unregister():
    for c in reversed(_classes):
        bpy.utils.unregister_class(c)
    for name, _ in _SCENE_PROPS:
        try:
            delattr(bpy.types.Scene, name)
        except Exception:
            pass


if __name__ == "__main__":
    register()
