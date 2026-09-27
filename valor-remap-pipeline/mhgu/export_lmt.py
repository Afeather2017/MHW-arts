"""Headless FreeKinetics export: writes the w03_00 tree to an LMT file.

Same code path as the GUI export button (exportNodeToFile); the scene's
freehk_tether must be the MHW armature (m_mesh Armature).

  blender -b --python mhgu/export_lmt.py          # exports to E:\MHW-arts\mhgu-export-test.lmt
"""
import bpy, importlib
MASTER = r"E:\MHW-arts\MHW-Valor.blend"
OUT = r"E:\MHW-arts\mhgu-export-test.lmt"
bpy.ops.wm.open_mainfile(filepath=MASTER)
scn = bpy.context.scene
scn.freehk_tether = bpy.data.objects["m_mesh Armature"]
tree = bpy.data.node_groups["w03_00"]
out = [n for n in tree.nodes if n.bl_idname == "LMTFileNode"][0]
out.filepath = OUT
eo = importlib.import_module("MHW-FreeKinetics.operators.export_ops")
addon = bpy.context.preferences.addons.get(eo.__package__.split(".")[0])
eo.exportNodeToFile(out, addon.preferences)
print("[export] done ->", OUT, flush=True)
