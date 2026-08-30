"""Bake MHW IK/anchor bones without using the FreeKinetics UI.

Run after Auto-Rig Pro retargeting, with the target action active on
``m_mesh Armature``. The script imports the already-loaded FreeKinetics module
via importlib (its package name contains a hyphen).
"""
import bpy
import importlib

TARGET_OBJECT = "m_mesh Armature"

arm = bpy.data.objects.get(TARGET_OBJECT)
if arm is None or arm.type != 'ARMATURE':
    raise RuntimeError("Target armature not found: %s" % TARGET_OBJECT)
if not arm.animation_data or not arm.animation_data.action:
    raise RuntimeError("No active action on %s" % TARGET_OBJECT)

action = arm.animation_data.action
tether = importlib.import_module("MHW-FreeKinetics.blender.tetherOps")

# Body IK follows deform bones.  These mappings are the modern MHW humanoid
# mapping used by FreeKinetics, but are passed explicitly so this script does
# not depend on UI state or checkbox values.
body_map = [
    (247, 57, 'FULL'),  # right hand IK -> right palm
    (248, 40, 'FULL'),  # left hand IK  -> left palm
    (249, 20, 'FULL'),  # right foot IK -> right ankle
    (250, 16, 'FULL'),  # left foot IK  -> left ankle
    (252, 4,  'LOC'),   # head IK -> head position
    (254, 3,  'LOC'),   # neck IK -> neck position
]

# Do body IK first. This creates animation curves for 247/248/249/250/252/254.
tether.recalculateBodyIKBones(arm, action, body_map)

# 251 and 253 have no deform target. Pin them to world-space anchor positions.
# The default is the world origin; adjust these offsets if your MHW rig's ground
# or upper-body anchor is intentionally displaced.
ground_map = [
    (253, (0.0, 0.0, 0.0)),  # ground anchor
    (251, (0.0, 0.0, 0.0)),  # upper-body anchor
]
tether.recalculateGroundIKBones(arm, action, ground_map)

# Add FreeKinetics bone-function metadata to generated curves when available.
try:
    tether._tagIKBoneFunctions(action, [
        arm.pose.bones[n] for n in (
            "MhBone_247", "MhBone_248", "MhBone_249", "MhBone_250",
            "MhBone_251", "MhBone_252", "MhBone_253", "MhBone_254")
        if n in arm.pose.bones
    ])
except Exception as exc:
    print("Warning: could not tag IK curves:", exc)

print("IK migration complete:", action.name)
for fid in (247, 248, 249, 250, 251, 252, 253, 254):
    name = "MhBone_%03d" % fid
    curves = [fc for fc in action.fcurves if ('pose.bones["%s"]' % name) in fc.data_path]
    print(name, "curves:", len(curves), "keys:", sum(len(fc.keyframe_points) for fc in curves))

