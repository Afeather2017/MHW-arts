# 验收(单会话): 应用五类窗口操作 + 立即导出 — 不保存 blend
import bpy, importlib, os
PC = bpy.data.actions["TIML::nTimelineParam::PlMotionCommon.292"]
PI = bpy.data.actions["TIML::nTimelineParam::PlMotionInput.148"]
EC = bpy.data.actions["TIML::nTimelineParam::nWwiseTimeline::EventCollision00.170"]
bpy.context.window_manager.freehk_timl_edit = PC
bpy.ops.freehk.timl_iframe_window(action_name=PC.name, start=0, end=12)     # 无敌 b26+门
bpy.ops.freehk.timl_armor_window(action_name=PC.name, start=20, end=40)     # 霸体 b24
bpy.ops.freehk.timl_guard_window(action_name=PC.name, start=60, end=72)     # 防御 b28
bpy.ops.freehk.timl_attack_window(action_name=EC.name, slot=3, start=50, end=70)  # 攻击窗
bpy.ops.freehk.timl_bit_window(action_name=PI.name, track=0, bit=8, start=10, end=30, on=True)   # 按键开
bpy.ops.freehk.timl_bit_window(action_name=PI.name, track=0, bit=14, start=15, end=25, on=False) # 按键关

scn = bpy.context.scene
scn.freehk_tether = bpy.data.objects["m_mesh Armature"]
tree = bpy.data.node_groups["w03_00"]
out = [n for n in tree.nodes if n.bl_idname == "LMTFileNode"][0]
out.filepath = os.environ.get("OUT", r"E:\MHW-arts\timl-editor\_acceptance.lmt")
eo = importlib.import_module("MHW-FreeKinetics.operators.export_ops")
addon = bpy.context.preferences.addons.get(eo.__package__.split(".")[0])
eo.exportNodeToFile(out, addon.preferences)
print("[accept] exported ->", out.filepath, flush=True)
