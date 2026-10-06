"""Diff LIMITS tags (min_x/min_y) on divide_valor_sheath curves: backup (pre-splice) vs current."""
import bpy

def tagsnapshot(act):
    snap = {}
    for f in act.fcurves:
        for m in f.modifiers:
            if m.type == "LIMITS":
                snap[(f.data_path, f.array_index)] = (round(m.min_x, 3), round(m.min_y, 3),
                                                      m.use_restricted_range, m.mute)
                break
    return snap

CUR = bpy.data.actions["divide_valor_sheath"]
cur = tagsnapshot(CUR)

with bpy.data.libraries.load(r"E:\MHW-arts\MHW-Valor.blend.bak-20261007-020924", assets_only=True) as (src, _):
    pass

# load the backup action under a temp library link to compare tags
# simpler: parse the .blend via a second scene is heavy; instead open the backup
# action by link:
with bpy.data.libraries.load(r"E:\MHW-arts\MHW-Valor.blend.bak-20261007-020924",
                             link=True, assets_only=True) as (src, dst):
    dst.actions = [n for n in src.actions if n == "divide_valor_sheath"]

old_act = bpy.data.actions.get("divide_valor_sheath")
# linked data may arrive with suffix; find the linked one
if old_act is None or old_act.library is None:
    for a in bpy.data.actions:
        if a.library is not None and a.name.startswith("divide_valor_sheath"):
            old_act = a
            break
old = tagsnapshot(old_act)
print("cur tags=%d old tags=%d" % (len(cur), len(old)))
diffs = 0
for key in sorted(set(old) | set(cur)):
    o, c = old.get(key), cur.get(key)
    if o != c:
        diffs += 1
        print("DIFF %s[%d]: old=%s new=%s" % (key[0][:48], key[1], o, c))
print("total tag diffs: %d" % diffs)
print("### DONE")
