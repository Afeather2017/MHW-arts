# -*- coding: utf-8 -*-
"""方向路线切换窗 GUI 工具无头自测 (纯内存, 不保存 blend).
用法: blender -b <blend> --python _test_rdir_switch_window.py
覆盖: 幂等 (已正确配置再跑无变化) / 误配修复 (簇被清+基线早开+连段早开 → 全部归位;
      只改键值不删键 — op 归一化键值, 不重建键位) / 合成 action 新建键路径 /
      操作符路径 / 类别错误分支.
目标: PlMotionInput.150 (107, 档=42) / .153 (110, 档=58) — 当前部署态即正确配置."""
import os, importlib.util
import bpy

ADDON = os.path.join(os.environ.get("APPDATA"), "Blender Foundation", "Blender", "4.3",
                     "scripts", "addons", "MHW-FreeKinetics")
spec = importlib.util.spec_from_file_location("twe", os.path.join(ADDON, "timl_window_editor.py"))
twe = importlib.util.module_from_spec(spec)
spec.loader.exec_module(twe)

MFLAG, MFLAG1 = twe.MFLAG, twe.MFLAG1

# 当前部署正确态 (2026-10-27 切换窗定案): 与 blend 实际键位/键值逐一对应
TARGETS = {
    "TIML::nTimelineParam::PlMotionInput.150": dict(start=42,
        mflag1={0: 0x0, 22: 0x0, 30: 0x0, 42: 0xCFFC30CC, 100: 0xCFFC30CC},
        mflag={0: {0: 0x0, 22: 0x100, 42: 0xC300, 100: 0x300, 124: 0x301},
               1: {0: 0x0, 22: 0x0, 30: 0x0, 42: 0xC003, 100: 0x0, 124: 0x0}}),
    "TIML::nTimelineParam::PlMotionInput.153": dict(start=58,
        mflag1={0: 0x0, 38: 0x0, 58: 0xCFFC30CC},
        mflag={0: {0: 0x0, 38: 0x0, 46: 0x100, 58: 0xC100, 64: 0xC300, 109: 0xC301},
               1: {0: 0x0, 38: 0x0, 46: 0x0, 58: 0xC003, 64: 0xC003, 109: 0xC003}}),
}

fails = []
def check(name, cond, detail=""):
    print(("  ok  " if cond else "  FAIL") + " " + name + ((" | " + detail) if detail and not cond else ""))
    if not cond:
        fails.append(name)

def snap_mflag1(a):
    chans = twe.get_channels(a, MFLAG1, create=False)
    lo = {int(k.co[0]): int(k.co[1]) for k in chans[0].keyframe_points} if chans[0] else {}
    hi = {int(k.co[0]): int(k.co[1]) for k in chans[1].keyframe_points} if chans[1] else {}
    return {f: ((hi.get(f, 0) << 16) | lo.get(f, 0)) for f in set(lo) | set(hi)}

def snap_mflag(a):
    chans = twe.get_channels(a, MFLAG, create=False)
    lo = {int(k.co[0]): int(k.co[1]) for k in chans[0].keyframe_points} if chans[0] else {}
    hi = {int(k.co[0]): int(k.co[1]) for k in chans[1].keyframe_points} if chans[1] else {}
    return {0: lo, 1: hi}

def set_keys(a, tf_hash, ch_lo, ch_hi):
    """按通道重建 (ch_lo/ch_hi = {帧: 16位值}), 严格保留各通道原键位 (含零值键)."""
    for ix, m in ((0, ch_lo), (1, ch_hi)):
        old = a.fcurves.find("FreeHKTiml.m%08X" % tf_hash, index=ix)
        if old is not None:
            a.fcurves.remove(old)
        fc = a.fcurves.new(data_path="FreeHKTiml.m%08X" % tf_hash, index=ix)
        items = sorted(m.items())
        fc.keyframe_points.add(len(items))
        for k, (f, v) in zip(fc.keyframe_points, items):
            k.co = (f, v)
            k.interpolation = 'EXPO'
        fc.update()

# == 1. 幂等: 正确配置上再跑无变化 ==
for name, cfg in TARGETS.items():
    a = bpy.data.actions.get(name)
    assert a is not None, name
    before1, before2 = snap_mflag1(a), snap_mflag(a)
    print("==", name)
    check("初值=部署态", before1 == cfg["mflag1"] and before2 == cfg["mflag"],
          "%s | %s" % (before1, before2))
    twe.recipe_rdir_switch_window(a, cfg["start"])
    check("幂等 (正确配置再跑无变化)",
          snap_mflag1(a) == before1 and snap_mflag(a) == before2,
          "%s | %s" % (snap_mflag1(a), snap_mflag(a)))

# == 2. 误配修复: 只破坏键值 (簇清零/基线早开/连段早开), 键位保持 → op 全部归位 ==
a = bpy.data.actions["TIML::nTimelineParam::PlMotionInput.150"]
good1, good2 = snap_mflag1(a), snap_mflag(a)
set_keys(a, MFLAG1,
         {0: 0x0, 22: 0x1000, 30: 0x1000, 42: 0x0, 100: 0x30CC},
         {0: 0x0, 22: 0x400, 30: 0x400, 42: 0x0, 100: 0xCFFC})   # 基线早开+簇清
set_keys(a, MFLAG,
         {0: 0x0, 22: 0x4100, 42: 0xC300, 100: 0x300, 124: 0x301},
         {0: 0x0, 22: 0x4001, 30: 0x0, 42: 0xC003, 100: 0x0, 124: 0x0})  # 连段早开
print("== 误配修复 (107)")
twe.recipe_rdir_switch_window(a, 42)
check("误配→归位 (mFlag1)", snap_mflag1(a) == good1, str(snap_mflag1(a)))
check("误配→归位 (mFlag)", snap_mflag(a) == good2, str(snap_mflag(a)))

# == 3. 合成 action: 无档键时新建键路径 (mFlag1 插簇键 / mFlag 插连段恢复键) ==
syn = bpy.data.actions.new("TEST::rdir_synthetic")
syn.freehk.starType = "TIML_Action"
syn.freehk.timelineParamRaw = "m6FCCD10E"
set_keys(syn, MFLAG1, {22: 0x1000}, {22: 0x400})    # 仅有早期基线键, 无档键
set_keys(syn, MFLAG, {22: 0x4100}, {22: 0x4001})    # 仅有早期连段键, 无档键
print("== 合成 action (新建键路径)")
twe.recipe_rdir_switch_window(syn, 42)
s1, s2 = snap_mflag1(syn), snap_mflag(syn)
check("合成 mFlag1: 早键清零+档插簇键", s1 == {22: 0x0, 42: 0xCFFC30CC}, str(s1))
check("合成 mFlag: 早键剥三连段+档插恢复键",
      s2[0] == {22: 0x0100, 42: 0x4100} and s2[1] == {22: 0x0000, 42: 0x4001}, str(s2))

# == 4. 操作符路径 (真实 bl_idname 调用, 验证注册) ==
print("== operator path")
set_keys(a, MFLAG1,
         {0: 0x0, 22: 0x1000, 30: 0x1000, 42: 0x0, 100: 0x30CC},
         {0: 0x0, 22: 0x400, 30: 0x400, 42: 0x0, 100: 0xCFFC})
set_keys(a, MFLAG,
         {0: 0x0, 22: 0x4100, 42: 0xC300, 100: 0x300, 124: 0x301},
         {0: 0x0, 22: 0x4001, 30: 0x0, 42: 0xC003, 100: 0x0, 124: 0x0})
bpy.ops.freehk.timl_rdir_switch_window(action_name=a.name, start=42)
check("op 误配→归位", snap_mflag1(a) == good1 and snap_mflag(a) == good2,
      "%s | %s" % (snap_mflag1(a), snap_mflag(a)))

# == 5. 错误分支 ==
print("== error branches")
pc = next(x for x in bpy.data.actions if x.name.startswith("TIML::nTimelineParam::PlMotionCommon"))
try:
    twe.recipe_rdir_switch_window(pc, 0)
    check("非 PI action 报错", False)
except ValueError as e:
    check("非 PI action 报错", "PlMotionInput" in str(e), str(e))

print("RESULT:", "ALL PASS" if not fails else ("FAILED: %s" % fails))
