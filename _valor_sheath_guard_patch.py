# -*- coding: utf-8 -*-
"""valor_sheath (entry 306) 防御帧切换: b26 全向无敌 -> bit28 角度格挡.
原位修补已接线的 TIML action (不重建动作, 节点连线不动):
  mFlag (m08FD20A6) f0 = 0x12400006  (bit28 格挡 + b25/b22/0x6 基线,
      = 2026-09-13 运行时实锤值 306184198, 正面格挡/背面受击)
  删除 mTrigger (m99897C43) 轨 (2048 是 b26 无敌窗的门, bit28 不需要;
      且 b26 与 bit28 互斥, b26 赢则格挡不运行)
Run: blender -b MHW-Valor.blend --python _valor_sheath_guard_patch.py
"""
import shutil
import sys
import time

import bpy

BLEND = r"E:\MHW-arts\MHW-Valor.blend"
TIML_NAME = "TIML::nTimelineParam::PlMotionCommon"
FLAG_HASH = "FreeHKTiml.m08FD20A6"   # PlMotionCommon::mFlag
TRIG_HASH = "FreeHKTiml.m99897C43"   # PlMotionCommon::mTrigger
GUARD_VALUE = 0x12400000 | 0x00000006  # bit28 | b25 | b22 | 0x6

bak = BLEND + ".bak-" + time.strftime("%Y%m%d-%H%M%S")
shutil.copyfile(BLEND, bak)
print("[backup] %s" % bak, flush=True)

bpy.ops.wm.open_mainfile(filepath=BLEND)

timl = bpy.data.actions.get(TIML_NAME)
assert timl is not None, "TIML action missing"
n_before = len(timl.fcurves)

# mFlag: 单键 f0 = 0x12400006 (整段常开)
lo = GUARD_VALUE & 0xFFFF
hi = (GUARD_VALUE >> 16) & 0xFFFF
for idx, val in ((0, lo), (1, hi)):
    fc = timl.fcurves.find(FLAG_HASH, index=idx)
    if fc is None:
        fc = timl.fcurves.new(data_path=FLAG_HASH, index=idx)
    kp = fc.keyframe_points
    kp.clear()
    kp.add(1)
    kp[0].co = (0.0, float(val))
    kp[0].interpolation = 'CONSTANT'
    fc.update()

# mTrigger 轨删除 (b26 配方遗留)
removed = 0
for fc in list(timl.fcurves):
    if fc.data_path == TRIG_HASH:
        timl.fcurves.remove(fc)
        removed += 1
timl.use_fake_user = True
print("[patch] mFlag f0=%#010x (low=%d high=%d), mTrigger curves removed=%d, fcurves %d->%d" % (
    GUARD_VALUE, lo, hi, removed, n_before, len(timl.fcurves)), flush=True)

# ---- 验证: 导出器回读 ----
sys.path.append(r"C:\Users\Afeather\AppData\Roaming\Blender Foundation\Blender\4.3\scripts\addons")
import importlib
ec = importlib.import_module("MHW-FreeKinetics.error_handling.errorController")
fh = importlib.import_module("MHW-FreeKinetics.blender.nodetree.freeHKActionNodes")


class Opt:
    error_text_level = "Verbose"
    error_log_level = "All"
    fcurve_error = "Fix"
    action_error = "Fix"
    graph_error = "Fix"
    export_hidden = True


class _EO:
    def tuple(self):
        return ("harness",)


ec.errorOwner = lambda *a, **k: _EO()
teh = ec.ErrorHandler(timl, Opt())
teh.takeOwnership(timl)
struct = fh.ExportTIMLTypesNode(timl, teh).structure()
got = {}
for t in struct.transforms:
    for kf in t.keyframes:
        got["%08X" % t.datatypeHash] = int(kf.value)
print("[verify] TIML transforms=%d %s" % (struct.count, got), flush=True)
ok = True
if got.get("08FD20A6") != GUARD_VALUE:
    print("    !!! mFlag = %s != %#010x" % (got.get("08FD20A6"), GUARD_VALUE)); ok = False
if "99897C43" in got:
    print("    !!! mTrigger 仍在"); ok = False

# 接线确认(只读, 不动)
tree = bpy.data.node_groups.get("w03_00")
for n in tree.nodes:
    ia = getattr(n, "input_action", None)
    if ia is not None and ia.name == TIML_NAME:
        linked = [l.to_node.name for o in n.outputs for l in o.links]
        print("[wiring] %s -> %s linked=%s" % (n.name, ia.name, linked or "none"), flush=True)

print("[result] %s" % ("ALL GREEN" if ok else "FAILURES — NOT SAVING"), flush=True)
if not ok:
    sys.exit(1)

bpy.ops.wm.save_mainfile()
print("[done] saved:", BLEND, flush=True)
