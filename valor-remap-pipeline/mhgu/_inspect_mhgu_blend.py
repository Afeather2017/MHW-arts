"""READ-ONLY inspection of E:\\MHW-arts\\mhgu.blend. NEVER saves."""
import bpy

print("=== FILE:", bpy.data.filepath)
print("=== SCENE frame range:", bpy.context.scene.frame_start, bpy.context.scene.frame_end)

print("\n=== OBJECTS ===")
for o in bpy.data.objects:
    print(f"  {o.name!r} type={o.type} parent={o.parent.name if o.parent else None}"
          f" anim_data={'Y' if o.animation_data else 'N'}")

print("\n=== ARMATURES ===")
for a in bpy.data.armatures:
    print(f"  {a.name!r} bones={len(a.bones)}")

print("\n=== ACTIONS (%d) ===" % len(bpy.data.actions))
for act in sorted(bpy.data.actions, key=lambda a: a.name):
    ad = act.get("mhgu_source", None)
    fk = act.get("freehk", None)
    fkinfo = ""
    if fk:
        fkinfo = f" freehk(frameCount={fk.get('frameCount','?')}, tether={getattr(fk.get('tetherFrame', None), 'name', None)})"
    users = act.users
    print(f"  {act.name!r} users={users} fake={act.use_fake_user} fcurves={len(act.fcurves)}"
          f" range={tuple(round(x,1) for x in act.frame_range)} src={ad}{fkinfo}")

print("\n=== NODE TREES ===")
for nt in bpy.data.node_groups:
    print(f"  {nt.name!r} type={nt.type} nodes={len(nt.nodes)}")

print("\n=== TARGET ACTIONS DETAIL ===")
for want in ("195", "196", "095", "95"):
    for act in bpy.data.actions:
        if want in act.name:
            bones = set()
            props = set()
            for fc in act.fcurves:
                import re
                m = re.match(r'pose\.bones\["([^"]+)"\]\.(\w+)$', fc.data_path)
                if m:
                    bones.add(m.group(1)); props.add(m.group(2))
                else:
                    bones.add("__obj__:" + fc.data_path); props.add(fc.data_path)
            print(f"  --- {act.name!r}: {len(act.fcurves)} fcurves, {len(bones)} bones, props={sorted(props)}")
            print(f"      bones: {sorted(bones)}")
            fk = act.get("freehk")
            if fk:
                print(f"      freehk keys: {list(fk.keys())}")
