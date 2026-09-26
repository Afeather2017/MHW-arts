"""Quantify: native001 head pos vs ARP-112-end vs migrated-112-end."""
import bpy

def head_pos(path, act_name, frame):
    bpy.ops.wm.open_mainfile(filepath=path)
    ob = bpy.data.objects["m_mesh Armature"]
    a = bpy.data.actions[act_name]
    ob.animation_data_create()
    ob.animation_data.action = a
    bpy.context.scene.frame_set(frame)
    bpy.context.view_layer.update()
    w = ob.matrix_world @ ob.pose.bones["MhBone_004"].matrix
    return w.to_translation()

print("native001  f0  :", tuple(round(v, 3) for v in head_pos(r"E:\MHW-arts\MHW-Valor.blend", "LMT::w03_00::001", 0)))
print("native001  f60 :", tuple(round(v, 3) for v in head_pos(r"E:\MHW-arts\MHW-Valor.blend", "LMT::w03_00::001", 60)))
print("ARP-112    f83 :", tuple(round(v, 3) for v in head_pos(r"E:\MHW-arts\MHW-Valor.backup-20260926-pre-objectloc.blend", "plw_LongSword_100_112_Armature_remap", 83)))
print("MIG-112    f83 :", tuple(round(v, 3) for v in head_pos(r"E:\MHW-arts\MHW-Valor.blend", "plw_LongSword_100_112_Armature_remap", 83)))
print("ARP-112    f0  :", tuple(round(v, 3) for v in head_pos(r"E:\MHW-arts\MHW-Valor.backup-20260926-pre-objectloc.blend", "plw_LongSword_100_112_Armature_remap", 0)))
print("DONE")
