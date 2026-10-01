"""Final on-disk spot check in MASTER for the 3 new remaps. READ-ONLY."""
import bpy

bpy.ops.wm.open_mainfile(filepath=r"E:\MHW-arts\MHW-Valor.blend")

for n in ("LMT::w07::195_remap", "LMT::w07::196_remap", "LMT::w07::095_remap"):
    a = bpy.data.actions.get(n)
    if a is None:
        print(n, "MISSING")
        continue
    fk = a.get("freehk")
    t = getattr(fk.get("tetherFrame"), "name", None) if fk else None
    def info(path, idx):
        fc = a.fcurves.find(path, index=idx)
        if fc is None:
            return "absent"
        lim = next((m for m in fc.modifiers if m.type == "LIMITS"), None)
        k0 = fc.keyframe_points[0].co if len(fc.keyframe_points) else None
        return "keys=%d k0=%s min_x=%s min_y=%s" % (
            len(fc.keyframe_points),
            (round(k0[0], 1), round(k0[1], 3)) if k0 else None,
            round(lim.min_x, 1) if lim else None,
            round(lim.min_y, 1) if lim else None)
    print("==", n)
    print("   users=%d fake=%s frameCount=%s tether=%s" % (
        a.users, a.use_fake_user, fk.get("frameCount") if fk else "?", t))
    print("   030.scaleY:", info('pose.bones["MhBone_030"].scale', 1))
    print("   047.scaleY:", info('pose.bones["MhBone_047"].scale', 1))
    print("   030.locX  :", info('pose.bones["MhBone_030"].location', 0))
    print("   000.locY  :", info('pose.bones["MhBone_000"].location', 1))
    print("   249.locX  :", info('pose.bones["MhBone_249"].location', 0))
    objs = [fc for fc in a.fcurves if not fc.data_path.startswith("pose.bones[")]
    print("   object fcurves:", [(fc.data_path, fc.array_index, len(fc.keyframe_points)) for fc in objs])
