import bpy

source = (
    "E:/MHW-arts/research/mhgu/extracted/m001_body/player/mod/m/pl001/"
    "m_body001_out.glb"
)
output = "E:/MHW-arts/research/mhgu/mhgu_hunter_armature.fbx"

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=source)
bpy.ops.export_scene.fbx(
    filepath=output,
    use_selection=False,
    add_leaf_bones=False,
    bake_anim=True,
    bake_anim_use_all_actions=True,
    bake_anim_use_nla_strips=True,
)

print("WROTE", output)
