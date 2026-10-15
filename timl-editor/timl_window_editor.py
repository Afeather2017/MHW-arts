# -*- coding: utf-8 -*-
"""TIML 窗口编辑器 — FreeKinetics 扩展 (防御/无敌/霸体/攻击/按键 窗 + 位语义).

设计: 完全复用 FreeKinetics 的 TIML 表示 — TIML action 的 FreeHKTiml.m<hash>
fcurve; bigFlags 类 (mFlag/mFlag1..6/mTrigger...) 按 array_index 0/1 拆低/高 16 位
(timl_importer.TIMLFlagImport 的表示), 导出侧 parseBigFlag 按整数帧合并.
本模块只在这对通道上做"位窗"操作, 不引入任何新的序列化.

窗口配方 (LMT_TIML research + AGENTS.md 实证):
  无敌帧  PlMotionCommon.mFlag b26 窗 + mTrigger 2048 门键(窗内, 位置无关)
          — b26 与 b28 互斥, 写 b26 前先清窗内 b28
  霸体帧  PlMotionCommon.mFlag b24 窗 (无需门键)
  防御帧  PlMotionCommon.mFlag b28 窗 (角度格挡; 写前清窗内 b26)
  攻击帧  EventCollisionNN.mFlag 整值窗 (0=off, 1/2/3=槽位)
  按键    PlMotionInput mFlag/mFlag1/mFlag2/... 位窗
  基线    新建 PlMotionCommon.mFlag 轨时种 f0 = 0x02400006 (b25|b22|b1|b2)
  帧律    一律整数帧 (小数帧是已知崩溃源); transition ↔ 插值: mFlag族=EXPO(5),
          mTrigger=SINE(6), 常量=LINEAR(1)

位语义 (2026-10-15 汇总, 标注置信度): 见 BIT_SEMANTICS.

用法 (GUI): N 面板 "FreeHK" → TIML 窗口编辑 → 选 action → 按钮.
用法 (无头): bpy.ops.freehk.timl_iframe_window(action_name=..., start=0, end=12).
"""
import bpy
from bpy.props import IntProperty, BoolProperty, PointerProperty, StringProperty

try:
    from .blender.timl_importer import bigFlags
except ImportError:  # 脚本直跑 (不作为包导入)
    bigFlags = [150806694, 2575924291, 4027018852, 2154666731, 4150962813,
                1852046279, 503910216, 1762541534, 2768909048, 3787782803]
BIGFLAG_SET = set(bigFlags)

# hash 常量 (research/MHW-Free-HyperKinetics bigFlags 表)
MFLAG = 150806694       # 0x08FD20A6
MFLAG6 = 4027018852
MFLAG3 = 2154666731
MFLAG2 = 4150962813     # 0xF76AAA7D
MFLAG1 = 1852046279     # 0x6E63FBC7
MFLAG4 = 503910216      # 0x1E090F48
MFLAG5 = 1762541534
MTRIGGER = 2575924291   # 0x99897C43
CAT_PC = 0x48D6114F     # nTimelineParam::PlMotionCommon (unkn0=11)
CAT_PI = 0x6FCCD10E     # nTimelineParam::PlMotionInput   (unkn0=9)

# 轨选择器: index → (hash, 名)
TRACKS = [
    (MFLAG, "mFlag"), (MFLAG1, "mFlag1"), (MFLAG2, "mFlag2"), (MFLAG3, "mFlag3"),
    (MFLAG4, "mFlag4"), (MFLAG5, "mFlag5"), (MFLAG6, "mFlag6"),
]
TRACK_HASHES = [h for h, _ in TRACKS]

GATE_VALUE = 2048            # mTrigger 2048 = 无敌门 (0x800)
PC_BASELINE = 0x02400006     # b25|b22|b1|b2

TRANSITION_FLAG = 'EXPO'     # vanilla mFlag 族 transition=5
TRANSITION_TRIGGER = 'SINE'  # vanilla mTrigger transition=6

# ---- 位语义表 (track hash → 类别 → bit → (标签, 置信度)) ---------------------
# 置信度: 确证=局内A/B验证 / 推断=强证据未终验 / 未解=仅观测到模式
C_TRUE, C_GUESS, C_UNK = "确证", "推断", "未解"

BIT_SEMANTICS = {
    MFLAG: {  # PlMotionCommon.mFlag / PlMotionInput.mFlag 共用 hash, 按类别分读
        "PC": {
            26: ("无敌 (no-hit, 需 2048 门)", C_TRUE),
            24: ("霸体 (super armor)", C_TRUE),
            28: ("角度格挡 (正面格挡/背面受击)", C_TRUE),
            25: ("攻击动作基线", C_TRUE),
            22: ("基线位", C_TRUE),
            1: ("基线位", C_TRUE),
            2: ("基线位", C_TRUE),
            27: ("霸体? (115开窗/108,109,338-340尾脉冲)", C_UNK),
            31: ("GP? 见切/收刀族全程持有; 他版306=b31+b26 组合GP窗", C_UNK),
            30: ("未知 (0.0.2-345 的 306 新增 0x52400006)", C_UNK),
            15: ("事件位写入?", C_UNK),
        },
        "PI": {
            8: ("翻滚接受", C_TRUE),
            14: ("连段预输入", C_TRUE),
            16: ("连段预输入", C_TRUE),
            30: ("连段预输入", C_TRUE),
            17: ("特殊派生窗 (见切→必殺攻撃, 配 b31)", C_TRUE),
            31: ("特殊派生窗 (配 b17)", C_TRUE),
            9: ("与 b8 伴出", C_UNK),
            15: ("与 b14/16 伴出", C_UNK),
            0: ("未知 (循环/收势族)", C_UNK),
        },
    },
    MFLAG1: {
        "PC": {
            30: ("零伤格挡成分 (他版306: 0x50000000=b30+b28, 配 mStaminaParamIndex=256)", C_GUESS),
            28: ("零伤格挡成分 (配 b30)", C_GUESS),
        },
        "PI": {  # 组合键开关: R2+方向 解锁 (2026-10-12 假设, 单变量测试文件已造未测)
            12: ("组合键开关 (R2+方向, 对 b26)", C_GUESS),
            26: ("组合键开关 (对 b12)", C_GUESS),
            13: ("组合键开关 (对 b27)", C_GUESS),
            27: ("组合键开关 (对 b13)", C_GUESS),
        },
    },
    MFLAG2: {  # 事件位写入轨 (值=要写的 event bit) — 强假设
        "PC": {
            15: ("写事件位15 = 见切成功锁存/纳刀态信号", C_GUESS),
        },
        "PI": {
            4: ("未解 (值族 0x150/0x3F0)", C_UNK),
            5: ("未解", C_UNK), 6: ("未解", C_UNK), 7: ("未解", C_UNK),
            8: ("未解", C_UNK), 9: ("未解", C_UNK),
            22: ("未解", C_UNK), 23: ("未解", C_UNK),
        },
    },
}
for _h in (MFLAG3, MFLAG4, MFLAG5, MFLAG6):
    BIT_SEMANTICS[_h] = {"PC": {}, "PI": {}}
BIT_SEMANTICS[MFLAG4]["PI"][0] = ("未解 (342 见 4键)", C_UNK)

# mTrigger 值语义
TRIGGER_SEMANTICS = {
    2048: ("无敌门 (b26 窗内放一个即可, 位置无关)", C_TRUE),
    1: ("通用事件码", C_GUESS),
    2: ("通用事件码", C_GUESS),
    65537: ("见切起手码 0x10001", C_UNK),
}


def bit_label(tf_hash, bit, cat="PI"):
    """位 → '标签' 或 '未解 bN'."""
    sem = BIT_SEMANTICS.get(tf_hash, {}).get(cat, {}).get(bit)
    if sem:
        return sem[0]
    return "未解 b%d" % bit


def bit_conf(tf_hash, bit, cat="PI"):
    sem = BIT_SEMANTICS.get(tf_hash, {}).get(cat, {}).get(bit)
    return sem[1] if sem else C_UNK


# ---- 基础: action / fcurve 访问 -----------------------------------------

def timl_actions():
    return [a for a in bpy.data.actions
            if getattr(a.freehk, "starType", "") == "TIML_Action"]


def action_param_hash(action):
    raw = getattr(action.freehk, "timelineParamRaw", "") or getattr(action.freehk, "timelineParam", "")
    try:
        return int(raw[1:], 16)
    except (ValueError, IndexError, TypeError):
        return None


def action_cat(action):
    """'PC' / 'PI' / 其他→'?'."""
    ph = action_param_hash(action)
    if ph == CAT_PC:
        return "PC"
    if ph == CAT_PI:
        return "PI"
    return "?"


def get_channels(action, tf_hash, create=False):
    """返回 {0: 低16位fcurve或None, 1: 高16位fcurve或None}."""
    path = "FreeHKTiml.m%08X" % tf_hash
    out = {}
    for ix in (0, 1):
        f = action.fcurves.find(path, index=ix)
        if f is None and create:
            f = action.fcurves.new(data_path=path, index=ix)
        out[ix] = f
    return out


def channel_for_bit(channels, bit):
    """bit(全局0-31) → (该bit所在通道, 通道内掩码)."""
    if bit >= 16:
        return channels[1], 1 << (bit - 16)
    return channels[0], 1 << bit


def insert_key(fcurve, frame, value, interp, back, period):
    kf = fcurve.keyframe_points.insert(int(frame), int(value))
    kf.interpolation = interp
    kf.back = back
    kf.period = period
    kf.co = (int(frame), int(value))  # insert 可能保留小数帧, 强制整数
    return kf


def holding_params(fcurve, frame, default_interp, default_back=1):
    """frame 之前最近键的 (interpolation, back, period); 空轨给默认."""
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


# ---- 通道级窗口编辑 (单通道最小键, 不动另一通道) ---------------------------

def channel_bit_spans(fcurve, local_mask):
    """通道步进函数中 local_mask 的开区间 [(start, end_or_None)]."""
    if fcurve is None:
        return []
    spans, start = [], None
    for kf in fcurve.keyframe_points:
        on = (int(kf.co[1]) & local_mask) != 0
        if on and start is None:
            start = int(kf.co[0])
        elif not on and start is not None:
            spans.append((start, int(kf.co[0])))
            start = None
    if start is not None:
        spans.append((start, None))
    return spans


def _key_at(fcurve, f):
    for kf in fcurve.keyframe_points:
        if int(kf.co[0]) == f:
            return kf
    return None


def _value_at(fcurve, f):
    v = 0
    for kf in fcurve.keyframe_points:
        if kf.co[0] <= f:
            v = int(kf.co[1])
        else:
            break
    return v


def channel_set_bit_window(fcurve, local_mask, f0, f1):
    """通道上把 local_mask 打开成 [f0,f1): f0 键置位, f1 键清位, 窗内键全置位.
    整数帧; 其他位保留."""
    if f0 >= f1:
        raise ValueError("需要 start < end (整数帧)")
    k = _key_at(fcurve, f0)
    if k is None:
        interp, back, period = holding_params(fcurve, f0, TRANSITION_FLAG)
        insert_key(fcurve, f0, _value_at(fcurve, f0) | local_mask, interp, back, period)
    else:
        k.co = (f0, int(k.co[1]) | local_mask)
    k = _key_at(fcurve, f1)
    if k is None:
        interp, back, period = holding_params(fcurve, f1, TRANSITION_FLAG)
        insert_key(fcurve, f1, _value_at(fcurve, f1) & ~local_mask, interp, back, period)
    else:
        k.co = (f1, int(k.co[1]) & ~local_mask)
    for kf in fcurve.keyframe_points:
        t = int(kf.co[0])
        if f0 < t < f1:
            kf.co = (t, int(kf.co[1]) | local_mask)
    fcurve.update()


def channel_clear_bit_range(fcurve, local_mask, f0, f1):
    """清 [f0,f1) 内所有键的 local_mask (含边界键, 不动边界外)."""
    if fcurve is None:
        return False
    hit = False
    for kf in fcurve.keyframe_points:
        t = int(kf.co[0])
        if f0 <= t < f1:
            kf.co = (t, int(kf.co[1]) & ~local_mask)
            hit = True
    if hit:
        fcurve.update()
    return hit


def channel_set_value_window(fcurve, on_value, f0, f1):
    """整值窗 (攻击帧): f0=on_value, f1=0, 窗内=on_value."""
    if f0 >= f1:
        raise ValueError("需要 start < end (整数帧)")
    k = _key_at(fcurve, f0)
    if k is None:
        interp, back, period = holding_params(fcurve, f0, 'LINEAR', 0)
        insert_key(fcurve, f0, on_value, interp, back, period)
    else:
        k.co = (f0, on_value)
    k = _key_at(fcurve, f1)
    if k is None:
        interp, back, period = holding_params(fcurve, f1, 'LINEAR', 0)
        insert_key(fcurve, f1, 0, interp, back, period)
    else:
        k.co = (f1, 0)
    for kf in fcurve.keyframe_points:
        t = int(kf.co[0])
        if f0 < t < f1:
            kf.co = (t, on_value)
    fcurve.update()


# ---- 配方 ------------------------------------------------------------------

def _ensure_pc_mflag(action):
    """PlMotionCommon.mFlag 双通道, 空轨时种基线键."""
    chans = get_channels(action, MFLAG, create=True)
    for ix in (0, 1):
        f = chans[ix]
        if not len(f.keyframe_points):
            seed = (PC_BASELINE & 0xFFFF) if ix == 0 else ((PC_BASELINE >> 16) & 0xFFFF)
            insert_key(f, 0, seed, TRANSITION_FLAG, 1, 0)
            f.update()
    return chans


def _ensure_pc_mtrigger(action):
    f = action.fcurves.find("FreeHKTiml.m%08X" % MTRIGGER, index=0)
    if f is None:
        f = action.fcurves.new(data_path="FreeHKTiml.m%08X" % MTRIGGER, index=0)
    return f


def recipe_iframes(action, f0, f1):
    _ensure_pc_mflag(action)
    chans = get_channels(action, MFLAG)
    hi, mask = channel_for_bit(chans, 26)
    channel_set_bit_window(hi, mask, f0, f1)
    msgs = ["无敌窗 b26 [%d,%d) = %d帧 ≈ %.2fs" % (f0, f1, f1 - f0, (f1 - f0) / 60.0)]
    lo28, mask28 = channel_for_bit(chans, 28)
    if channel_clear_bit_range(lo28, mask28, f0, f1):
        msgs.append("窗内 b28 (角度格挡) 已清除 (与 b26 互斥)")
    trig = _ensure_pc_mtrigger(action)
    if any((int(k.co[1]) & GATE_VALUE) for k in trig.keyframe_points):
        msgs.append("2048 门键已存在, 未重复添加")
    else:
        interp, back, period = holding_params(trig, f0, TRANSITION_TRIGGER, 0)
        insert_key(trig, f0, GATE_VALUE, interp, back, period)
        trig.update()
        msgs.append("mTrigger 2048 门键 @f%d" % f0)
    return msgs


def recipe_armor(action, f0, f1):
    _ensure_pc_mflag(action)
    chans = get_channels(action, MFLAG)
    hi, mask = channel_for_bit(chans, 24)
    channel_set_bit_window(hi, mask, f0, f1)
    return ["霸体窗 b24 [%d,%d) = %d帧 ≈ %.2fs" % (f0, f1, f1 - f0, (f1 - f0) / 60.0)]


def recipe_guard(action, f0, f1):
    _ensure_pc_mflag(action)
    chans = get_channels(action, MFLAG)
    hi, mask = channel_for_bit(chans, 28)
    channel_set_bit_window(hi, mask, f0, f1)
    msgs = ["格挡窗 b28 [%d,%d) = %d帧 ≈ %.2fs" % (f0, f1, f1 - f0, (f1 - f0) / 60.0)]
    lo26, mask26 = channel_for_bit(chans, 26)
    if channel_clear_bit_range(lo26, mask26, f0, f1):
        msgs.append("窗内 b26 (无敌) 已清除 (与 b28 互斥; 格挡无需 2048 门)")
    return msgs


def recipe_attack(action, slot, f0, f1):
    if slot not in (0, 1, 2, 3):
        raise ValueError("槽位 0=off 1/2/3=on")
    lo = get_channels(action, MFLAG, create=True)[0]
    if slot == 0:
        changed = channel_clear_bit_range(lo, 0xFFFF, f0, f1)
        if not _key_at(lo, f1):
            interp, back, period = holding_params(lo, f1, 'LINEAR', 0)
            insert_key(lo, f1, _value_at(lo, f1), interp, back, period)
        lo.update()
        return ["攻击窗 OFF [%d,%d)%s" % (f0, f1, "" if changed else " (本就无窗)")]
    channel_set_value_window(lo, slot, f0, f1)
    return ["攻击窗 slot%d [%d,%d) = %d帧 ≈ %.2fs" % (slot, f0, f1, f1 - f0, (f1 - f0) / 60.0)]


def recipe_bit_window(action, tf_hash, bit, f0, f1, on, cat=None):
    """通用位窗 (防御实验位 b27/b31、按键位、mFlag1 组合键、mFlag2 事件位...)."""
    if cat is None:
        cat = action_cat(action)
    chans = get_channels(action, tf_hash, create=True)
    ch, mask = channel_for_bit(chans, bit)
    if on:
        channel_set_bit_window(ch, mask, f0, f1)
    else:
        channel_clear_bit_range(ch, mask, f0, f1)
        if not _key_at(ch, f1):
            interp, back, period = holding_params(ch, f1, TRANSITION_FLAG)
            insert_key(ch, f1, _value_at(ch, f1) & ~mask, interp, back, period)
        ch.update()
    tname = dict(TRACKS).get(tf_hash, "m?")
    return ["%s b%d(%s) [%d,%d) %s" % (tname, bit, bit_label(tf_hash, bit, cat),
                                       f0, f1, "开" if on else "关")]


def recipe_gate(action, frame):
    trig = _ensure_pc_mtrigger(action)
    if any((int(k.co[1]) & GATE_VALUE) for k in trig.keyframe_points):
        return ["2048 门键已存在, 未重复添加"]
    interp, back, period = holding_params(trig, frame, TRANSITION_TRIGGER, 0)
    insert_key(trig, frame, GATE_VALUE, interp, back, period)
    trig.update()
    return ["mTrigger 2048 门键 @f%d" % frame]


# ---- 窗口清单 (只读, 带语义标签) --------------------------------------------

def _fmt_spans(spans):
    return "  ".join("[%d,%s)" % (s, "open" if e is None else str(e)) for s, e in spans) or "(无)"


def describe_windows(action):
    """人类可读窗口清单 (防御/无敌/霸体/攻击/按键/门/事件位), 带语义标签."""
    out = []
    ph = action_param_hash(action)
    cat = action_cat(action)
    if ph == CAT_PC:
        chans = get_channels(action, MFLAG)
        if any(c is not None and len(c.keyframe_points) for c in chans.values()):
            for bit in (26, 24, 28, 27, 31, 30, 15):
                ch, mask = channel_for_bit(chans, bit)
                sp = channel_bit_spans(ch, mask)
                if sp:
                    out.append("mFlag %s(b%d): %s" % (bit_label(MFLAG, bit, "PC"), bit, _fmt_spans(sp)))
        for tf_hash, tname in ((MFLAG1, "mFlag1"), (MFLAG2, "mFlag2"),
                               (MFLAG3, "mFlag3"), (MFLAG4, "mFlag4"),
                               (MFLAG5, "mFlag5"), (MFLAG6, "mFlag6")):
            cs = get_channels(action, tf_hash)
            seen = set()
            for bit in range(32):
                ch, mask = channel_for_bit(cs, bit)
                sp = channel_bit_spans(ch, mask)
                if sp and bit not in seen:
                    seen.add(bit)
                    out.append("%s %s(b%d): %s" % (tname, bit_label(tf_hash, bit, cat), bit, _fmt_spans(sp)))
        trig = action.fcurves.find("FreeHKTiml.m%08X" % MTRIGGER, index=0)
        if trig is not None and len(trig.keyframe_points):
            parts = []
            for kf in trig.keyframe_points:
                v = int(kf.co[1])
                if not v:
                    continue
                if v == GATE_VALUE:
                    sem = TRIGGER_SEMANTICS.get(v)
                    label = "%d(%s)" % (v, sem[0]) if sem else str(v)
                elif v & GATE_VALUE:
                    label = "%d (含2048门)" % v
                elif v in TRIGGER_SEMANTICS:
                    label = "%d(%s)" % (v, TRIGGER_SEMANTICS[v][0])
                else:
                    label = str(v)
                parts.append("f%d=%s" % (int(kf.co[0]), label))
            out.append("mTrigger: " + ("  ".join(parts) if parts else "(无非零键)"))
    else:
        lo = get_channels(action, MFLAG)[0]
        if lo is not None and len(lo.keyframe_points):
            keys = [(int(k.co[0]), int(k.co[1])) for k in lo.keyframe_points]
            spans, start = [], None
            for frame, value in keys:
                on = value != 0
                if on and start is None:
                    start = frame
                elif not on and start is not None:
                    spans.append((start, frame))
                    start = None
            if start is not None:
                spans.append((start, None))
            if spans:
                out.append("mFlag值窗: %s" % _fmt_spans(spans))
        for tf_hash, tname in TRACKS:
            cs = get_channels(action, tf_hash)
            seen = set()
            for bit in range(32):
                ch, mask = channel_for_bit(cs, bit)
                sp = channel_bit_spans(ch, mask)
                if sp and bit not in seen:
                    seen.add(bit)
                    out.append("%s %s(b%d): %s" % (tname, bit_label(tf_hash, bit, cat), bit, _fmt_spans(sp)))
    return out


# ---- Operators -------------------------------------------------------------
# start/end = -1 表示取场景属性 (面板共享同一组起止帧输入).

def _scene_f(context, name, own, default=None):
    if own is not None and own >= 0:
        return own
    v = getattr(context.scene, name, default)
    return default if v is None else v


class _TimlOp:
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

    def _frames(self, context, end_prop="freehk_timl_end"):
        f0 = _scene_f(context, "freehk_timl_start", self.start, 0)
        f1 = _scene_f(context, end_prop, self.end, 12)
        if f0 < 0 or f1 <= f0:
            raise ValueError("需要 0 <= start < end (整数帧), 得到 [%s,%s)" % (f0, f1))
        return int(f0), int(f1)


class FREEHK_OT_timl_iframe_window(_TimlOp, bpy.types.Operator):
    bl_idname = "freehk.timl_iframe_window"
    bl_label = "无敌窗"
    bl_description = "PlMotionCommon.mFlag b26 无敌窗 + mTrigger 2048 门键; 清窗内 b28 (与角度格挡互斥)"
    start: IntProperty(name="起始帧", default=-1)
    end: IntProperty(name="结束帧", default=-1)

    def execute(self, context):
        a = self._target(context)
        if not a:
            self.report({'ERROR'}, "未选择 TIML action")
            return {'CANCELLED'}
        try:
            f0, f1 = self._frames(context)
            msgs = recipe_iframes(a, f0, f1)
        except ValueError as e:
            self.report({'ERROR'}, str(e))
            return {'CANCELLED'}
        for m in msgs:
            self.report({'INFO'}, m)
        return {'FINISHED'}


class FREEHK_OT_timl_armor_window(_TimlOp, bpy.types.Operator):
    bl_idname = "freehk.timl_armor_window"
    bl_label = "霸体窗"
    bl_description = "PlMotionCommon.mFlag b24 霸体窗 (无需门键)"
    start: IntProperty(name="起始帧", default=-1)
    end: IntProperty(name="结束帧", default=-1)

    def execute(self, context):
        a = self._target(context)
        if not a:
            self.report({'ERROR'}, "未选择 TIML action")
            return {'CANCELLED'}
        try:
            f0, f1 = self._frames(context)
            msgs = recipe_armor(a, f0, f1)
        except ValueError as e:
            self.report({'ERROR'}, str(e))
            return {'CANCELLED'}
        for m in msgs:
            self.report({'INFO'}, m)
        return {'FINISHED'}


class FREEHK_OT_timl_guard_window(_TimlOp, bpy.types.Operator):
    bl_idname = "freehk.timl_guard_window"
    bl_label = "防御窗"
    bl_description = "PlMotionCommon.mFlag b28 角度格挡窗 (正面格挡/背面受击); 清窗内 b26 (格挡无需 2048 门)"
    start: IntProperty(name="起始帧", default=-1)
    end: IntProperty(name="结束帧", default=-1)

    def execute(self, context):
        a = self._target(context)
        if not a:
            self.report({'ERROR'}, "未选择 TIML action")
            return {'CANCELLED'}
        try:
            f0, f1 = self._frames(context)
            msgs = recipe_guard(a, f0, f1)
        except ValueError as e:
            self.report({'ERROR'}, str(e))
            return {'CANCELLED'}
        for m in msgs:
            self.report({'INFO'}, m)
        return {'FINISHED'}


class FREEHK_OT_timl_attack_window(_TimlOp, bpy.types.Operator):
    bl_idname = "freehk.timl_attack_window"
    bl_label = "攻击窗"
    bl_description = "本 action (EventCollision) 的 mFlag 整值窗: slot 0=关 1/2/3=开"
    slot: IntProperty(name="槽位", default=-1, min=-1, max=3)
    start: IntProperty(name="起始帧", default=-1)
    end: IntProperty(name="结束帧", default=-1)

    def execute(self, context):
        a = self._target(context)
        if not a:
            self.report({'ERROR'}, "未选择 TIML action")
            return {'CANCELLED'}
        slot = self.slot if self.slot >= 0 else getattr(context.scene, "freehk_timl_slot", 2)
        try:
            f0, f1 = self._frames(context)
            msgs = recipe_attack(a, slot, f0, f1)
        except ValueError as e:
            self.report({'ERROR'}, str(e))
            return {'CANCELLED'}
        for m in msgs:
            self.report({'INFO'}, m)
        return {'FINISHED'}


class FREEHK_OT_timl_bit_window(_TimlOp, bpy.types.Operator):
    bl_idname = "freehk.timl_bit_window"
    bl_label = "任意位窗"
    bl_description = "通用 bigFlags 位窗 (0=mFlag 1=mFlag1 2=mFlag2 3-6=mFlag3-6); 防御实验位(b27/b31)/组合键(mFlag1)/事件位(mFlag2)/按键位全走这里"
    track: IntProperty(name="轨", default=-1, min=-1, max=6)
    bit: IntProperty(name="位", default=-1, min=-1, max=31)
    start: IntProperty(name="起始帧", default=-1)
    end: IntProperty(name="结束帧", default=-1)
    on: BoolProperty(name="开窗", default=True)

    def execute(self, context):
        a = self._target(context)
        if not a:
            self.report({'ERROR'}, "未选择 TIML action")
            return {'CANCELLED'}
        track = self.track if self.track >= 0 else getattr(context.scene, "freehk_timl_track", 0)
        bit = self.bit if self.bit >= 0 else getattr(context.scene, "freehk_timl_bit", 8)
        tf_hash = TRACK_HASHES[track]
        try:
            f0, f1 = self._frames(context)
            msgs = recipe_bit_window(a, tf_hash, bit, f0, f1, self.on)
        except ValueError as e:
            self.report({'ERROR'}, str(e))
            return {'CANCELLED'}
        for m in msgs:
            self.report({'INFO'}, m)
        return {'FINISHED'}


class FREEHK_OT_timl_gate(_TimlOp, bpy.types.Operator):
    bl_idname = "freehk.timl_gate"
    bl_label = "加 2048 门键"
    bl_description = "PlMotionCommon.mTrigger 加 2048 (无敌门, 位置无关)"
    frame: IntProperty(name="帧", default=-1)

    def execute(self, context):
        a = self._target(context)
        if not a:
            self.report({'ERROR'}, "未选择 TIML action")
            return {'CANCELLED'}
        frame = self.frame if self.frame >= 0 else getattr(context.scene, "freehk_timl_gate", 0)
        for m in recipe_gate(a, int(frame)):
            self.report({'INFO'}, m)
        return {'FINISHED'}


class FREEHK_OT_timl_list_windows(_TimlOp, bpy.types.Operator):
    bl_idname = "freehk.timl_list_windows"
    bl_label = "列出当前窗口"
    bl_description = "解出所选 action 的全部窗口 (带语义标签) → 控制台 + 信息区"

    def execute(self, context):
        a = self._target(context)
        if not a:
            self.report({'ERROR'}, "未选择 TIML action")
            return {'CANCELLED'}
        lines = describe_windows(a)
        for line in lines:
            print("[timl-window]", a.name, line)
        self.report({'INFO'}, "%d 个窗口, 详见系统控制台" % len(lines))
        return {'FINISHED'}


# ---- UI --------------------------------------------------------------------

def _draw_windows_panel(layout, context):
    wm = context.window_manager
    layout.prop(wm, "freehk_timl_edit")
    a = wm.freehk_timl_edit
    if not a:
        layout.label(text="选择要编辑的 TIML action", icon='INFO')
        return
    ph = action_param_hash(a)
    cat = action_cat(a)
    layout.operator("freehk.timl_list_windows", icon='VIEWZOOM')

    box = layout.box()
    row = box.row(align=True)
    row.prop(context.scene, "freehk_timl_start", text="起")
    row.prop(context.scene, "freehk_timl_end", text="止")

    if ph == CAT_PC:
        box.label(text="防御系 (PlMotionCommon)", icon='SHIELD')
        col = box.column(align=True)
        col.operator("freehk.timl_iframe_window", text="无敌窗 (b26+2048门)")
        col.operator("freehk.timl_armor_window", text="霸体窗 (b24)")
        col.operator("freehk.timl_guard_window", text="防御窗 (b28 角度格挡)")
        row = box.row(align=True)
        row.prop(context.scene, "freehk_timl_gate", text="门帧")
        row.operator("freehk.timl_gate", text="加2048门")
        adv = box.box()
        adv.label(text="任意位窗 (轨 0-6 = mFlag..mFlag6)", icon='IPO_CONSTANT')
        row = adv.row(align=True)
        row.prop(context.scene, "freehk_timl_track", text="轨")
        row.prop(context.scene, "freehk_timl_bit", text="位")
        row = adv.row(align=True)
        adv.operator("freehk.timl_bit_window", text="开窗")
        op = adv.operator("freehk.timl_bit_window", text="关窗")
        op.on = False
    elif "EventCollision" in a.name:
        box.label(text="攻击窗 (EventCollision)", icon='MESH_CUBE')
        row = box.row(align=True)
        row.prop(context.scene, "freehk_timl_slot", text="槽")
        row.operator("freehk.timl_attack_window", text="应用")
    elif "PlMotionInput" in a.name:
        box.label(text="按键窗 (PlMotionInput)", icon='HAND')
        row = box.row(align=True)
        row.prop(context.scene, "freehk_timl_track", text="轨")
        row.prop(context.scene, "freehk_timl_bit", text="位")
        row = box.row(align=True)
        row.operator("freehk.timl_bit_window", text="开窗")
        op = row.operator("freehk.timl_bit_window", text="关窗")
        op.on = False
    else:
        box.label(text="类别 0x%08X: 仅列出窗口" % (ph or 0), icon='INFO')

    # 当前窗口 (带语义标签)
    wbox = layout.box()
    wbox.label(text="当前窗口", icon='ACTION')
    for line in describe_windows(a):
        wbox.label(text=line)

    # 位语义速查 (本 action 类别相关的已知语义)
    sbox = layout.box()
    sbox.label(text="位语义速查 (%s)" % ("PlMotionCommon" if cat == "PC"
                                         else ("PlMotionInput" if cat == "PI" else "本类别未知")),
               icon='BOOKMARKS')
    for tf_hash, tname in TRACKS:
        sem = BIT_SEMANTICS.get(tf_hash, {}).get(cat if cat in ("PC", "PI") else "PI", {})
        for bit in sorted(sem):
            label, conf = sem[bit]
            sbox.label(text="%s b%d %s [%s]" % (tname, bit, label, conf))
    if ph == CAT_PC:
        for v in sorted(TRIGGER_SEMANTICS):
            l, c = TRIGGER_SEMANTICS[v]
            sbox.label(text="mTrigger=%d %s [%s]" % (v, l, c))


class FREEHK_PT_timl_windows_3d(bpy.types.Panel):
    bl_label = "TIML 窗口编辑"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = "FreeHK"

    def draw(self, context):
        _draw_windows_panel(self.layout, context)


class FREEHK_PT_timl_windows_dopesheet(bpy.types.Panel):
    bl_label = "TIML 窗口编辑"
    bl_space_type = 'DOPESHEET_EDITOR'
    bl_region_type = 'UI'
    bl_category = "FreeHK"

    def draw(self, context):
        _draw_windows_panel(self.layout, context)


_classes = [
    FREEHK_OT_timl_iframe_window,
    FREEHK_OT_timl_armor_window,
    FREEHK_OT_timl_guard_window,
    FREEHK_OT_timl_attack_window,
    FREEHK_OT_timl_bit_window,
    FREEHK_OT_timl_gate,
    FREEHK_OT_timl_list_windows,
    FREEHK_PT_timl_windows_3d,
    FREEHK_PT_timl_windows_dopesheet,
]

_SCENE_PROPS = (
    ("freehk_timl_start", IntProperty(name="起始帧", default=0)),
    ("freehk_timl_end", IntProperty(name="结束帧", default=12)),
    ("freehk_timl_slot", IntProperty(name="攻击槽位", default=2, min=0, max=3)),
    ("freehk_timl_track", IntProperty(name="轨", default=0, min=0, max=6)),
    ("freehk_timl_bit", IntProperty(name="位", default=8, min=0, max=31)),
    ("freehk_timl_gate", IntProperty(name="门帧", default=0)),
)


def register():
    for c in _classes:
        bpy.utils.register_class(c)
    bpy.types.WindowManager.freehk_timl_edit = PointerProperty(
        name="TIML Action", type=bpy.types.Action, poll=lambda self, obj:
        getattr(obj.freehk, "starType", "") == "TIML_Action")
    for name, prop in _SCENE_PROPS:
        setattr(bpy.types.Scene, name, prop)


def unregister():
    for c in reversed(_classes):
        bpy.utils.unregister_class(c)
    try:
        del bpy.types.WindowManager.freehk_timl_edit
    except Exception:
        pass
    for name, _ in _SCENE_PROPS:
        try:
            delattr(bpy.types.Scene, name)
        except Exception:
            pass


if __name__ == "__main__":
    register()
