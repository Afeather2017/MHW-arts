import bpy

tree = next(
    tree
    for tree in bpy.data.node_groups
    if tree.bl_idname == "FreeHKNodeTree" and len(tree.nodes) == 3
)

output_node = next(node for node in tree.nodes if node.bl_idname == "LMTFileNode")
output_node.filepath = (
    "E:/MHW-arts/research/mhw/sakura_test/w03_00_freehk_staging.lmt"
)

area = bpy.context.screen.areas[0]
area.type = "NODE_EDITOR"
space = area.spaces.active
space.tree_type = "FreeHKNodeTree"
space.pin = True
space.node_tree = tree
region = next(region for region in area.regions if region.type == "WINDOW")

override = {
    "window": bpy.context.window,
    "screen": bpy.context.screen,
    "scene": bpy.context.scene,
    "area": area,
    "region": region,
    "space_data": space,
}

print("FREEHK_TREE", tree.name, len(tree.nodes))
print("FREEHK_TETHER", bpy.context.scene.freehk_tether)
print("FREEHK_EXPORT", bpy.ops.freehk.export(override, mode="lmt"))
