"""Bake MHRS weapon/socket bone motion into the MHW target action.

Run from Blender's Text Editor or Python Console after Auto-Rig Pro retargeting.
Expected objects: source ``Armature`` and target ``m_mesh Armature``.
Expected actions: source action on MHRS and target (retargeted) action on MHW.
"""
import bpy

SOURCE_OBJECT = "Armature"
TARGET_OBJECT = "m_mesh Armature"
WEAPON_MAP = {
    "L_Weapon_00": "MhBone_030",
    "R_Weapon_00": "MhBone_047",
}
PARENT_MAP = {
    "L_Weapon_00": ("L_Arm_03", "MhBone_008"),
    "R_Weapon_00": ("R_Arm_03", "MhBone_012"),
}

src = bpy.data.objects[SOURCE_OBJECT]
dst = bpy.data.objects[TARGET_OBJECT]
if not src.animation_data or not src.animation_data.action:
    raise RuntimeError("MHRS Armature has no active source action")
if not dst.animation_data or not dst.animation_data.action:
    raise RuntimeError("MHW m_mesh Armature has no active target action")

src_action = src.animation_data.action
dst_action = dst.animation_data.action
scene = bpy.context.scene
old_frame = scene.frame_current

# Relative-to-rest transfer avoids assuming that the two armature objects use
# identical object transforms or rest orientations.
src_rest = {n: src.pose.bones[n].bone.matrix_local.copy() for n in WEAPON_MAP}
dst_rest = {n: dst.pose.bones[n].bone.matrix_local.copy() for n in WEAPON_MAP.values()}
src_parent_rest = {n: src.pose.bones[p].bone.matrix_local.copy() for n, (p, _) in PARENT_MAP.items()}
dst_parent_rest = {n: dst.pose.bones[p].bone.matrix_local.copy() for n, (_, p) in PARENT_MAP.items()}

start = int(min(src_action.frame_range[0], dst_action.frame_range[0]))
end = int(max(src_action.frame_range[1], dst_action.frame_range[1]))

# Bake every integer frame in the complete animation range. This is important
# when the weapon bones have sparse/no keys but their hand parents are moving.
frames = set(range(start, end + 1))

for frame in sorted(frames):
    scene.frame_set(frame)
    bpy.context.view_layer.update()
    for sname, dname in WEAPON_MAP.items():
        sb = src.pose.bones.get(sname)
        db = dst.pose.bones.get(dname)
        if sb is None or db is None:
            print("Skipping missing bone", sname, dname)
            continue
        sp, dp = PARENT_MAP[sname]
        sbp, dbp = src.pose.bones[sp], dst.pose.bones[dp]
        # Transfer motion relative to the hand parent. This preserves the
        # socket's actual grip offset even when the two rigs use different axes.
        src_rest_rel = src_parent_rest[sname].inverted() @ src_rest[sname]
        src_pose_rel = sbp.matrix.inverted() @ sb.matrix
        motion_delta = src_rest_rel.inverted() @ src_pose_rel
        dst_rest_rel = dst_parent_rest[sname].inverted() @ dst_rest[dname]
        db.matrix = dbp.matrix @ (dst_rest_rel @ motion_delta)
        db.rotation_mode = 'QUATERNION'
        db.keyframe_insert("location", frame=frame)
        db.keyframe_insert("rotation_quaternion", frame=frame)
        db.keyframe_insert("scale", frame=frame)

scene.frame_set(old_frame)
bpy.context.view_layer.update()
print("Transferred MHRS weapon sockets to MHW:", WEAPON_MAP)
print("Baked frames:", min(frames), "..", max(frames), "count", len(frames))
