# -*- coding: utf-8 -*-
"""组合键解锁簇 GUI 工具无头自测 (纯内存, 不保存 blend).
用法: blender -b <blend> --python _test_combo_unlock.py
覆盖: 幂等 / 关簇→基线 / 开簇→参考值还原 / 操作符路径 / 类别与缺轨错误分支."""
import os, importlib.util
import bpy

ADDON = os.path.join(os.environ.get("APPDATA"), "Blender Foundation", "Blender", "4.3",
                     "scripts", "addons", "MHW-FreeKinetics")
spec = importlib.util.spec_from_file_location("twe", os.path.join(ADDON, "timl_window_editor.py"))
twe = importlib.util.module_from_spec(spec)
spec.loader.exec_module(twe)

MFLAG1 = twe.MFLAG1
REF = {  # 参考 w03_00.lmt.r+up_dash 的部署值 (32位合成)
    "TIML::nTimelineParam::PlMotionInput.149": (24, {0: 0x0, 24: 0xC7FC10CC, 44: 0xCFFC10CC,
                                                     54: 0xCFFC30CC, 120: 0xC7FC10CC}),
    "TIML::nTimelineParam::PlMotionInput.153": (38, {0: 0x0, 38: 0xC7FC10CC, 58: 0xCFFC30CC}),
    "TIML::nTimelineParam::PlMotionInput.163": (5, {0: 0x0, 5: 0xC7FC10CC, 14: 0xCFFC30CC, 46: 0x0}),
}
BASE = {  # 关簇后的期望基线 (0 值关窗键保持)
    "TIML::nTimelineParam::PlMotionInput.149": {0: 0x0, 24: 0x04001000, 44: 0x0C001000,
                                                54: 0x0C003000, 120: 0x04001000},
    "TIML::nTimelineParam::PlMotionInput.153": {0: 0x0, 38: 0x04001000, 58: 0x0C003000},
    "TIML::nTimelineParam::PlMotionInput.163": {0: 0x0, 5: 0x04001000, 14: 0x0C003000, 46: 0x0},
}

def snap(action):
    chans = twe.get_channels(action, MFLAG1, create=False)
    lo = {int(k.co[0]): int(k.co[1]) for k in chans[0].keyframe_points} if chans[0] else {}
    hi = {int(k.co[0]): int(k.co[1]) for k in chans[1].keyframe_points} if chans[1] else {}
    return {f: ((hi.get(f, 0) << 16) | lo.get(f, 0)) for f in set(lo) | set(hi)}

fails = []
def check(name, cond, detail=""):
    print(("  ok  " if cond else "  FAIL") + " " + name + ((" | " + detail) if detail and not cond else ""))
    if not cond:
        fails.append(name)

for name, (f0, ref) in REF.items():
    a = bpy.data.actions.get(name)
    assert a is not None, name
    before = snap(a)
    print("==", name)
    check("初值=参考", before == ref, str(before))
    msgs = twe.recipe_combo_unlock(a, f0, on=True)
    print("   [idem]", msgs[0])
    check("幂等 (已开再开无变化)", snap(a) == before and "0 处键" in msgs[0], msgs[0])
    msgs = twe.recipe_combo_unlock(a, f0, on=False)
    print("   [off ]", msgs[0])
    check("关簇→基线", snap(a) == BASE[name], str(snap(a)))
    msgs = twe.recipe_combo_unlock(a, f0, on=True)
    print("   [on  ]", msgs[0])
    check("开簇→参考还原", snap(a) == ref, str(snap(a)))

# 操作符路径 (真实 bl_idname 调用, 验证注册)
print("== operator path")
a = bpy.data.actions["TIML::nTimelineParam::PlMotionInput.149"]
bpy.ops.freehk.timl_combo_unlock(action_name=a.name, start=24, on=False)
check("op 关簇→基线", snap(a) == BASE["TIML::nTimelineParam::PlMotionInput.149"])
bpy.ops.freehk.timl_combo_unlock(action_name=a.name, start=24, on=True)
check("op 开簇→参考", snap(a) == REF["TIML::nTimelineParam::PlMotionInput.149"][1])

# 错误分支
print("== error branches")
pc = next(x for x in bpy.data.actions if x.name.startswith("TIML::nTimelineParam::PlMotionCommon"))
try:
    twe.recipe_combo_unlock(pc, 0, True)
    check("非 PI action 报错", False)
except ValueError as e:
    check("非 PI action 报错", "PlMotionInput" in str(e), str(e))

print("RESULT:", "ALL PASS" if not fails else ("FAILED: %s" % fails))
