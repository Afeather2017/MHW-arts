# -*- coding: utf-8 -*-
"""TIML 屏幕震动编辑器 (PlMotionCommon: mCameraShakeIndex + mTrigger bit10 + mViewParamId)

配方 (2026-10-21/22 定案, user-verified 343/156):
  震动 = ① m252961CD (mCameraShakeIndex) 强度窗: LINEAR guard 键
           (0,0),(s-1,0),(s,档),(e,档),(e+1,0)   档位表 = wpNN_shake.csk 按武器
         ② m99897C43 (mTrigger, bigFlag 双通道) bit10=1024 点火爆窗:
           ix0=(0,0),(s,1024),(s+引信,0), ix1 同帧全零镜像   [触发位=扳机, 索引=强度]
         ③ 可选 mF4D12F36 (mViewParamId) 拉镜头: CONSTANT
           (0,0),(vs-1,0),(vs,id),(ve,id),(ve+1,0),(animLength-1,-1)
  禁忌: bit11=2048 是无敌/no-hit 族, 永远别写; SINT 通道键必须 back=period=0。
用法 (无头):
  bpy.ops.freehk.timl_shake_setup('EXEC_DEFAULT', entry_num=343,
      frame_start=69, frame_end=90, grade=43, view_on=True, view_start=31, view_end=72)
  bpy.ops.freehk.timl_shake_list('EXEC_DEFAULT', entry_num=343)
"""
import bpy
from bpy.props import IntProperty, BoolProperty

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

SHAKE_HASH = 0x252961CD      # mCameraShakeIndex
TRIG_HASH = 0x99897C43       # mTrigger (bigFlag 双通道)
VIEW_HASH = 0xF4D12F36       # mViewParamId
PMC_CLASSID = "m48D6114F"    # nTimelineParam::PlMotionCommon 类型 id
TREE_NAME = "w03_00"
SHAKE_BIT = 1024             # bit10 = 震动点火 (bit11=2048 是无敌族, 禁用)


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


def _entry_pmc(entry_num):
    """BFS 该 entry 的 TIML 链, 返回 (PlMotionCommon action 或 None, animLength).
    同 entry 有多个 PM 块时优先带 mCameraShakeIndex 通道的那个."""
    act, td = _entry_nodes(entry_num)
    if td is None:
        return None, None
    seen, stack = set(), [td]
    first = None
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
                if a is not None and 'PlMotionCommon' in a.name:
                    if any(fc.data_path.endswith("%08X" % SHAKE_HASH) for fc in a.fcurves):
                        return a, td.animLength
                    if first is None:
                        first = a
    return first, td.animLength


def _next_pmc_suffix():
    n = 0
    for a in bpy.data.actions:
        if a.name.startswith("TIML::nTimelineParam::PlMotionCommon."):
            try:
                n = max(n, int(a.name.rsplit('.', 1)[1]))
            except ValueError:
                pass
    return n + 1


def ensure_pmc(entry_num):
    """找到或新建该 entry 的 PlMotionCommon 参数, 返回 (action, animLength, 是否新建)."""
    a, anim_len = _entry_pmc(entry_num)
    if a is not None:
        return a, anim_len, False
    act, td = _entry_nodes(entry_num)
    if td is None:
        raise ValueError("entry %d 没有连 TIML Data Node" % entry_num)
    tree = _tree()
    a = bpy.data.actions.new("TIML::nTimelineParam::PlMotionCommon.%03d"
                             % _next_pmc_suffix())
    a.freehk.starType = 'TIML_Action'
    a.freehk.timelineParam = PMC_CLASSID
    a.freehk.timelineParamRaw = PMC_CLASSID
    a.freehk.timl_reorder = 'BLENDER'
    a.freehk.frameCount = -1
    a.freehk.unkn0 = 0
    a.freehk.fold = 'MAIN'
    a.freehk.resample = False
    node = tree.nodes.new('TIMLActionNode')
    node.input_action = a
    node.location = (td.location.x - 220, td.location.y - 40)
    tree.links.new(node.outputs['TIML Animation'], td.inputs['TIML Animation'])
    return a, td.animLength, True


# ---- 键列写入 (防 remove() 静默失败: 整条 FCurve 重建) ----------------------

def _rebuild(action, tf_hash, ix, keys, interp):
    """keys=[(frame,val)] — 删旧通道重建 (逐键 remove() 会静默失败, 整条删)."""
    path = "FreeHKTiml.m%08X" % tf_hash
    fc = None
    old = action.fcurves.find(path, index=ix)
    if old is not None:
        try:
            action.fcurves.remove(old)   # 整条删, 成功后 old 已死不可再用
        except RuntimeError:
            fc = old                     # 删不掉则原地清键
            for k in list(fc.keyframe_points):
                try:
                    fc.keyframe_points.remove(k)
                except RuntimeError:
                    pass
    if fc is None:
        fc = action.fcurves.new(data_path=path, index=ix)
    for f, v in keys:
        k = fc.keyframe_points.insert(f, v)
        k.interpolation = interp
        k.back = 0.0            # SINT 通道 (含负值) 必须 back=period=0
        k.period = 0.0
    fc.keyframe_points.sort()
    fc.update()
    return fc


def _chan(action, tf_hash):
    return action.fcurves.find("FreeHKTiml.m%08X" % tf_hash, index=0)


# ---- 配方 ------------------------------------------------------------------

def set_shake_window(action, start, end, grade):
    """mCameraShakeIndex 强度窗 (LINEAR + guard 键)."""
    return _rebuild(action, SHAKE_HASH, 0,
                    [(0.0, 0), (start - 1.0, 0), (float(start), grade),
                     (float(end), grade), (end + 1.0, 0)], 'LINEAR')


def set_trigger(action, start, burst=5):
    """mTrigger bit10 点火爆窗 [start, start+burst) — bigFlag 双通道镜像."""
    frames = [0.0, float(start), float(start + burst)]
    r0 = _rebuild(action, TRIG_HASH, 0,
                  list(zip(frames, [0, SHAKE_BIT, 0])), 'SINE')
    r1 = _rebuild(action, TRIG_HASH, 1,
                  list(zip(frames, [0, 0, 0])), 'SINE')
    return r0, r1


def set_view_window(action, start, end, view_id, anim_length):
    """mViewParamId 拉镜头窗 + 结尾 -1 标记 (CONSTANT)."""
    return _rebuild(action, VIEW_HASH, 0,
                    [(0.0, 0), (start - 1.0, 0), (float(start), view_id),
                     (float(end), view_id), (end + 1.0, 0),
                     (float(max(anim_length - 1, end + 2)), -1)], 'CONSTANT')


def read_state(action):
    """当前状态 → dict (shake 键 / trigger 键 / view 键)."""
    out = {}
    for name, h in (("shake", SHAKE_HASH), ("trigger", TRIG_HASH), ("view", VIEW_HASH)):
        fc = _chan(action, h)
        out[name] = None if fc is None else [(int(k.co[0]), int(k.co[1])) for k in fc.keyframe_points]
    return out


def configure(entry_num, start, end, grade=43, burst=5,
              view_on=False, view_start=0, view_end=0, view_id=14):
    """一键配方. view_start/end=0 表示跟随震动窗. 返回描述字符串."""
    if end <= start:
        raise ValueError("止帧需大于起帧")
    pmc, anim_len, created = ensure_pmc(entry_num)
    set_shake_window(pmc, start, end, grade)
    set_trigger(pmc, start, burst)
    vmsg = "无"
    if view_on:
        vs, ve = (start, end) if (view_start == 0 and view_end == 0) else (view_start, view_end)
        if ve <= vs:
            raise ValueError("拉镜头止帧需大于起帧")
        set_view_window(pmc, vs, ve, view_id, anim_len or (end + 32))
        vmsg = "f%d-%d id%d" % (vs, ve, view_id)
    st = read_state(pmc)
    return ("entry%d: PMC=%s (%s)\n  shake=%s\n  trigger=%s\n  view=%s" %
            (entry_num, pmv_name(pmc), "新建" if created else "已有",
             st["shake"], st["trigger"], vmsg))


def pmv_name(action):
    try:
        return action.name.split('.')[-1]
    except Exception:
        return str(action)


# ---- 操作符 ----------------------------------------------------------------

class _ShakeOp:
    bl_options = {"REGISTER", "UNDO"}
    entry_num: IntProperty(name="Entry", description="LMT entry 号", default=343, min=1)


class FREEHK_OT_timl_shake_setup(_ShakeOp, bpy.types.Operator):
    bl_idname = "freehk.timl_shake_setup"
    bl_label = "一键配置震动"
    bl_description = ("配方: CameraShakeIndex 强度窗 + mTrigger bit10 点火爆窗\n"
                      "(档位表 = wpNN_shake.csk 按武器; 343/156 已验证)")
    frame_start: IntProperty(name="起帧", default=69, min=1)
    frame_end: IntProperty(name="止帧", default=90, min=2)
    grade: IntProperty(name="档位", default=43, min=0, max=255)
    burst: IntProperty(name="点火帧数", default=5, min=1)
    view_on: BoolProperty(name="拉镜头", default=False)
    view_start: IntProperty(name="镜头起", default=0, min=0,
                            description="0 = 跟随震动窗")
    view_end: IntProperty(name="镜头止", default=0, min=0,
                          description="0 = 跟随震动窗")
    view_id: IntProperty(name="镜头id", default=14, min=0)

    def execute(self, context):
        try:
            msg = configure(self.entry_num, self.frame_start, self.frame_end,
                            self.grade, self.burst,
                            self.view_on, self.view_start, self.view_end, self.view_id)
        except ValueError as e:
            self.report({'ERROR'}, str(e))
            return {'CANCELLED'}
        for line in msg.split("\n"):
            print("[timl-shake]", line)
        self.report({'INFO'}, msg.replace("\n", " | "))
        return {'FINISHED'}


class FREEHK_OT_timl_shake_list(_ShakeOp, bpy.types.Operator):
    bl_idname = "freehk.timl_shake_list"
    bl_label = "列出震动状态"
    bl_description = "PlMotionCommon 的 shake/trigger/view 键 → 控制台 + 信息区"

    def execute(self, context):
        act, _td = _entry_nodes(self.entry_num)
        pmc, anim_len = _entry_pmc(self.entry_num)
        lines = ["entry%d (anim %s f):" % (self.entry_num, anim_len)]
        lines.append("  动作: %s" % (act.name if act else "无"))
        if pmc is None:
            lines.append("  PlMotionCommon: 无 (先一键配置)")
        else:
            st = read_state(pmc)
            lines.append("  PMC: %s" % pmc.name.split('.')[-1])
            lines.append("  shake:   %s" % (st["shake"] or "无"))
            lines.append("  trigger: %s" % (st["trigger"] or "无"))
            lines.append("  view:    %s" % (st["view"] or "无"))
        for line in lines:
            print("[timl-shake]", line)
        self.report({'INFO'}, "%d 行, 详见系统控制台" % len(lines))
        return {'FINISHED'}


# ---- 面板 ------------------------------------------------------------------

def _draw_shake_panel(layout, context):
    sc = context.scene
    entry = sc.freehk_shake_entry
    layout.prop(sc, "freehk_shake_entry", text="Entry")
    row = layout.row(align=True)
    op = row.operator("freehk.timl_shake_setup", text="一键配置")
    op.entry_num = entry
    op.frame_start = sc.freehk_shake_start
    op.frame_end = sc.freehk_shake_end
    op.grade = sc.freehk_shake_grade
    op.burst = sc.freehk_shake_burst
    op.view_on = sc.freehk_shake_view_on
    op.view_start = sc.freehk_shake_view_start
    op.view_end = sc.freehk_shake_view_end
    op.view_id = sc.freehk_shake_view_id
    row.operator("freehk.timl_shake_list").entry_num = entry

    grid = layout.column(align=True)
    row = grid.row(align=True)
    row.prop(sc, "freehk_shake_start", text="起")
    row.prop(sc, "freehk_shake_end", text="止")
    row = grid.row(align=True)
    row.prop(sc, "freehk_shake_grade", text="档位")
    row.prop(sc, "freehk_shake_burst", text="引信")

    box = layout.box()
    box.prop(sc, "freehk_shake_view_on", text="拉镜头 (ViewParam)")
    if sc.freehk_shake_view_on:
        row = box.row(align=True)
        row.prop(sc, "freehk_shake_view_start", text="起")
        row.prop(sc, "freehk_shake_view_end", text="止")
        box.prop(sc, "freehk_shake_view_id", text="id")

    pmc, anim_len = _entry_pmc(entry)
    box = layout.box()
    if pmc is None:
        box.label(text="PlMotionCommon: 无")
    else:
        st = read_state(pmc)
        box.label(text="PMC.%s (anim %sf)" % (pmc.name.split('.')[-1], anim_len))
        sh = st["shake"]
        if sh:
            window = [kv for kv in sh if kv[1] > 0]
            if window:
                box.label(text="震动: f%d-%d 档%d" % (window[0][0], window[-1][0],
                                                     window[0][1]))
            else:
                box.label(text="震动: 无非零窗")
        else:
            box.label(text="震动: 无")
        tg = st["trigger"]
        fires = [kv for kv in (tg or []) if kv[1] > 0]
        box.label(text="点火: %s" % (("f%s bit%d" % (fires[0][0], fires[0][1])
                                      if fires else "无")))
        vw = st["view"]
        vfire = [kv for kv in (vw or []) if kv[1] > 0]
        box.label(text="镜头: %s" % (("f%s id%d" % (vfire[0][0], vfire[0][1])
                                      if vfire else ("仅-1标记" if vw else "无"))))
    layout.label(text="改内存, 导出后才写 .lmt")


class FREEHK_PT_timl_shake_3d(bpy.types.Panel):
    bl_label = "TIML 震动"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = "FreeHK"

    def draw(self, context):
        _draw_shake_panel(self.layout, context)


class FREEHK_PT_timl_shake_dopesheet(bpy.types.Panel):
    bl_label = "TIML 震动"
    bl_space_type = 'DOPESHEET_EDITOR'
    bl_region_type = 'UI'
    bl_category = "FreeHK"

    def draw(self, context):
        _draw_shake_panel(self.layout, context)


_classes = [
    FREEHK_OT_timl_shake_setup,
    FREEHK_OT_timl_shake_list,
    FREEHK_PT_timl_shake_3d,
    FREEHK_PT_timl_shake_dopesheet,
]

_SCENE_PROPS = (
    ("freehk_shake_entry", IntProperty(name="Entry", default=343, min=1)),
    ("freehk_shake_start", IntProperty(name="起帧", default=69, min=1)),
    ("freehk_shake_end", IntProperty(name="止帧", default=90, min=2)),
    ("freehk_shake_grade", IntProperty(name="档位", default=43, min=0, max=255)),
    ("freehk_shake_burst", IntProperty(name="点火帧数", default=5, min=1)),
    ("freehk_shake_view_on", BoolProperty(name="拉镜头", default=False)),
    ("freehk_shake_view_start", IntProperty(name="镜头起", default=0, min=0)),
    ("freehk_shake_view_end", IntProperty(name="镜头止", default=0, min=0)),
    ("freehk_shake_view_id", IntProperty(name="镜头id", default=14, min=0)),
)


def register():
    for cls in _classes:
        bpy.utils.register_class(cls)
    for prop, val in _SCENE_PROPS:
        setattr(bpy.types.Scene, prop, val)


def unregister():
    for prop, _val in _SCENE_PROPS:
        try:
            delattr(bpy.types.Scene, prop)
        except Exception:
            pass
    for cls in reversed(_classes):
        try:
            bpy.utils.unregister_class(cls)
        except Exception:
            pass
