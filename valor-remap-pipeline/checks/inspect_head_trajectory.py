"""Ground truth: world trajectories of the head bone while playing 112 in
(a) ARP-state backup (root values on MhBone_000 bone-local space),
(b) migrated state (travel on object curves),
(c) the Rise source (Root bone).
Plus: where do MhBone_000 / Rise Root bone-local axes actually point?
"""
import bpy, mathutils

BACKUP = r"E:\MHW-arts\MHW-Valor.backup-20260926-pre-objectloc.blend"
CURRENT = r"E:\MHW-arts\MHW-Valor.blend"

def axis_report(dst, src_ob):
    b = dst.data.bones["MhBone_000"]
    m = b.matrix_local.to_3x3()
    print("  MhBone_000 rest axes (local -> armature):")
    for i, name in enumerate(("X", "Y", "Z")):
        v = m @ mathutils.Vector((1.0 if i == 0 else 0, 1.0 if i == 1 else 0, 1.0 if i == 2 else 0))
        print("    local %s -> (%.3f, %.3f, %.3f)" % (name, v.x, v.y, v.z))
    r = src_ob.data.bones["Root"]
    m2 = r.matrix_local.to_3x3()
    print("  Rise Root rest axes (local -> armature):")
    for i, name in enumerate(("X", "Y", "Z")):
        v = m2 @ mathutils.Vector((1.0 if i == 0 else 0, 1.0 if i == 1 else 0, 1.0 if i == 2 else 0))
        print("    local %s -> (%.3f, %.3f, %.3f)" % (name, v.x, v.y, v.z))

def trajectory(path, obj_name, action_name, bone, n=8):
    bpy.ops.wm.open_mainfile(filepath=path)
    ob = bpy.data.objects[obj_name]
    scene = bpy.context.scene
    view = bpy.context.view_layer
    a = bpy.data.actions[action_name]
    lo, hi = a.frame_range
    ob.animation_data_create()
    ob.animation_data.action = a
    pb = ob.pose.bones[bone]
    print("  '%s' bone %s over %.0f..%.0f (world x,y,z):" % (action_name, bone, lo, hi))
    for i in range(n):
        t = lo + (hi - lo) * i / (n - 1)
        scene.frame_set(round(t))
        view.update()
        w = ob.matrix_world @ pb.matrix
        print("    f%-4.0f (%+.3f, %+.3f, %.3f)" % (t, w.to_translation().x, w.to_translation().y, w.to_translation().z))

print("=== axis orientations (current blend) ===")
bpy.ops.wm.open_mainfile(filepath=CURRENT)
axis_report(bpy.data.objects["m_mesh Armature"], bpy.data.objects["Armature"])

print("=== (c) Rise source 112: head trajectory ===")
# find a head-ish bone on the rise rig
bpy.ops.wm.open_mainfile(filepath=CURRENT)
names = [b.name for b in bpy.data.objects["Armature"].data.bones]
head = next((n for n in names if "head" in n.lower()), None)
print("  rise head bone:", head)

print("=== (a) ARP-state 112 remap: MhBone_004 world trajectory ===")
trajectory(BACKUP, "m_mesh Armature", "plw_LongSword_100_112_Armature_remap", "MhBone_004")

print("=== (b) migrated 112: MhBone_004 world trajectory ===")
trajectory(CURRENT, "m_mesh Armature", "plw_LongSword_100_112_Armature_remap", "MhBone_004")
print("DONE")
