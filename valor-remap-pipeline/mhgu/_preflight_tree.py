# -*- coding: utf-8 -*-
"""全树预检:解析 w03_00 里每个 LMTActionNode 引用的动作(导出同代码路径)。"""
import bpy, sys
sys.path.append(r"C:\Users\Afeather\AppData\Roaming\Blender Foundation\Blender\4.3\scripts\addons")
import importlib
lmt_exporter = importlib.import_module("MHW-FreeKinetics.blender.lmt_exporter")
ec = importlib.import_module("MHW-FreeKinetics.error_handling.errorController")

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

tree = bpy.data.node_groups["w03_00"]
seen = {}
bad = []
for n in tree.nodes:
    if n.bl_idname != "LMTActionNode":
        continue
    iap = getattr(n, "input_action", None)
    ia = getattr(iap, "name", None) if iap else None
    if not ia or ia in seen:
        continue
    seen[ia] = True
    try:
        a = bpy.data.actions.get(ia)
    except Exception:
        a = None
    if a is None:
        a = next((x for x in bpy.data.actions if x.name == ia), None)
    if a is None:
        bad.append((ia, "action missing"))
        continue
    try:
        eh = ec.ErrorHandler(a, Opt())
        eh.takeOwnership(a)
        p = lmt_exporter.LMTActionParser(a, eh)
        if not p.valid:
            bad.append((ia, "invalid, log=%s" % [e.code for e in getattr(eh, 'log', [])][:6]))
    except ValueError as e:
        bad.append((ia, "ValueError: %s" % e))
    except Exception as e:
        bad.append((ia, "%s: %s" % (type(e).__name__, e)))
print("[preflight] actions parsed:", len(seen), "problems:", len(bad))
for ia, why in bad:
    print("  BAD", ia, "->", why)
