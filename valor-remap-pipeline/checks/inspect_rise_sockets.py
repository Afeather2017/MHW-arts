"""Inspect MHRS-side weapon binding evidence in the Rise source actions.

Question: does Rise encode weapon binding state like MHW (integer Y-scale on
weapon socket bones), or some other mechanism?
Evidence to gather from the 157 plw_* source actions on the Rise 'Armature':
- which bones exist with 'eapon'/'sheath'/'wa_' in the name
- channels on L_Weapon_00 / R_Weapon_00 (loc/rot/scale), value patterns
- whether scale curves exist anywhere in Rise actions, and on which bones
- integer-valued scale stats (MHW grip states are 1/2/3 integers)
"""
import bpy, re, collections

bpy.ops.wm.open_mainfile(filepath=r"E:\MHW-arts\MHW-Valor.blend")
src = bpy.data.objects.get("Armature")
if not src:
    print("!!! no Rise Armature"); raise SystemExit

print("=== Rise rig bones (weapon/sheath/arm related) ===")
names = [b.name for b in src.data.bones]
hits = [n for n in names if re.search(r"weapon|sheath|sword|arm|hand|hand|wrist", n, re.I)]
print("total bones=%d related=%s" % (len(names), sorted(hits)))

srcs = sorted([a for a in bpy.data.actions
               if a.name.startswith("plw_") and not a.name.endswith("_remap")],
              key=lambda a: a.name)
print("=== source actions: %d ===" % len(srcs))

WP = ("L_Weapon_00", "R_Weapon_00")

def curve_stat(a, bone, prop):
    fc = a.fcurves.find('pose.bones["%s"].%s' % (bone, prop))
    if fc is None:
        return None
    vals = [k.co[1] for k in fc.keyframe_points]
    ints = sum(1 for v in vals if abs(v - round(v)) < 1e-4)
    return (fc.array_index, len(vals), round(min(vals), 4), round(max(vals), 4), ints)

# per-action channel inventory on the two sockets
n_any = 0
scale_actions = []
prop_counter = collections.Counter()
distinct_scale_vals = collections.Counter()
for a in srcs:
    has = False
    for b in WP:
        for prop in ("location", "rotation_quaternion", "rotation_euler", "scale"):
            st = None
            for f in a.fcurves:
                if f.data_path == 'pose.bones["%s"].%s' % (b, prop):
                    st = True
                    prop_counter[(b, prop)] += 1
            if st:
                has = True
                if prop == "scale":
                    if a not in scale_actions:
                        scale_actions.append(a)
                fc = a.fcurves.find('pose.bones["%s"].%s' % (b, prop))
                if fc is not None and prop == "scale":
                    for k in fc.keyframe_points:
                        distinct_scale_vals[round(k.co[1], 3)] += 1
    if has:
        n_any += 1

print("actions with ANY socket-bone channel: %d/%d" % (n_any, len(srcs)))
print("channel inventory (bone,prop) -> #actions: %s" % dict(prop_counter))
print("socket scale distinct values: %s" % dict(sorted(distinct_scale_vals.items())))

# any scale curves anywhere in Rise actions?
scale_bones = collections.Counter()
n_scale_act = 0
for a in srcs:
    seen = False
    for f in a.fcurves:
        m = re.match(r'pose\.bones\["([^"]+)"\]\.scale$', f.data_path)
        if m:
            scale_bones[m.group(1)] += 1
            seen = True
    n_scale_act += int(seen)
print("actions with ANY scale fcurve: %d/%d" % (n_scale_act, len(srcs)))
print("scale fcurve bones: %s" % dict(scale_bones.most_common(12)))

# detailed dump for two sample actions (one with, one without socket channels)
for tag, a in (("WITH-socket", scale_actions[0] if scale_actions else None),
               ("first-src", srcs[0])):
    if a is None:
        continue
    print("=== sample '%s' (%s) ===" % (tag, a.name))
    for b in WP:
        for prop in ("location", "rotation_quaternion", "scale"):
            for f in a.fcurves:
                if f.data_path == 'pose.bones["%s"].%s' % (b, prop):
                    vals = [k.co[1] for k in f.keyframe_points]
                    print("  %s %s[%d] keys=%d %.4f..%.4f" % (b, prop, f.array_index,
                          len(vals), min(vals), max(vals)))
print("DONE")
