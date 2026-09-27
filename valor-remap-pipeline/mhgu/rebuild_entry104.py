"""Deterministic rebuild of entry 104 (刚1): merge from vanilla, re-bake IK,
convert to cm, tag, min-2, export.  The whole 104 pipeline in one script."""
import bpy, importlib
MHGU = r"E:\MHW-arts\mhgu-valor-mhw.blend"
IKB = ("MhBone_247","MhBone_248","MhBone_249","MhBone_250","MhBone_251","MhBone_252","MhBone_253")
TAG = {(247, "location"): 4, (247, "rotation_quaternion"): 6,
       (248, "location"): 4, (248, "rotation_quaternion"): 6,
       (249, "location"): 4, (249, "rotation_quaternion"): 14,
       (250, "location"): 4, (250, "rotation_quaternion"): 14,
       (251, "location"): 4, (251, "rotation_quaternion"): 2,
       (252, "location"): 4, (252, "rotation_quaternion"): 2,
       (253, "location"): 1, (253, "rotation_quaternion"): 2}

# PHASE A: merge from vanilla (PRESERVE=0 -> our baked IK)
src = open(r"E:\MHW-arts\valor-remap-pipeline\mhgu\7_merge_into_native.py", encoding="utf8").read()
exec(compile(src, "7_merge_inline", "exec"))

# PHASE B: re-bake IK (deterministic, metres)
mm = bpy.data.objects["m_mesh Armature"]
scn = bpy.context.scene
view = bpy.context.view_layer
mg = importlib.import_module("MHW-FreeKinetics.blender.tetherOps")
act = bpy.data.actions["LMT::w03_00::104"]
mm.animation_data.action = act
mg.recalculateBodyIKBones(mm, act, [(247, 57, 'FULL'), (248, 40, 'FULL'),
                                     (249, 20, 'FULL'), (250, 16, 'FULL'), (252, 4, 'LOC')])
mg.recalculateGroundIKBones(mm, act)
mm.animation_data.action = None

# PHASE C: (removed - the exporter converts m->cm itself for LIMITS-tagged fcurves)

# PHASE D: tags
nt = 0
for fc in act.fcurves:
    for (bone, prop), buf in TAG.items():
        if fc.data_path == 'pose.bones["MhBone_%03d"].%s' % (bone, prop):
            for m2 in list(fc.modifiers):
                if m2.type == "LIMITS":
                    fc.modifiers.remove(m2)
            l = fc.modifiers.new("LIMITS")
            l.use_restricted_range = True
            l.mute = True
            l.frame_start = l.frame_end = 0
            l.min_x, l.min_y = buf, bone
            nt += 1
print("[tag]", nt, "tagged", flush=True)

# PHASE E: min-2-keys
n2 = 0
for fc in act.fcurves:
    kp = fc.keyframe_points
    if len(kp) == 1:
        f0, v0 = kp[0].co[0], kp[0].co[1]
        kp.add(1)
        kp[1].co = (f0 + 1.0, v0)
        fc.update()
        n2 += 1
print("[min2]", n2, "bumped", flush=True)
bpy.ops.wm.save_mainfile()

# PHASE F: export
tree = bpy.data.node_groups["w03_00"]
out = [x for x in tree.nodes if x.bl_idname == "LMTFileNode"][0]
out.filepath = r"E:\MHW-arts\mhgu-export-test.lmt"
eo = importlib.import_module("MHW-FreeKinetics.operators.export_ops")
addon = bpy.context.preferences.addons.get(eo.__package__.split(".")[0])
eo.exportNodeToFile(out, addon.preferences)
print("[export] done", flush=True)

# PHASE G: verify
lmt2 = importlib.import_module("MHW-FreeKinetics.struct.Lmt").parseLMT(r"E:\MHW-arts\mhgu-export-test.lmt")
a2 = [x for x in lmt2.ActionHeaders if x.id == 104][0]
print("VERIFY entry104 fc=%s tracks=%d" % (getattr(a2, "frameCount", "?"), len(a2.bones)), flush=True)
for b in a2.bones:
    if (b.boneId, b.usage) == (249, 1):
        t = 0
        for kf in b.keyframes[:3]:
            t += kf.frame
            v = getattr(kf, "vec", None)
            print("  249 loc f%d = %s (cm expect ~-38..-46)" % (t, tuple(round(c,1) for c in v[:3]) if v else None), flush=True)
        break
