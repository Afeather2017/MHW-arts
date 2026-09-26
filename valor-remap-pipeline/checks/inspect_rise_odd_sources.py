"""Identify the odd Rise-source actions (no socket channels / MHW-bone scale)."""
import bpy, re

bpy.ops.wm.open_mainfile(filepath=r"E:\MHW-arts\MHW-Valor.blend")
srcs = sorted([a for a in bpy.data.actions
               if a.name.startswith("plw_") and not a.name.endswith("_remap")],
              key=lambda a: a.name)
WP = ("L_Weapon_00", "R_Weapon_00")
no_socket = []
mhw_bones = []
for a in srcs:
    has = any(a.fcurves.find('pose.bones["%s"].location' % b) for b in WP)
    if not has:
        no_socket.append((a.name, len(a.fcurves), tuple(round(v) for v in a.frame_range)))
    if any(f.data_path.startswith('pose.bones["MhBone_') for f in a.fcurves):
        bones = {f.data_path.split('"')[1] for f in a.fcurves if f.data_path.startswith('pose.bones["MhBone_')}
        mhw_bones.append((a.name, len(a.fcurves), len(bones)))

print("sources without socket channels: %s" % no_socket)
print("sources addressing MHW bones: %s" % mhw_bones)

# how many keys do the socket curves actually carry (constants vs animated)?
import collections
pat = collections.Counter()
anim = 0
for a in srcs:
    fc = a.fcurves.find('pose.bones["L_Weapon_00"].location')
    if fc:
        n = len(fc.keyframe_points)
        pat[n] += 1
        vals = [k.co[1] for k in fc.keyframe_points]
        if max(vals) - min(vals) > 1e-4:
            anim += 1
print("L_Weapon_00 location key-count histogram: %s | animated: %d" % (dict(sorted(pat.items())), anim))
print("DONE")
