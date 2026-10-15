# 参数化无头导出: BLEND=<blend> OUT=<lmt> blender -b --python _export_blend_lmt.py
# 与 GUI 导出按钮同代码路径 (exportNodeToFile)。
import bpy, importlib, os
BLEND = os.environ.get("BLEND", r"E:\MHW-arts\MHW-valor-blender\MHW-Valor-0.0.1.blend")
OUT = os.environ.get("OUT", r"E:\MHW-arts\timl-editor\_export_out.lmt")
bpy.ops.wm.open_mainfile(filepath=BLEND)
scn = bpy.context.scene
scn.freehk_tether = bpy.data.objects["m_mesh Armature"]
tree = bpy.data.node_groups["w03_00"]
out = [n for n in tree.nodes if n.bl_idname == "LMTFileNode"][0]
out.filepath = OUT
eo = importlib.import_module("MHW-FreeKinetics.operators.export_ops")
addon = bpy.context.preferences.addons.get(eo.__package__.split(".")[0])
eo.exportNodeToFile(out, addon.preferences)
print("[export] done ->", OUT, flush=True)
