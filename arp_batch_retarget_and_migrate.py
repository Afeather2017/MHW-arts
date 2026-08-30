"""Run Auto Rig Pro batch retarget, then MHW IK/socket migration.

Configure Auto Rig Pro once (source rig, target rig, bone map, rest pose), then
run this script from Blender. It enables all actions through ARP's own batch
operator and post-processes the resulting ``*_remap`` actions.
"""
import bpy

SOURCE_OBJECT = "Armature"
TARGET_OBJECT = "m_mesh Armature"

src = bpy.data.objects[SOURCE_OBJECT]
dst = bpy.data.objects[TARGET_OBJECT]

# Select the source rig while changing ARP's action-remap list. ARP's toggle
# operator reads the active source armature's action list.
for obj in bpy.context.view_layer.objects:
    obj.select_set(False)
src.select_set(True)
bpy.context.view_layer.objects.active = src

print("Selecting plw_ actions for Auto Rig Pro...")
bpy.ops.arp.remap_disable_all_actions()
for action in sorted(bpy.data.actions, key=lambda a: a.name):
    if action.name.startswith("plw_") and not action.name.endswith("_remap"):
        bpy.ops.arp.toggle_action_remap(action_name=action.name)
# ARP retarget uses the configured source/target pair; make the target active
# for the actual batch operation.
for obj in bpy.context.view_layer.objects:
    obj.select_set(False)
src.select_set(True)
dst.select_set(True)
bpy.context.view_layer.objects.active = dst
print("Running Auto Rig Pro batch retarget...")
# arp.batch_retarget() only opens ARP's selection dialog. The actual loop is
# implemented by arp.retarget when the scene batch flag is enabled.
bpy.context.scene.batch_retarget = True
bpy.context.scene.arp_remap_only_containing = "plw_"
bpy.ops.arp.retarget('EXEC_DEFAULT', frame_start=0, frame_end=1)

# ARP creates target actions ending in _remap. Reuse the postprocessor.
exec(open(r"E:\MHW-arts\batch_migrate_plw_actions.py", encoding="utf-8").read(), globals())
