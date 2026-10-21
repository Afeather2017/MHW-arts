# -*- coding: utf-8 -*-
"""TIML 拖尾编辑器 — FreeKinetics 扩展 (nTimelineParam::PlMotionVisual).

武器拖尾机制定案 (2026-10-21, 全链 user-verified, 详见 MHW-arts/AGENTS.md):
  拖尾 = 三条件: ①动作含 MhBone_254/252 变换轨 (恒等单键即可, 数值无关)
               ②TIML PlMotionVisual mFlag bit3 (值 8) 窗覆盖想要的帧段
               ③weapon+0x2398 武装 (DLL gauge-rules 'trail <set> <id>' / 原生气刃付费)
  消费者: master 更新器测 [*(player+0xE490)+8] bit3 → 0x140AE0FF0 生成特效 0x9C4。
  bit3/4/5 = PlMotionVisual 三条 WeaponEfcType 槽激活位; 本编辑器管理槽0 (拖尾)。

本编辑器 (entry 驱动, 与"TIML 窗口编辑"的 action 驱动互补):
  1. 选 entry 号 → 一键配置: 找到/新建该 entry 的 PlMotionVisual 参数
     (新建含 freehk.starType='TIML_Action' + timelineParam=m46F3E901,
      缺它导出器按 A_TIML_INVALID_ACTION_TYPE 静默剔除!) + 补动作 254/252 恒等轨。
  2. 段增删: bit3 多段窗 (双通道 ix0=值/ix1=高位 同步, CONSTANT), 其他位 (2048 等) 保留。
  3. 提示行: DLL 侧 gauge-rules.txt 需加 'trail 3 <请求id>' (本工具不改 DLL 配置)。

用法 (GUI): N 面板 "FreeHK" → TIML 拖尾。
用法 (无头): bpy.ops.freehk.timl_trail_setup(entry_num=345)
            bpy.ops.freehk.timl_trail_seg_add(entry_num=345, frame_start=24, frame_end=38)
            bpy.ops.freehk.timl_trail_seg_del(entry_num=345, seg_index=0)
            bpy.ops.freehk.timl_trail_list(entry_num=345)
"""
import bpy
from bpy.props import IntProperty, StringProperty

try:
    from .timl_window_editor import get_channels
except ImportError:  # 脚本直跑
    def get_channels(action, tf_hash, create=False):
        path = "FreeHKTiml.m%08X" % tf_hash
        out = {}
        for ix in (0, 1):
            f = action.fcurves.find(path, index=ix)
            if f is None and create:
                f = action.fcurves.new(data_path=path, index=ix)
            out[ix] = f
        return out

MFLAG = 0x08FD20A6          # PlMotionVisual 公共 bigflag (bit3=槽0=拖尾)
PMV_CLASSID = "m46F3E901"   # nTimelineParam::PlMotionVisual 类型 id
IK_BONES = ("MhBone_254", "MhBone_252")
TREE_NAME = "w03_00"


# ---- 树/entry 遍历 ---------------------------------------------------------

def _tree():
    return bpy.data.node_groups.get(TREE_NAME)


def _entry_nodes(entry_num):
    """(LMTActionNode 的 action, TIMLDataNode) — 找不到时对应项为 None."""
    tree = _tree()
    if tree is None:
        return None, None
    act, td = None, None
    for n in tree.nodes:
        if n.bl_idname != 'LMTEntryNode' or n.get('entryNum') != entry_num:
            continue
        for inp in n.inputs:
            for l in inp.links:
                if l.from_node.bl_idname == 'LMTActionNode':
                    act = l.from_node.input_action
                elif l.from_node.bl_idname == 'TIMLDataNode':
                    td = l.from_node
    return act, td


def _entry_pmv(entry_num):
    """BFS 该 entry 的 TIML 链, 返回 PlMotionVisual action 或 None."""
    act, td = _entry_nodes(entry_num)
    if td is None:
        return None
    seen, stack = set(), [td]
    while stack:
        cur = stack.pop()
        for i in cur.inputs:
            for l in i.links:
                m = l.from_node
                if m.name in seen:
                    continue
                seen.add(m.name)
                stack.append(m)
                a = getattr(m, 'input_action', None)
                if a is not None and 'PlMotionVisual' in a.name:
                    return a
    return None


# ---- 段解析/写入 (bit3 位平面, 保留其他位) ---------------------------------

def pmv_segments(action):
    """bit3 连续段 [(起帧, 止帧 exclusive), ...] (从 arr0 键值 & 8 解出)."""
    fc = action.fcurves.find("FreeHKTiml.m%08X" % MFLAG, index=0)
    if fc is None or not len(fc.keyframe_points):
        return []
    keys = sorted((int(k.co[0]), int(k.co[1])) for k in fc.keyframe_points)
    segs, cur = [], None
    for i, (f, v) in enumerate(keys):
        on = bool(v & 8)
        if on and cur is None:
            cur = f
        if not on and cur is not None:
            segs.append((cur, f))
            cur = None
    if cur is not None:
        segs.append((cur, keys[-1][0]))
    return segs


def set_bit3_windows(action, segments):
    """重写 bit3 位平面为 segments, 其他位 (2048 等) 按原键保留."""
    ch = get_channels(action, MFLAG, create=True)
    fc0, fc1 = ch[0], ch[1]
    old0 = sorted((int(k.co[0]), int(k.co[1])) for k in fc0.keyframe_points)
    old1 = sorted((int(k.co[0]), int(k.co[1])) for k in fc1.keyframe_points)
    old1d = dict(old1)

    def base(f):  # 该帧的非-bit3 值 (取此前最近键)
        v = 0
        for kf, kv in old0:
            if kf <= f:
                v = kv & ~8
        return v

    frames = sorted({f for f, _ in old0} |
                    {s for s, _ in segments} | {e for _, e in segments} |
                    {f for f, _ in old1})
    out = []
    for f in frames:
        on = any(s <= f < e for s, e in segments)
        v = base(f) | (8 if on else 0)
        if out and out[-1][1] == v:
            continue  # 同值相邻键去重
        out.append((f, v))
    while len(fc0.keyframe_points):
        fc0.keyframe_points.remove(fc0.keyframe_points[0])
    for f, v in out:
        k = fc0.keyframe_points.insert(float(f), float(v))
        k.interpolation = 'CONSTANT'
    fc0.update()
    while len(fc1.keyframe_points):
        fc1.keyframe_points.remove(fc1.keyframe_points[0])
    for f in frames:
        k = fc1.keyframe_points.insert(float(f), float(old1d.get(f, 0)))
        k.interpolation = 'CONSTANT'
    fc1.update()


# ---- 一键配置 --------------------------------------------------------------

def _next_pmv_suffix():
    n = 0
    for a in bpy.data.actions:
        if a.name.startswith("TIML::nTimelineParam::PlMotionVisual."):
            try:
                n = max(n, int(a.name.rsplit('.', 1)[1]))
            except ValueError:
                pass
    return n + 1


def ensure_pmv(entry_num):
    """找到或新建该 entry 的 PlMotionVisual 参数, 返回 (action, 是否新建)."""
    a = _entry_pmv(entry_num)
    if a is not None:
        return a, False
    _, td = _entry_nodes(entry_num)
    if td is None:
        raise ValueError("entry %d 没有连 TIML Data Node" % entry_num)
    tree = _tree()
    a = bpy.data.actions.new("TIML::nTimelineParam::PlMotionVisual.%03d"
                             % _next_pmv_suffix())
    a.freehk.starType = 'TIML_Action'
    a.freehk.timelineParam = PMV_CLASSID
    a.freehk.timelineParamRaw = PMV_CLASSID
    a.freehk.timl_reorder = 'BLENDER'
    a.freehk.frameCount = -1
    a.freehk.unkn0 = 0
    a.freehk.fold = 'MAIN'
    a.freehk.resample = False
    ch = get_channels(a, MFLAG, create=True)
    for ix in (0, 1):
        k = ch[ix].keyframe_points.insert(0.0, 0.0)
        k.interpolation = 'CONSTANT'
        ch[ix].update()
    node = tree.nodes.new('TIMLActionNode')
    node.input_action = a
    node.location = (td.location.x - 220, td.location.y - 40)
    tree.links.new(node.outputs['TIML Animation'], td.inputs['TIML Animation'])
    return a, True


def ensure_ik_tracks(action):
    """动作缺 254/252 旋转轨时补恒等单键 (拖尾绑定需要轨存在, 数值无关)."""
    made = []
    for bone in IK_BONES:
        dp = 'pose.bones["%s"].rotation_quaternion' % bone
        if any(fc.data_path == dp for fc in action.fcurves):
            continue
        for ai in range(4):
            fc = action.fcurves.new(data_path=dp, index=ai, action_group=bone)
            k = fc.keyframe_points.insert(0.0, 1.0 if ai == 0 else 0.0)
            k.interpolation = 'CONSTANT'
            fc.update()
        made.append(bone)
    return made


# ---- 操作符 ----------------------------------------------------------------

class _TrailOp:
    bl_options = {"REGISTER", "UNDO"}
    entry_num: IntProperty(name="Entry", description="LMT entry 号", default=345, min=1)


class FREEHK_OT_timl_trail_setup(_TrailOp, bpy.types.Operator):
    bl_idname = "freehk.timl_trail_setup"
    bl_label = "一键配置拖尾"
    bl_description = ("找到/新建该 entry 的 PlMotionVisual 参数 + 补动作 254/252 恒等轨\n"
                      "(DLL 侧另需 gauge-rules.txt 'trail 3 <请求id>')")

    def execute(self, context):
        act, _td = _entry_nodes(self.entry_num)
        if act is None:
            self.report({'ERROR'}, "entry %d 没有连动作 (LMT Action Node)" % self.entry_num)
            return {'CANCELLED'}
        try:
            pmv, created = ensure_pmv(self.entry_num)
        except ValueError as e:
            self.report({'ERROR'}, str(e))
            return {'CANCELLED'}
        made = ensure_ik_tracks(act)
        segs = pmv_segments(pmv)
        msg = "entry%d: PMV=%s (%s), 254/252补轨=%s, bit3段=%s" % (
            self.entry_num, pmv.name, "新建" if created else "已有",
            made or "无缺", segs or "无")
        print("[timl-trail]", msg)
        self.report({'INFO'}, msg)
        return {'FINISHED'}


class FREEHK_OT_timl_trail_seg_add(_TrailOp, bpy.types.Operator):
    bl_idname = "freehk.timl_trail_seg_add"
    bl_label = "加拖尾段"
    bl_description = "bit3 (8) 窗加一段 [起, 止) — 覆盖该帧段即出拖尾"
    frame_start: IntProperty(name="起帧", default=24, min=0)
    frame_end: IntProperty(name="止帧", default=38, min=1)

    def execute(self, context):
        pmv = _entry_pmv(self.entry_num)
        if pmv is None:
            self.report({'ERROR'}, "entry %d 无 PlMotionVisual 参数 (先一键配置)" % self.entry_num)
            return {'CANCELLED'}
        if self.frame_end <= self.frame_start:
            self.report({'ERROR'}, "止帧需大于起帧")
            return {'CANCELLED'}
        segs = pmv_segments(pmv) + [(self.frame_start, self.frame_end)]
        segs.sort()
        set_bit3_windows(pmv, segs)
        msg = "entry%d: 段=%s" % (self.entry_num, pmv_segments(pmv))
        print("[timl-trail]", msg)
        self.report({'INFO'}, msg)
        return {'FINISHED'}


class FREEHK_OT_timl_trail_seg_del(_TrailOp, bpy.types.Operator):
    bl_idname = "freehk.timl_trail_seg_del"
    bl_label = "删拖尾段"
    bl_description = "按序号删一段 (0 起, 见面板段列表)"
    seg_index: IntProperty(name="段序号", default=0, min=0)

    def execute(self, context):
        pmv = _entry_pmv(self.entry_num)
        if pmv is None:
            self.report({'ERROR'}, "entry %d 无 PlMotionVisual 参数" % self.entry_num)
            return {'CANCELLED'}
        segs = pmv_segments(pmv)
        if self.seg_index >= len(segs):
            self.report({'ERROR'}, "段 %d 不存在 (共 %d 段)" % (self.seg_index, len(segs)))
            return {'CANCELLED'}
        segs.pop(self.seg_index)
        set_bit3_windows(pmv, segs)
        msg = "entry%d: 删段%d, 段=%s" % (self.entry_num, self.seg_index, pmv_segments(pmv))
        print("[timl-trail]", msg)
        self.report({'INFO'}, msg)
        return {'FINISHED'}


class FREEHK_OT_timl_trail_list(_TrailOp, bpy.types.Operator):
    bl_idname = "freehk.timl_trail_list"
    bl_label = "列出拖尾状态"
    bl_description = "PMV 参数 / bit3 段 / 254-252 轨 → 控制台 + 信息区"

    def execute(self, context):
        act, _td = _entry_nodes(self.entry_num)
        pmv = _entry_pmv(self.entry_num)
        lines = ["entry%d:" % self.entry_num]
        lines.append("  动作: %s" % (act.name if act else "无"))
        if act is not None:
            for bone in IK_BONES:
                has = any(bone in fc.data_path for fc in act.fcurves)
                lines.append("  %s 轨: %s" % (bone, "有" if has else "缺 (拖尾需存在)"))
        lines.append("  PMV: %s" % (pmv.name if pmv else "无 (先一键配置)"))
        if pmv is not None:
            lines.append("  bit3 段: %s" % (pmv_segments(pmv) or "无"))
        for line in lines:
            print("[timl-trail]", line)
        self.report({'INFO'}, "%d 行, 详见系统控制台" % len(lines))
        return {'FINISHED'}


# ---- 面板 ------------------------------------------------------------------

def _draw_trail_panel(layout, context):
    entry = context.scene.freehk_trail_entry
    layout.prop(context.scene, "freehk_trail_entry", text="Entry")
    row = layout.row(align=True)
    row.operator("freehk.timl_trail_setup", text="一键配置")
    row.operator("freehk.timl_trail_list", text="状态")
    act, _td = _entry_nodes(entry)
    pmv = _entry_pmv(entry)
    box = layout.box()
    if act is not None:
        ik_ok = all(any(b in fc.data_path for fc in act.fcurves) for b in IK_BONES)
        box.label(text="动作: %s" % act.name)
        box.label(text="254/252 轨: %s" % ("齐" if ik_ok else "缺! (一键配置补)"))
    else:
        box.label(text="entry 未连动作")
    if pmv is not None:
        box.label(text="PMV: %s" % pmv.name.split('.')[-1])
        segs = pmv_segments(pmv)
        if segs:
            for i, (s, e) in enumerate(segs):
                box.label(text="  段%d: f%d-%d" % (i, s, e))
        else:
            box.label(text="  无 bit3 段")
        srow = box.row(align=True)
        srow.prop(context.scene, "freehk_trail_start", text="起")
        srow.prop(context.scene, "freehk_trail_end", text="止")
        box.operator("freehk.timl_trail_seg_add")
        drow = box.row(align=True)
        drow.prop(context.scene, "freehk_trail_seg", text="段")
        drow.operator("freehk.timl_trail_seg_del")
    else:
        box.label(text="PMV: 无")
    layout.label(text="DLL: gauge-rules.txt 'trail 3 <请求id>'")
    layout.label(text="↑ 改内存, 导出后才写 .lmt")


class FREEHK_PT_timl_trail_3d(bpy.types.Panel):
    bl_label = "TIML 拖尾"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = "FreeHK"

    def draw(self, context):
        _draw_trail_panel(self.layout, context)


class FREEHK_PT_timl_trail_dopesheet(bpy.types.Panel):
    bl_label = "TIML 拖尾"
    bl_space_type = 'DOPESHEET_EDITOR'
    bl_region_type = 'UI'
    bl_category = "FreeHK"

    def draw(self, context):
        _draw_trail_panel(self.layout, context)


_classes = [
    FREEHK_OT_timl_trail_setup,
    FREEHK_OT_timl_trail_seg_add,
    FREEHK_OT_timl_trail_seg_del,
    FREEHK_OT_timl_trail_list,
    FREEHK_PT_timl_trail_3d,
    FREEHK_PT_timl_trail_dopesheet,
]

_SCENE_PROPS = (
    ("freehk_trail_entry", IntProperty(name="Entry", default=345, min=1)),
    ("freehk_trail_start", IntProperty(name="起帧", default=24, min=0)),
    ("freehk_trail_end", IntProperty(name="止帧", default=38, min=1)),
    ("freehk_trail_seg", IntProperty(name="段序号", default=0, min=0)),
)


def register():
    for cls in _classes:
        bpy.utils.register_class(cls)
    for prop, val in _SCENE_PROPS:
        setattr(bpy.types.Scene, prop, val)


def unregister():
    for prop, _val in _SCENE_PROPS:
        delattr(bpy.types.Scene, prop)
    for cls in reversed(_classes):
        bpy.utils.unregister_class(cls)
