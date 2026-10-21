# 拖尾工具验证: BLEND=<备份blend> OUT=<产物lmt> blender -b <blend> --python 本脚本
# 从任意状态 (含无 PMV/轨缺失/已有旧窗) 按"目前的设计"复现五招拖尾配置并导出。
# 设计: 345=刚1 f8-70 | 339=f12-28 | 340=f24-40 | 342=f30-46 | 343无双斩=24-38+58-72
import bpy, os, importlib

DESIGN = {
    345: [(8, 70)],
    339: [(12, 28)],
    340: [(24, 40)],
    342: [(30, 46)],
    343: [(24, 38), (58, 72)],
}

assert bpy.data.node_groups.get("w03_00"), "w03_00 树不存在"
for e, segs in DESIGN.items():
    bpy.ops.freehk.timl_trail_setup(entry_num=e)
    # 先清既有段 (从任意状态归零到设计); 后台模式 op 报 ERROR 会抛 RuntimeError
    for _ in range(8):
        try:
            r = bpy.ops.freehk.timl_trail_seg_del(entry_num=e, seg_index=0)
        except RuntimeError:
            break
        if r != {'FINISHED'}:
            break
    for s, t in segs:
        r = bpy.ops.freehk.timl_trail_seg_add(entry_num=e, frame_start=s, frame_end=t)
        assert r == {'FINISHED'}, "seg_add failed: entry%d %d-%d" % (e, s, t)
    bpy.ops.freehk.timl_trail_list(entry_num=e)

scn = bpy.context.scene
scn.freehk_tether = bpy.data.objects["m_mesh Armature"]
tree = bpy.data.node_groups["w03_00"]
out = [n for n in tree.nodes if n.bl_idname == "LMTFileNode"][0]
OUT = os.environ.get("OUT", r"E:\MHW-arts\timl-editor\_trail_tool_export.lmt")
orig = out.filepath
out.filepath = OUT
eo = importlib.import_module("MHW-FreeKinetics.operators.export_ops")
addon = bpy.context.preferences.addons.get(eo.__package__.split(".")[0])
eo.exportNodeToFile(out, addon.preferences)
out.filepath = orig
print("[validate] exported ->", OUT, os.path.getsize(OUT), "bytes", flush=True)
