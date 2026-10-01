"""READ-ONLY inspection of a pipeline blend. Arg after --: filepath."""
import bpy
import sys
import re

path = sys.argv[sys.argv.index("--") + 1]
bpy.ops.wm.open_mainfile(filepath=path)

print("=== FILE:", bpy.data.filepath)
print("=== scene frame:", bpy.context.scene.frame_start, bpy.context.scene.frame_end)
print("\n=== ARMATURE OBJECTS ===")
for o in bpy.data.objects:
    if o.type == "ARMATURE":
        print(f"  {o.name!r} bones={len(o.data.bones)} anim={o.animation_data.action.name if o.animation_data and o.animation_data.action else None}")

acts = bpy.data.actions
mhgu_src = [a for a in acts if "mhgu_source" in a and not a.name.endswith("_remap")]
remaps = [a for a in acts if a.name.endswith("_remap") and "mhgu_source" in bpy.data.actions.get(a.name[:-len("_remap")], "")]
travel = [a for a in acts if a.name.startswith("TRAVEL::")]
natives = sorted([a.name for a in acts if re.match(r"LMT::w03_00::\d+$", a.name)])
vanillas = [a for a in acts if a.name.endswith("_vanilla")]
divides = [a for a in acts if a.name.startswith(("divide", "left_up", "rigit_up", "right_down"))]
others_remap = [a for a in acts if a.name.endswith("_remap") and a not in remaps]

print(f"\n=== ACTION CENSUS: total={len(acts)}")
print(f"  mhgu_src={len(mhgu_src)} remaps(mhgu)={len(remaps)} travel={len(travel)} natives(w03_00)={len(natives)} vanillas={len(vanillas)} divide-family={len(divides)} other_remaps={len(others_remap)}")
print("  natives:", [n.split("::")[-1] for n in natives])
print("  other_remaps:", sorted(a.name for a in others_remap)[:10])
print("  divide-family:", sorted(a.name for a in divides))

for probe in ("LMT::w07::195_remap", "LMT::w07::196_remap", "LMT::w07::095_remap",
              "LMT::w07::195", "LMT::w07::196", "LMT::w07::095",
              "LMT::w03_00::106", "LMT::w03_00::104", "LMT::w03_00::103"):
    a = acts.get(probe)
    if a:
        fk = a.get("freehk")
        t = fk.get("tetherFrame") if fk else None
        print(f"  present: {probe!r} fc={len(a.fcurves)} range={tuple(round(x,1) for x in a.frame_range)} users={a.users} src={a.get('mhgu_source')}")

print("\n=== NODE TREES ===")
for nt in bpy.data.node_groups:
    print(f"  tree {nt.name!r} nodes={len(nt.nodes)}")
    for n in nt.nodes:
        kinds = []
        if n.inputs:
            kinds.append("in=%d" % len(n.inputs))
        if n.outputs:
            kinds.append("out=%d" % len(n.outputs))
        label = ""
        try:
            if hasattr(n, "action") and n.action:
                label = " action=%s" % n.action.name
        except Exception:
            pass
        print(f"    node {n.name!r} {n.bl_idname} {kinds}{label}")
