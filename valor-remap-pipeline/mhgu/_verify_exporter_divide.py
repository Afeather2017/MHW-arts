# -*- coding: utf-8 -*-
"""导出器级验证:对动作跑 LMTActionParser(导出的同一代码路径),valid=True 才可接线导出。
用法: blender -b <blend> --python mhgu/_verify_exporter_divide.py -- 动作名1 动作名2 ...
缺省 = divide 批次 5 个。"""
import bpy, sys
sys.path.append(r"C:\Users\Afeather\AppData\Roaming\Blender Foundation\Blender\4.3\scripts\addons")
import importlib
lmt_exporter = importlib.import_module("MHW-FreeKinetics.blender.lmt_exporter")
ec = importlib.import_module("MHW-FreeKinetics.error_handling.errorController")

# harness 补丁:错误日志的 owner 构造在裸跑(无 GUI ownership 链)时会拿到 None,
# 与动作本身无关——导出器对 object 层曲线本来就记 UNEXPORTABLE-omit。
class _EO:
    def tuple(self):
        return ("harness",)
ec.errorOwner = lambda *a, **k: _EO()

class Opt:
    error_text_level = "Verbose"
    error_log_level = "All"
    fcurve_error = "Fix"
    action_error = "Fix"
    graph_error = "Fix"
    export_hidden = True

NAMES = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else [
    "divide_long", "divide_short",
    "left_up_before_divide", "rigit_up_before_divide", "right_down_before_divide"]

ok = True
for nm in NAMES:
    a = bpy.data.actions.get(nm)
    if a is None:
        print("%-26s MISSING" % nm); ok = False; continue
    eh = ec.ErrorHandler(a, Opt())
    eh.takeOwnership(a)
    p = lmt_exporter.LMTActionParser(a, eh)
    trs = getattr(p, "transforms", []) or []
    roots = 0
    for t in trs:
        bid = getattr(t, "boneFunction", getattr(t, "boneId", None))
        if bid is not None and bid < 0:
            roots += 1
    errs = getattr(eh, "log", [])
    print("%-26s valid=%s frameCount=%s transforms=%d rootTracks=%d logged=%d" % (
        nm, p.valid, getattr(p, "frameCount", "?"), len(trs), roots, len(errs)))
    if errs:
        from collections import Counter
        cnt = Counter(e.code for e in errs)
        print("    codes:", dict(cnt))
    if not p.valid:
        ok = False
print("[result]", "ALL VALID" if ok else "FAILURES PRESENT")
