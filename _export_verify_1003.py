# -*- coding: utf-8 -*-
"""Headless FreeKinetics export of the w03_00 tree to a SCRATCH file for
round-trip verification of the attack-window TIML (does NOT touch the game file)."""
import bpy
import importlib

MASTER = r"E:\MHW-arts\MHW-Valor.blend"
OUT = r"E:\MHW-arts\_blend_export_312final.lmt"

bpy.ops.wm.open_mainfile(filepath=MASTER)
scn = bpy.context.scene
scn.freehk_tether = bpy.data.objects["m_mesh Armature"]
tree = bpy.data.node_groups["w03_00"]
out = [n for n in tree.nodes if n.bl_idname == "LMTFileNode"][0]
out.filepath = OUT
eo = importlib.import_module("MHW-FreeKinetics.operators.export_ops")
addon = bpy.context.preferences.addons.get(eo.__package__.split(".")[0])
eo.exportNodeToFile(out, addon.preferences)
import os
print("[export] exists:", os.path.exists(OUT), "size:", os.path.getsize(OUT) if os.path.exists(OUT) else -1, flush=True)
