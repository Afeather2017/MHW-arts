# Dump a machine-readable inventory of all actions in the loaded blend:
# name, fcurve count, frame range, users, entry wirings, custom props, content hash.
# Output JSON path taken from --argv.
import bpy, hashlib, json, sys

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
OUT = argv[0] if argv else r"E:\MHW-arts\_inv_dump.json"

tree = bpy.data.node_groups.get("w03_00")
wired = {}
if tree:
    for n in tree.nodes:
        if n.bl_idname != "LMTEntryNode":
            continue
        inp = next((i for i in n.inputs if i.name == "LMT Animation"), None)
        if inp and inp.links:
            an = getattr(getattr(inp.links[0].from_node, "input_action", None), "name", None)
            if an:
                wired.setdefault(an, []).append(n.entryNum)

inv = {}
for a in bpy.data.actions:
    h = hashlib.md5()
    for fc in sorted(a.fcurves, key=lambda f: (f.data_path, f.array_index)):
        h.update(fc.data_path.encode())
        h.update(str(fc.array_index).encode())
        for k in sorted(fc.keyframe_points, key=lambda k: k.co[0]):
            h.update(("%.4f,%.6f;" % (k.co[0], k.co[1])).encode())
    inv[a.name] = dict(
        fcurves=len(a.fcurves),
        fr=[round(a.frame_range[0], 1), round(a.frame_range[1], 1)],
        users=a.users, fake=a.use_fake_user,
        wired=sorted(wired.get(a.name, [])),
        props={k: str(a[k]) for k in a.keys()},
        hash=h.hexdigest(),
    )
json.dump(dict(grade="w03_00", actions=inv), open(OUT, "w", encoding="utf-8"), ensure_ascii=False, indent=0)
print("[dump] actions=%d -> %s" % (len(inv), OUT))
