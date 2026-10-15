# 无头 dump 指定 TIML action 的双通道 fcurve → 合成 u32 键表 (照 ExportTIMLTypesNode.parseBigFlag 语义)
import bpy, sys
names = [a for a in sys.argv[sys.argv.index("--")+1:]] if "--" in sys.argv else []
def join_bigflag(action, path):
    chans = {}
    for f in action.fcurves:
        if f.data_path == path:
            chans[f.array_index] = list(f.keyframe_points)
    frames = sorted({int(k.co[0]) for c in chans.values() for k in c})
    out = []
    pl = pr = 0
    for t in frames:
        l = next((k for k in chans.get(0, []) if int(k.co[0]) == t), None)
        r = next((k for k in chans.get(1, []) if int(k.co[0]) == t), None)
        if r is not None: pr = int(r.co[1])
        if l is not None: pl = int(l.co[1])
        out.append((t, (pl + (pr << 16)) & 0xFFFFFFFF))
    return out
for a in bpy.data.actions:
    if a.name in names or (not names and a.name.endswith(('.345', '.342'))):
        print("== %s (%s) unkn0=%s" % (a.name, a.freehk.timelineParamRaw, a.freehk.unkn0))
        for f in a.fcurves:
            if f.data_path in ("FreeHKTiml.m08FD20A6", "FreeHKTiml.m99897C43",
                               "FreeHKTiml.m6E63FBC7", "FreeHKTiml.mF76AAA7D"):
                keys = join_bigflag(a, f.data_path)
                print("  %-24s %s" % (f.data_path[10:], "  ".join("f%d=%08X" % k for k in keys)))
            else:
                ks = " ".join("f%d=%s" % (int(k.co[0]), ("%.4g" % k.co[1])) for k in f.keyframe_points)
                print("  %-24s [ix%d] %s" % (f.data_path[10:], f.array_index, ks))
