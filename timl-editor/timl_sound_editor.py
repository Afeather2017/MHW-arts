# -*- coding: utf-8 -*-
"""TIML 声音/事件编辑器 — FreeKinetics 扩展 (nWwiseTimeline 声音类别).

2026-10-17 实证定案 (E345 user A/B 三轮, 全文见 MHW-arts/timl-editor/TIML_PARAM_REGISTRY.md):
  触发语义  nWwiseTimeline EventGroup 的 mTrigger bigflag (m99897C43) 键 = 触发事件:
            键帧 = 触发帧, 键值 = ReqNo 行位掩码 (bit0=Row A .. bit5=F);
            f0=0 是基线键 (勿删). 已验证操作: 删触发键 = 删那次发声 (EG02 f72 案例).
  ReqNo 行  每行一条 f0 常数键 (dataType=1); 行序 A..F = hash 权威序 (FHK TIMLDatatypes):
            A=0xE64D793E B=0x7F442884 C=0x08431812 D=0x96278DB1 E=0xE120BD27 F=0x7829EC9D.
  类别角色  EventGroup02 = 喊叫/气合声载体 (user A/B 确证) /
            EventGroup06 = 金属摩擦 (user 辨认; ⚠触发语义未 isolated —
            345 收 3 触发进窗实验 = 全灭, 慎删慎挪, 先单触发 A/B) /
            GameParameter = 刀风声 = 连续参数轨 (GameParam A..H 强度曲线被音频逐帧
            采样, 无 mTrigger, 改时机 = 编辑曲线键) /
            EffectParameter1 = 踏步 FX 自带声 (KeyFrameNN 使能窗 dataType4 + WorkNoNN FX id) /
            EventGroup01 角色未 isolated (345 上 EG02 在场时改它无听感).
  编辑铁律  插值仅限 CONSTANT/LINEAR/QUAD/CUBIC/QUART/EXPO/SINE (exporter
            interpolationMapping; 默认 BEZIER 会 ValueError 崩导出);
            bigflag 轨用 SINE (vanilla 惯例), 数值曲线 LINEAR;
            一律整数帧; bigflag 双通道 (ix0=低16/ix1=高16) 由本模块自动同步.

2026-10-24 实证定案 (156/343 喊声落地: 二分实验 + user 局内听音辨认):
  语音请求号 1309=爆发"哈" (307 小居合 EG02 行A=1309 双触发 f7/f31 = 听到的两声) /
            1409=前摇较劲"哦" (拖重物声) / 1119=中段喊 / 1019=挥砍喊 (345 时代结论未重验).
            EG02 在自定义槽位完全可用 (156/343 已落地验证).
  ⚠ 行序铁律 ReqNo 行 fcurve 必须全部位于 mTrigger 之前: 导出二进制轨序 = blend
            fcurve 插入序, 位掩码按轨序取行; 行插到 mTrigger 后面 → 掩码选中触发轨
            本身 → 无声 (156 首版静默案例). set_row 后补行已自动规范化;
            旧事件用 freehk.timl_snd_canon 一键修序.

用法 (GUI): N 面板 "FreeHK" → TIML 声音/事件 (与 TIML 窗口编辑共用 action 选择器).
用法 (无头): bpy.ops.freehk.timl_snd_list(action_name="TIML::...")
            bpy.ops.freehk.timl_snd_trig_add(action_name=..., frame=40, value=2)
            bpy.ops.freehk.timl_snd_trig_del(action_name=..., frame=72)
            bpy.ops.freehk.timl_snd_trig_move(action_name=..., frame=72, to_frame=40)
            bpy.ops.freehk.timl_snd_row_set(action_name=..., row=1, value=1019)
            bpy.ops.freehk.timl_snd_canon(action_name=...)                    # 修行序
            bpy.ops.freehk.timl_snd_voice_add(entry=156, frame=4, voice_id=1309)
"""
import bpy
from bpy.props import IntProperty, StringProperty

try:
    from .timl_window_editor import (get_channels, insert_key, holding_params,
                                     TRANSITION_TRIGGER)
    MTRIGGER = 0x99897C43
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

    def insert_key(fcurve, frame, value, interp, back=1, period=0):
        kf = fcurve.keyframe_points.insert(int(frame), int(value))
        kf.interpolation = interp
        kf.back = back
        kf.period = period
        kf.co = (int(frame), int(value))
        return kf

    def holding_params(fcurve, frame, default_interp, default_back=1):
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
            return default_interp, default_back, 0
        return best.interpolation, int(best.back), int(best.period)

    MTRIGGER = 0x99897C43
    TRANSITION_TRIGGER = 'SINE'

# ReqNo 行: hash 权威序 A..F (research/MHW-Free-HyperKinetics/struct/TIMLDatatypes.txt)
REQNO_ROWS = [
    (0xE64D793E, "A"), (0x7F442884, "B"), (0x08431812, "C"),
    (0x96278DB1, "D"), (0xE120BD27, "E"), (0x7829EC9D, "F"),
]
VALID_INTERP = ("CONSTANT", "LINEAR", "QUAD", "CUBIC", "QUART", "EXPO", "SINE")

# 类别角色 (2026-10-17 user 辨认/A/B; 短行, 面板逐行显示)
ROLE_HINTS = [
    ("EventGroup01", ("角色未 isolated", "345: EG02 在场时改它无听感 (111-145 家族)")),
    ("EventGroup02", ("喊叫/气合声载体 [user A/B 确证]",
                      "删触发=删那次喊声; 语音号: 1309=爆发哈 1409=前摇哦 1119=中段喊 1019=挥砍喊")),
    ("EventGroup03", ("杂项 SE", "(543 族)")),
    ("EventGroup05", ("杂项 SE", "(902 族)")),
    ("EventGroup06", ("金属摩擦声 [user 辨认]", "⚠触发语义未 isolated: 收窗实验=全灭, 慎删挪")),
    ("EventGroup07", ("落地/摩擦 SE 族 (56xx)", "")),
    ("GameParameter", ("刀风声 = 连续参数轨 [user 辨认]", "GameParam A/B 曲线被逐帧采样; 改时机=编辑曲线键(PO->曲线)")),
    ("EffectParameter1", ("踏步 FX 自带声 [user 辨认]", "KeyFrameNN=使能窗, WorkNoNN=FX id")),
    ("EventCollision", ("攻击窗 (非声音)", "")),
]

# 喊声语音请求号 → 听感 (2026-10-24 user 局内听音辨认; 307 双触发 = 1309 两连声)
VOICE_ID_HINTS = {
    1309: "爆发哈",
    1409: "前摇哦",
    1119: "中段喊",
    1019: "挥砍喊",
}


def voice_label(val):
    if val is None:
        return ""
    return VOICE_ID_HINTS.get(int(val), "")


def _fmt_row(letter, val):
    if val is None:
        return "%s=-" % letter
    lab = voice_label(val)
    return "%s=%s%s" % (letter, val, ("·" + lab) if lab else "")

TRANSITION_LINEAR = 'LINEAR'


# ---- 基础访问 --------------------------------------------------------------

def timl_actions():
    return [a for a in bpy.data.actions
            if getattr(a.freehk, "starType", "") == "TIML_Action"]


def role_hints(action):
    for key, hint in ROLE_HINTS:
        if key in action.name:
            return hint
    return (None, None)


def snd_rows(action):
    """[(行字母, fcurve 或 None, 值或 None)] — 值 = f0 键整值 (缺 f0 键给 None)."""
    out = []
    for rhash, letter in REQNO_ROWS:
        f = action.fcurves.find("FreeHKTiml.m%08X" % rhash, index=0)
        val = None
        if f is not None and len(f.keyframe_points):
            k0 = min(f.keyframe_points, key=lambda k: k.co[0])
            val = int(k0.co[1])
        out.append((letter, f, val))
    return out


def trigger_events(action):
    """mTrigger 双通道按帧合并 (parseBigFlag 语义: 缺键持前值) → [(帧, u32值)]."""
    chans = get_channels(action, MTRIGGER)
    frames = set()
    carr = {0: 0, 1: 0}
    for ix in (0, 1):
        fc = chans.get(ix)
        if fc is not None:
            for kf in fc.keyframe_points:
                frames.add(int(kf.co[0]))
    out = []
    for t in sorted(frames):
        val = 0
        for ix in (0, 1):
            fc = chans.get(ix)
            if fc is None:
                continue
            for kf in fc.keyframe_points:
                if int(kf.co[0]) <= t:
                    carr[ix] = int(kf.co[1])
                else:
                    break
            val |= (carr[ix] & 0xFFFF) << (16 * ix)
        out.append((t, val))
    return out


def decode_mask(mask, rows):
    """掩码 → 'B(1409·前摇哦)+C(135)'; 未知行给 '行N'; 0 → '空触发'."""
    if mask == 0:
        return "空触发(值0)"
    parts = []
    for i, (letter, _f, val) in enumerate(rows):
        if mask & (1 << i):
            s = "-" if val is None else str(val)
            lab = voice_label(val)
            if lab:
                s += "·" + lab
            parts.append("%s(%s)" % (letter, s))
    return "+".join(parts) if parts else "行%d(超出行表)" % mask


# ---- 编辑原语 (整数帧; 双通道自动同步; 插值校验) ----------------------------

def _check_interp(interp):
    if interp not in VALID_INTERP:
        raise ValueError("插值 %s 不被导出器支持, 可用: %s" % (interp, ",".join(VALID_INTERP)))


def set_trigger(action, frame, value, interp=TRANSITION_TRIGGER):
    """加/改触发: bigflag 双通道 f=frame 键 = value (ix0 低16, ix1 高16)."""
    _check_interp(interp)
    frame, value = int(frame), int(value) & 0xFFFFFFFF
    if frame < 0:
        raise ValueError("帧需 >= 0")
    chans = get_channels(action, MTRIGGER, create=True)
    for ix in (0, 1):
        fc = chans[ix]
        part = (value >> (16 * ix)) & 0xFFFF
        hit = None
        for kf in fc.keyframe_points:
            if int(kf.co[0]) == int(frame):
                hit = kf
                break
        if hit is not None:
            hit.co = (int(frame), part)
            hit.interpolation = interp
        else:
            di, db, dp = holding_params(fc, frame, interp, 0)
            insert_key(fc, frame, part, interp if interp else di, db, dp)
        fc.keyframe_points.sort()
        fc.update()


def del_trigger(action, frame):
    """删触发: 移除双通道上 f=frame 的键 (f0 基线键也允许删, 自担风险)."""
    frame = int(frame)
    chans = get_channels(action, MTRIGGER)
    removed = 0
    for ix in (0, 1):
        fc = chans.get(ix)
        if fc is None:
            continue
        for kf in list(fc.keyframe_points):
            if int(kf.co[0]) == frame:
                fc.keyframe_points.remove(kf)
                removed += 1
        fc.keyframe_points.sort()
        fc.update()
    return removed


def move_trigger(action, old_frame, new_frame):
    """挪触发: 双通道 f=old → f=new (保持值与插值)."""
    old_frame, new_frame = int(old_frame), int(new_frame)
    if new_frame < 0:
        raise ValueError("目标帧需 >= 0")
    if old_frame == new_frame:
        return 0
    chans = get_channels(action, MTRIGGER)
    moved = 0
    for ix in (0, 1):
        fc = chans.get(ix)
        if fc is None:
            continue
        for kf in fc.keyframe_points:
            if int(kf.co[0]) == old_frame:
                kf.co = (new_frame, int(kf.co[1]))
                moved += 1
        fc.keyframe_points.sort()
        fc.update()
    return moved


def set_row(action, row_index, value):
    """设 ReqNo 行值 (f0 常数键; 行缺轨则建, LINEAR; 新建行自动规范化行序)."""
    if not (0 <= row_index < len(REQNO_ROWS)):
        raise ValueError("行号需 0-%d (0=A), 得到 %d" % (len(REQNO_ROWS) - 1, row_index))
    rhash, letter = REQNO_ROWS[row_index]
    path = "FreeHKTiml.m%08X" % rhash
    fc = action.fcurves.find(path, index=0)
    created = fc is None
    if fc is None:
        fc = action.fcurves.new(data_path=path, index=0)
    hit = None
    for kf in fc.keyframe_points:
        if int(kf.co[0]) == 0:
            hit = kf
            break
    if hit is not None:
        hit.co = (0, int(value))
    else:
        insert_key(fc, 0, int(value), TRANSITION_LINEAR, 0, 0)
    fc.keyframe_points.sort()
    fc.update()
    if created:
        # 后补的行会 append 到 mTrigger 之后 → 二进制轨序里掩码会选中触发轨本身
        # (156 首版静默案例), 必须重排: 行全部在前, 触发殿后
        canonicalize_row_order(action)
    return letter


# ---- 行序规范化 (2026-10-24: 导出二进制轨序 = blend fcurve 插入序) ----------

def _row_rank(data_path, array_index):
    """排序键: ReqNo 行(0, 行号A..F) < mTrigger(1, 通道) < 其余(2, 保序)."""
    if data_path == "FreeHKTiml.m%08X" % MTRIGGER:
        return (1, array_index)
    for i, (rhash, _letter) in enumerate(REQNO_ROWS):
        if data_path == "FreeHKTiml.m%08X" % rhash:
            return (0, i)
    return (2, 0)


def row_order_ok(action):
    ranks = [_row_rank(fc.data_path, fc.array_index) for fc in action.fcurves]
    return ranks == sorted(ranks)


def canonicalize_row_order(action):
    """重建 fcurve 序: ReqNo 行(A..F) → mTrigger(ix0, ix1) → 其余 (保持相对序).
    已规范则原样返回 False; 键值/帧/插值/back/period 全保留."""
    if row_order_ok(action):
        return False
    snap = []
    for i, fc in enumerate(action.fcurves):
        keys = [dict(frame=int(k.co[0]), value=int(k.co[1]), interp=k.interpolation,
                     back=int(k.back), period=int(k.period)) for k in fc.keyframe_points]
        snap.append((_row_rank(fc.data_path, fc.array_index), i,
                     (fc.data_path, fc.array_index, keys)))
    for fc in list(action.fcurves):
        action.fcurves.remove(fc)
    for _rank, _i, (path, ix, keys) in sorted(snap, key=lambda t: (t[0], t[1])):
        fc = action.fcurves.new(data_path=path, index=ix)
        for k in keys:
            insert_key(fc, k["frame"], k["value"], k["interp"], k["back"], k["period"])
        fc.keyframe_points.sort()
        fc.update()
    return True


# ---- Operator --------------------------------------------------------------

class _SndOp:
    bl_options = {"REGISTER", "UNDO"}
    action_name: StringProperty(name="TIML Action", options={'HIDDEN'})

    def _target(self, context):
        name = getattr(self, "action_name", "")
        if name:
            a = bpy.data.actions.get(name)
            if a is None:
                raise ValueError("TIML action 不存在: %s" % name)
            return a
        return context.window_manager.freehk_timl_edit


class FREEHK_OT_timl_snd_list(_SndOp, bpy.types.Operator):
    bl_idname = "freehk.timl_snd_list"
    bl_label = "列出声音/事件"
    bl_description = "解码 ReqNo 行 + mTrigger 触发 (行掩码→发声行) → 控制台 + 信息区"

    def execute(self, context):
        a = self._target(context)
        if not a:
            self.report({'ERROR'}, "未选择 TIML action (与 'TIML 窗口编辑' 共用选择器)")
            return {'CANCELLED'}
        rows = snd_rows(a)
        lines = ["ReqNo 行: " + "  ".join(
            _fmt_row(l, v) for l, _f, v in rows)]
        trig = get_channels(a, MTRIGGER)[0]
        if trig is None:
            lines.append("无 mTrigger 轨 (连续参数类别, 如 GameParameter)")
        else:
            for fr, val in trigger_events(a):
                if fr == 0 and val == 0:
                    lines.append("  f0 = 基线键 (勿删)")
                else:
                    lines.append("  f%d → %s" % (fr, decode_mask(val, rows)))
        for line in lines:
            print("[timl-sound]", a.name, "|", line)
        self.report({'INFO'}, "%d 行触发/行表, 详见系统控制台; 面板内实时显示" % (len(lines) - 1))
        return {'FINISHED'}


class FREEHK_OT_timl_snd_trig_add(_SndOp, bpy.types.Operator):
    bl_idname = "freehk.timl_snd_trig_add"
    bl_label = "加/改触发"
    bl_description = ("mTrigger 帧=触发帧, 值=ReqNo 行位掩码 (bit0=A..bit5=F, 如 4=只发C行). "
                      "帧上已有键则改值. f0=0 为基线键勿动")
    frame: IntProperty(name="帧", default=-1)
    value: IntProperty(name="行掩码", default=1, min=0, max=65535,
                       description="bit0=A, bit1=B, ... 例: 1=A 2=B 4=C 6=B+C")

    def execute(self, context):
        a = self._target(context)
        if not a:
            self.report({'ERROR'}, "未选择 TIML action")
            return {'CANCELLED'}
        scn = context.scene
        frame = self.frame if self.frame >= 0 else int(scn.freehk_snd_frame)
        value = self.value if self.value >= 0 else int(scn.freehk_snd_value)
        try:
            set_trigger(a, frame, value)
        except ValueError as e:
            self.report({'ERROR'}, str(e))
            return {'CANCELLED'}
        rows = snd_rows(a)
        self.report({'INFO'}, "触发 f%d → %s" % (frame, decode_mask(value, rows)))
        return {'FINISHED'}


class FREEHK_OT_timl_snd_trig_del(_SndOp, bpy.types.Operator):
    bl_idname = "freehk.timl_snd_trig_del"
    bl_label = "删触发"
    bl_description = "移除 mTrigger 上该帧的键 (双通道同步; = 删那次发声, EG02 已验证)"
    frame: IntProperty(name="帧", default=-1)

    def execute(self, context):
        a = self._target(context)
        if not a:
            self.report({'ERROR'}, "未选择 TIML action")
            return {'CANCELLED'}
        scn = context.scene
        frame = self.frame if self.frame >= 0 else int(scn.freehk_snd_frame)
        removed = del_trigger(a, frame)
        if not removed:
            self.report({'WARNING'}, "f%d 无触发键" % frame)
        else:
            self.report({'INFO'}, "已删 f%d 触发 (%d 通道)" % (frame, removed))
        return {'FINISHED'}


class FREEHK_OT_timl_snd_trig_move(_SndOp, bpy.types.Operator):
    bl_idname = "freehk.timl_snd_trig_move"
    bl_label = "挪触发"
    bl_description = "mTrigger 该帧的键挪到新帧 (保持值与插值, 双通道同步)"
    frame: IntProperty(name="原帧", default=-1)
    to_frame: IntProperty(name="新帧", default=-1)

    def execute(self, context):
        a = self._target(context)
        if not a:
            self.report({'ERROR'}, "未选择 TIML action")
            return {'CANCELLED'}
        scn = context.scene
        old = self.frame if self.frame >= 0 else int(scn.freehk_snd_frame)
        new = self.to_frame if self.to_frame >= 0 else int(scn.freehk_snd_to)
        try:
            moved = move_trigger(a, old, new)
        except ValueError as e:
            self.report({'ERROR'}, str(e))
            return {'CANCELLED'}
        if not moved:
            self.report({'WARNING'}, "f%d 无触发键" % old)
        else:
            self.report({'INFO'}, "触发 f%d → f%d" % (old, new))
        return {'FINISHED'}


class FREEHK_OT_timl_snd_row_set(_SndOp, bpy.types.Operator):
    bl_idname = "freehk.timl_snd_row_set"
    bl_label = "设 ReqNo 行值"
    bl_description = ("改某行 ReqNo 声音请求号 (f0 常数键). 行 0=A .. 5=F; "
                      "行值改动会导出生效, 但音色效果未逐一 A/B")
    row: IntProperty(name="行 (0=A)", default=-1, min=-1, max=5)
    value: IntProperty(name="值", default=-1, min=-1, max=65535)

    def execute(self, context):
        a = self._target(context)
        if not a:
            self.report({'ERROR'}, "未选择 TIML action")
            return {'CANCELLED'}
        scn = context.scene
        row = self.row if self.row >= 0 else int(scn.freehk_snd_row)
        value = self.value if self.value >= 0 else int(scn.freehk_snd_value)
        try:
            letter = set_row(a, row, value)
        except ValueError as e:
            self.report({'ERROR'}, str(e))
            return {'CANCELLED'}
        self.report({'INFO'}, "ReqNo %s = %d%s" % (letter, value,
                                                    ("·" + voice_label(value)) if voice_label(value) else ""))
        return {'FINISHED'}


class FREEHK_OT_timl_snd_canon(_SndOp, bpy.types.Operator):
    bl_idname = "freehk.timl_snd_canon"
    bl_label = "规范化行序"
    bl_description = ("把 ReqNo 行移到 mTrigger 之前 (位掩码按二进制轨序取行; 行落在触发轨"
                      "后面会让掩码选中触发轨本身 → 无声). 键值/帧/插值全保留")

    def execute(self, context):
        a = self._target(context)
        if not a:
            self.report({'ERROR'}, "未选择 TIML action")
            return {'CANCELLED'}
        changed = canonicalize_row_order(a)
        self.report({'INFO'}, "已重排 fcurve 序 (行→触发)" if changed else "行序已规范, 无需修改")
        return {'FINISHED'}


# ---- EG02 喊声快捷 (2026-10-24) ---------------------------------------------

EG02_BASE = "TIML::nTimelineParam::nWwiseTimeline::EventGroup02"


def _freehk_tree_for(entry):
    for ng in bpy.data.node_groups:
        if ng.bl_idname != "FreeHKNodeTree":
            continue
        for n in ng.nodes:
            if n.bl_idname == "LMTEntryNode" and n.entryNum == entry:
                return ng
    return None


def _entry_dnode(tree, entry):
    for n in tree.nodes:
        if n.bl_idname == "LMTEntryNode" and n.entryNum == entry:
            for i in n.inputs:
                for l in i.links:
                    if l.from_node.bl_idname == "TIMLDataNode":
                        return l.from_node
    return None


def _eg02_action_of(dnode):
    for i in dnode.inputs:
        for l in i.links:
            fn = l.from_node
            if (fn.bl_idname == "TIMLActionNode" and fn.input_action
                    and "EventGroup02" in fn.input_action.name):
                return fn.input_action
    return None


def _build_eg02_event(tree, dnode, voice_id, frame):
    """从 blend 里现成 EG02 拷 freehk ID 属性 → 重建规范单行事件 → 挂节点接入."""
    donors = [a for a in bpy.data.actions if a.name.startswith(EG02_BASE + ".")]
    if not donors:
        raise ValueError("blend 里没有现成 EventGroup02 可作模板 (先给任一动作手工建一个)")
    donor = next((a for a in donors
                  if row_order_ok(a) and sum(1 for _l, f, _v in snd_rows(a) if f is not None) == 1),
                 donors[0])
    act = donor.copy()
    for fc in list(act.fcurves):
        act.fcurves.remove(fc)
    row = act.fcurves.new(data_path="FreeHKTiml.m%08X" % REQNO_ROWS[0][0], index=0)
    insert_key(row, 0, int(voice_id), TRANSITION_LINEAR, 0, 0)
    row.keyframe_points.sort()
    row.update()
    for ix in (0, 1):
        fc = act.fcurves.new(data_path="FreeHKTiml.m%08X" % MTRIGGER, index=ix)
        insert_key(fc, 0, 0, TRANSITION_TRIGGER, 1 if ix == 0 else 0, 0)
        insert_key(fc, int(frame), 1 if ix == 0 else 0, TRANSITION_TRIGGER, 0, 0)
        fc.keyframe_points.sort()
        fc.update()
    i = 1
    while ("%s.%03d" % (EG02_BASE, i)) in bpy.data.actions:
        i += 1
    act.name = "%s.%03d" % (EG02_BASE, i)
    node = tree.nodes.new('TIMLActionNode')
    node.input_action = act
    locs = [(l.from_node.location.x, l.from_node.location.y)
            for sock in dnode.inputs for l in sock.links]
    if locs:
        node.location = (max(x for x, _y in locs) + 40, max(y for _x, y in locs) + 40)
    tree.links.new(node.outputs[0], dnode.inputs['TIML Animation'])
    return act


class FREEHK_OT_timl_snd_voice_add(bpy.types.Operator):
    bl_idname = "freehk.timl_snd_voice_add"
    bl_label = "给动作加喊声 (EG02)"
    bl_options = {"REGISTER", "UNDO"}
    bl_description = ("给 LMT entry 的 EG02 加一次发声: 无 EG02 则从模板新建事件并挂节点; "
                      "有则用承载该语音号的行加触发 (行缺失自动建+规范行序, 幂等). "
                      "语音号: 1309=爆发哈 1409=前摇哦 1119=中段喊 1019=挥砍喊")
    entry: IntProperty(name="LMT entry", default=-1, min=-1)
    frame: IntProperty(name="帧", default=-1, min=-1)
    voice_id: IntProperty(name="语音请求号", default=1309, min=0, max=65535)

    def execute(self, context):
        scn = context.scene
        entry = self.entry if self.entry >= 0 else int(scn.freehk_snd_entry)
        frame = self.frame if self.frame >= 0 else int(scn.freehk_snd_frame)
        vid = int(self.voice_id)
        if frame <= 0:
            self.report({'ERROR'}, "帧需 >= 1 (f0 是基线键)")
            return {'CANCELLED'}
        tree = _freehk_tree_for(entry)
        if tree is None:
            self.report({'ERROR'}, "FreeHK 树里找不到 entry %d" % entry)
            return {'CANCELLED'}
        dnode = _entry_dnode(tree, entry)
        if dnode is None:
            self.report({'ERROR'}, "entry %d 没有 TIML Data Node" % entry)
            return {'CANCELLED'}
        act = _eg02_action_of(dnode)
        created = act is None
        if created:
            try:
                act = _build_eg02_event(tree, dnode, vid, frame)
            except ValueError as e:
                self.report({'ERROR'}, str(e))
                return {'CANCELLED'}
            self.report({'INFO'}, "entry %d 新建 EG02 (%s): 行A=%d(%s), 触发 f%d" % (
                entry, act.name, vid, VOICE_ID_HINTS.get(vid, "?"), frame))
            return {'FINISHED'}
        canonicalize_row_order(act)  # 掩码按二进制轨序取行, 先保规范
        rows = snd_rows(act)
        row_idx = next((i for i, (_l, _f, v) in enumerate(rows) if v == vid), None)
        if row_idx is None:
            empty = next((i for i, (_l, f, _v) in enumerate(rows) if f is None), None)
            if empty is None:
                empty = 0  # 六行全满, 覆盖 A 行
            set_row(act, empty, vid)
            canonicalize_row_order(act)
            rows = snd_rows(act)
            row_idx = next(i for i, (_l, _f, v) in enumerate(rows) if v == vid)
        mask = 1 << row_idx
        if dict(trigger_events(act)).get(int(frame)) == mask:
            self.report({'INFO'}, "entry %d EG02 已有 f%d → %s, 无需修改" % (
                entry, frame, decode_mask(mask, rows)))
            return {'FINISHED'}
        set_trigger(act, frame, mask)
        self.report({'INFO'}, "entry %d 已加喊声: f%d → %s" % (
            entry, frame, decode_mask(mask, rows)))
        return {'FINISHED'}


# ---- UI --------------------------------------------------------------------

def _draw_snd_panel(layout, context):
    wm = context.window_manager
    # 喊声快捷: 只依赖 entry, 不依赖 action 选择器
    vbox = layout.box()
    vbox.label(text="喊声快捷 (EG02): 1309=爆发哈 1409=前摇哦")
    vrow = vbox.row(align=True)
    vrow.prop(context.scene, "freehk_snd_entry", text="entry")
    vrow.prop(context.scene, "freehk_snd_voice", text="语音号")
    vrow2 = vbox.row(align=True)
    vrow2.prop(context.scene, "freehk_snd_frame", text="帧")
    vbox.operator("freehk.timl_snd_voice_add")
    if not hasattr(wm, "freehk_timl_edit"):
        layout.label(text="需 TIML 窗口编辑模块提供 action 选择器")
        return
    a = wm.freehk_timl_edit
    if not a:
        layout.label(text="与 'TIML 窗口编辑' 共用上面的 action 选择器")
        return
    layout.operator("freehk.timl_snd_list")

    rows = snd_rows(a)
    box = layout.box()
    box.label(text="ReqNo 行:  " + "  ".join(
        _fmt_row(l, v) for l, _f, v in rows))
    rrow = box.row(align=True)
    rrow.prop(context.scene, "freehk_snd_row", text="行(0=A)")
    rrow.prop(context.scene, "freehk_snd_value", text="值")
    rrow2 = box.row(align=True)
    rrow2.operator("freehk.timl_snd_row_set")
    if not row_order_ok(a):
        rrow2.operator("freehk.timl_snd_canon", icon='ERROR')
    else:
        rrow2.operator("freehk.timl_snd_canon")

    tbox = layout.box()
    trig = get_channels(a, MTRIGGER)[0]
    if trig is None:
        tbox.label(text="无 mTrigger (连续参数类别如 GameParameter:")
        tbox.label(text="改时机请在曲线编辑器动 GameParam/KeyFrame 键)")
    else:
        tbox.label(text="触发 (mTrigger, 值=行掩码):")
        shown = 0
        for fr, val in trigger_events(a):
            if fr == 0 and val == 0:
                tbox.label(text="  f0 基线键 (勿删)")
                continue
            tbox.label(text="  f%d → %s" % (fr, decode_mask(val, rows)))
            shown += 1
        if not shown:
            tbox.label(text="  (无触发事件)")
        crow = tbox.row(align=True)
        crow.prop(context.scene, "freehk_snd_frame", text="帧")
        crow.prop(context.scene, "freehk_snd_value", text="掩码")
        tbox.operator("freehk.timl_snd_trig_add")
        crow2 = tbox.row(align=True)
        crow2.prop(context.scene, "freehk_snd_frame", text="帧")
        crow2.operator("freehk.timl_snd_trig_del")
        crow3 = tbox.row(align=True)
        crow3.prop(context.scene, "freehk_snd_frame", text="原帧")
        crow3.prop(context.scene, "freehk_snd_to", text="新帧")
        tbox.operator("freehk.timl_snd_trig_move")
    layout.label(text="↑ 点击立即改内存TIML (Ctrl+Z撤销); 导出后才写.lmt")
    hint1, hint2 = role_hints(a)
    if hint1:
        hbox = layout.box()
        hbox.label(text=hint1)
        if hint2:
            hbox.label(text=hint2)


class FREEHK_PT_timl_sound_3d(bpy.types.Panel):
    bl_label = "TIML 声音/事件"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = "FreeHK"

    def draw(self, context):
        _draw_snd_panel(self.layout, context)


class FREEHK_PT_timl_sound_dopesheet(bpy.types.Panel):
    bl_label = "TIML 声音/事件"
    bl_space_type = 'DOPESHEET_EDITOR'
    bl_region_type = 'UI'
    bl_category = "FreeHK"

    def draw(self, context):
        _draw_snd_panel(self.layout, context)


_classes = [
    FREEHK_OT_timl_snd_list,
    FREEHK_OT_timl_snd_trig_add,
    FREEHK_OT_timl_snd_trig_del,
    FREEHK_OT_timl_snd_trig_move,
    FREEHK_OT_timl_snd_row_set,
    FREEHK_OT_timl_snd_canon,
    FREEHK_OT_timl_snd_voice_add,
    FREEHK_PT_timl_sound_3d,
    FREEHK_PT_timl_sound_dopesheet,
]

_SCENE_PROPS = (
    ("freehk_snd_frame", IntProperty(name="触发帧", default=0, min=0)),
    ("freehk_snd_to", IntProperty(name="新帧", default=0, min=0)),
    ("freehk_snd_value", IntProperty(name="行掩码/值", default=1, min=0, max=65535)),
    ("freehk_snd_row", IntProperty(name="ReqNo 行 (0=A)", default=0, min=0, max=5)),
    ("freehk_snd_entry", IntProperty(name="LMT entry", default=156, min=0)),
    ("freehk_snd_voice", IntProperty(name="语音请求号", default=1309, min=0, max=65535)),
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
