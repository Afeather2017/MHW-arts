"""Batch-postprocess all plw_ animation actions.

Run after Auto-Rig Pro has created the MHW retargeted actions. The script pairs
target actions named ``*_remap`` with source actions by removing that
suffix, then runs the no-UI IK migration and weapon socket migration logic.
"""
import bpy
import importlib

SOURCE_OBJECT = "Armature"          # MHRS
TARGET_OBJECT = "m_mesh Armature"   # MHW
TARGET_SUFFIXES = ("_remap",)

src = bpy.data.objects[SOURCE_OBJECT]
dst = bpy.data.objects[TARGET_OBJECT]
tether = importlib.import_module("MHW-FreeKinetics.blender.tetherOps")

GRIP_Y_SCALE_PATHS = {
    'pose.bones["MhBone_030"].scale',
    'pose.bones["MhBone_047"].scale',
}


def set_grip_y_scale_linear(action):
    """Use straight interpolation for the discrete 030/047 grip states.

    These channels are integer state switches, not animated socket transforms.
    Blender inserts Bezier keys by default, which makes a grip value blend
    between states during a transition.  MHW expects the authored Y-scale
    state to be sampled as a regular straight line, so force LINEAR only on
    array index 1 of the two grip curves.
    """
    for fcurve in action.fcurves:
        if fcurve.data_path in GRIP_Y_SCALE_PATHS and fcurve.array_index == 1:
            for key in fcurve.keyframe_points:
                key.interpolation = 'LINEAR'


def find_source(target_name):
    candidates = [target_name]
    for suffix in TARGET_SUFFIXES:
        if target_name.endswith(suffix):
            candidates.insert(0, target_name[:-len(suffix)])
    # Common ARP naming may add a numeric suffix after the source action name.
    for name in candidates:
        act = bpy.data.actions.get(name)
        if act:
            return act
    prefix = candidates[0]
    matches = [a for a in bpy.data.actions if a.name.startswith(prefix)]
    return sorted(matches, key=lambda a: len(a.name))[0] if matches else None

targets = sorted(
    [a for a in bpy.data.actions
     if a.name.startswith("plw_") and any(a.name.endswith(s) for s in TARGET_SUFFIXES)],
    key=lambda a: a.name
)
if not targets:
    raise RuntimeError("No plw_ target actions ending in _remap found")

scene = bpy.context.scene
old_frame = scene.frame_current
old_src = src.animation_data.action if src.animation_data else None
old_dst = dst.animation_data.action if dst.animation_data else None

for index, target_action in enumerate(targets, 1):
    source_action = find_source(target_action.name)
    if source_action is None:
        print("SKIP (no source action):", target_action.name)
        continue
    src.animation_data_create(); dst.animation_data_create()
    src.animation_data.action = source_action
    dst.animation_data.action = target_action
    print("[%d/%d] %s <- %s" % (index, len(targets), target_action.name, source_action.name))

    # Direct IK bake, independent of the FreeKinetics UI.
    tether.recalculateBodyIKBones(dst, target_action, [
        (247, 57, 'FULL'), (248, 40, 'FULL'), (249, 20, 'FULL'),
        (250, 16, 'FULL'), (252, 4, 'LOC'), (254, 3, 'LOC')])
    tether.recalculateGroundIKBones(dst, target_action, [
        (253, (0.0, 0.0, 0.0)), (251, (0.0, 0.0, 0.0))])

    # Parent-relative weapon socket transfer.
    weapon_map = {
        "L_Weapon_00": ("MhBone_030", "L_Arm_03", "MhBone_008"),
        "R_Weapon_00": ("MhBone_047", "R_Arm_03", "MhBone_012"),
    }
    rest = {n: src.pose.bones[n].bone.matrix_local.copy() for n in weapon_map}
    drest = {v[0]: dst.pose.bones[v[0]].bone.matrix_local.copy() for v in weapon_map.values()}
    prest = {n: src.pose.bones[v[1]].bone.matrix_local.copy() for n, v in weapon_map.items()}
    pdrest = {n: dst.pose.bones[v[2]].bone.matrix_local.copy() for n, v in weapon_map.items()}
    start = int(min(source_action.frame_range[0], target_action.frame_range[0]))
    end = int(max(source_action.frame_range[1], target_action.frame_range[1]))
    for frame in range(start, end + 1):
        scene.frame_set(frame); bpy.context.view_layer.update()
        for sname, (dname, spname, dpname) in weapon_map.items():
            sb, db = src.pose.bones.get(sname), dst.pose.bones.get(dname)
            if not sb or not db: continue
            rel_rest = prest[sname].inverted() @ rest[sname]
            rel_pose = src.pose.bones[spname].matrix.inverted() @ sb.matrix
            delta = rel_rest.inverted() @ rel_pose
            target_rel = pdrest[sname].inverted() @ drest[dname]
            db.matrix = dst.pose.bones[dpname].matrix @ (target_rel @ delta)
            db.rotation_mode = 'QUATERNION'
            db.keyframe_insert("location", frame=frame)
            db.keyframe_insert("rotation_quaternion", frame=frame)
            db.keyframe_insert("scale", frame=frame)

    set_grip_y_scale_linear(target_action)

src.animation_data.action = old_src
dst.animation_data.action = old_dst
scene.frame_set(old_frame)
print("Batch migration complete:", len(targets), "target actions")
