# -*- coding: utf-8 -*-
# 只读导出当前 blend 的 LMT (不做任何修改), 用作修复前基线。
# 用法: OUT=... blender -b MHW-Valor.blend --python _export_current_blend.py
import bpy, os, importlib
OUT = os.environ.get("OUT", r"E:\MHW-arts\_prefix_export.lmt")
tree = bpy.data.node_groups["w03_00"]
scn = bpy.context.scene
scn.freehk_tether = bpy.data.objects["m_mesh Armature"]
fnode = [n for n in tree.nodes if n.bl_idname == "LMTFileNode"][0]
fnode.filepath = OUT
eo = importlib.import_module("MHW-FreeKinetics.operators.export_ops")
addon = bpy.context.preferences.addons.get(eo.__package__.split(".")[0])
eo.exportNodeToFile(fnode, addon.preferences)
print("[export] OK -> " + OUT, flush=True)
