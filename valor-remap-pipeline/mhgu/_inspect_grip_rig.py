import bpy, sys

class _Tee:
    def __init__(self):
        self.f = open(r"E:\MHW-arts\valor-remap-pipeline\mhgu\_grip_rig_report.txt", "w", encoding="utf-8")
    def write(self, s):
        sys.__stdout__.write(s)
        self.f.write(s)
    def flush(self):
        self.f.flush()
sys.stdout = _Tee()

print("=== swo022 Armature DETAIL ===")
swo = bpy.data.objects.get('swo022 Armature')
if swo:
    print("type:", swo.type, "anim action:", swo.animation_data and swo.animation_data.action and swo.animation_data.action.name)
    ad = swo.animation_data
    if ad:
        for tr in ad.nla_tracks:
            print("  NLA track:", tr.name, "mute:", tr.mute)
            for st in tr.strips:
                print("    strip:", st.action.name if st.action else None, st.frame_start, st.frame_end)
        if ad.drivers:
            for d in ad.drivers:
                print("  DRIVER:", d.data_path, d.driver.expression)
    for b in swo.data.bones:
        print(f"  BONE {b.name!r} parent={b.parent.name if b.parent else None} inherit_scale={getattr(b,'inherit_scale','?')}")
    for pb in swo.pose.bones:
        for con in pb.constraints:
            tgt = getattr(con, 'target', None)
            print(f"  PBCON bone={pb.name} con={con.name} type={con.type} "
                  f"target={tgt and tgt.name} subtarget={getattr(con,'subtarget',None)} "
                  f"influence={con.influence} mute={con.mute} show_exp={getattr(con,'show_expanded',None)}")
            for attr in ('target_space','owner_space','mix_mode','use_offset','head_tail','use_bbone_shape'):
                if hasattr(con, attr):
                    print(f"     {attr}={getattr(con,attr)}")

print("\n=== m_mesh Armature sockets ===")
mm = bpy.data.objects.get('m_mesh Armature')
if mm:
    print("anim action:", mm.animation_data and mm.animation_data.action and mm.animation_data.action.name)
    for name in ('MhBone_000','MhBone_030','MhBone_047','MhBone_008','MhBone_106'):
        pb = mm.pose.bones.get(name)
        b = mm.data.bones.get(name)
        if b:
            print(f"  {name}: parent={b.parent.name if b.parent else None} "
                  f"rest_head={tuple(round(v,4) for v in b.head_local)} rest_tail={tuple(round(v,4) for v in b.tail_local)} "
                  f"rest_matrix_ok")
            if pb:
                print(f"     pose loc={tuple(round(v,4) for v in pb.location)} "
                      f"rot={tuple(round(v,3) for v in pb.rotation_quaternion)} "
                      f"scl={tuple(round(v,3) for v in pb.scale)}")
                for con in pb.constraints:
                    print(f"     constraint {con.name} {con.type}")

print("\n=== weapon mesh ===")
wm = bpy.data.objects.get('Group_0_Sub_0__Ch_Wp_Mt__5')
if wm:
    for mod in wm.modifiers:
        print("  modifier:", mod.name, mod.type, getattr(mod,'object',None) and mod.object.name)
    print("  parent:", wm.parent and wm.parent.name, "parent_type:", wm.parent_type, "parent_bone:", wm.parent_bone)

print("\n=== ALL ACTIONS ===")
for act in sorted(bpy.data.actions, key=lambda a: a.name):
    paths = {fc.data_path for fc in act.fcurves}
    who = "?"
    if any('swo' in p for p in paths):
        pass
    # detect armature by bone name set
    print(f"ACT {act.name!r} frames=[{act.frame_range[0]},{act.frame_range[1]}] curves={len(act.fcurves)} users={act.users}")

print("\n=== LMT actions with 030/047 scale ===")
for act in bpy.data.actions:
    if not act.name.startswith('LMT::'):
        continue
    rows = []
    for fc in act.fcurves:
        if ('MhBone_030' in fc.data_path or 'MhBone_047' in fc.data_path) and 'scale' in fc.data_path:
            ks = [(round(kp.co[0],2), round(kp.co[1],3)) for kp in fc.keyframe_points]
            # limit/modify info
            lim = None
            for m in fc.modifiers:
                if m.type == 'LIMITS':
                    lim = (round(m.min_x,2), round(m.max_x,2))
            rows.append((fc.data_path.split('"')[1], fc.array_index, ks[:8], len(ks), lim))
    if rows:
        print(f"ACT {act.name!r}")
        for bn, ai, ks, n, lim in sorted(rows):
            print(f"   {bn}.scale[{ai}] n={n} lim={lim} keys={ks}")

print("\n=== scene ===")
sc = bpy.context.scene
print("fps:", sc.render.fps, "range:", sc.frame_start, sc.frame_end, "current frame:", sc.frame_current)
print("DONE")
