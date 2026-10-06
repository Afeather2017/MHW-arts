"""Inspect the two actions for the divide_valor_sheath splice task (read-only)."""
import bpy, json, sys

KWS = ["divide_valor", "plw_longsword_100_111"]
EXACT = None  # filled below

def bonename(path):
    if '["' in path:
        return path.split('["')[1].split('"]')[0]
    return None

report = {}
all_actions = []
for act in bpy.data.actions:
    all_actions.append((act.name, tuple(act.frame_range), len(act.fcurves), act.users))

print("### ALL ACTIONS (%d)" % len(all_actions))
for name, fr, nc, users in sorted(all_actions):
    print(f"  {name!r} range={fr[0]:.3f}..{fr[1]:.3f} curves={nc} users={users}")

for act in bpy.data.actions:
    if not any(k in act.name.lower() for k in KWS):
        continue
    fr = act.frame_range
    curves = list(act.fcurves)
    paths = {}
    for fc in curves:
        e = paths.setdefault(fc.data_path, {"idxs": set(), "keys": [], "maxf": None, "minf": None})
        e["idxs"].add(fc.array_index)
        xs = [p.co.x for p in fc.keyframe_points]
        if xs:
            e["maxf"] = max(e["maxf"] or xs[0], max(xs))
            e["minf"] = min(e["minf"] or xs[0], min(xs))
        e["keys"].append(len(fc.keyframe_points))
    allmax = max((e["maxf"] for e in paths.values()), default=0)
    bones = sorted({bonename(p) for p in paths if bonename(p)})
    obj_paths = sorted(p for p in paths if not bonename(p))
    rep = {
        "name": act.name,
        "range": [fr[0], fr[1]],
        "curves": len(curves),
        "groups": len(act.groups),
        "users": act.users,
        "max_key_frame": allmax,
        "n_paths": len(paths),
        "n_bones": len(bones),
        "obj_paths": obj_paths,
        "sample_paths": sorted(paths)[:12],
    }
    # per-curve max-frame histogram (rounded) to spot tail pollution
    hist = {}
    for fc in curves:
        mx = max((p.co.x for p in fc.keyframe_points), default=None)
        if mx is not None:
            hist[round(mx)] = hist.get(round(mx), 0) + 1
    rep["maxframe_hist"] = dict(sorted(hist.items()))
    # curves whose max key is far below action end (suspicious for tail work)
    print("=== ACTION %r" % act.name)
    print(json.dumps(rep, indent=1, default=str))
    # bone set dump
    print("BONES(%d): %s" % (len(bones), ",".join(bones)))
    # which objects use it
    for ob in bpy.data.objects:
        ad = ob.animation_data
        if ad and ad.action and ad.action.name == act.name:
            print("USED-BY-OBJECT: %r (type=%s)" % (ob.name, ob.type))

print("### DONE")
