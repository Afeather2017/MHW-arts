# -*- coding: utf-8 -*-
import bpy
bpy.ops.wm.open_mainfile(filepath=r"E:\MHW-arts\MHW-Valor.blend")
act = bpy.data.actions["LMT::w07::095_remap"]
maxf = max((kf.co[0] for fc in act.fcurves for kf in fc.keyframe_points), default=0)
print("095_remap: star=%s frameCount=%s maxKF=%g curves=%d tether=%s" %
      (act.freehk.starType, act.freehk.frameCount, maxf, len(act.fcurves),
       act.freehk.tetherFrame.name if act.freehk.tetherFrame else None), flush=True)
for name in sorted(a.name for a in bpy.data.actions if a.name.endswith(".095")):
    a = bpy.data.actions[name]
    if not a.name.startswith("TIML::"):
        continue
    print("== %s param=%s unkn0=%s" % (a.name, a.freehk.timelineParamRaw, a.freehk.unkn0), flush=True)
    for fc in a.fcurves:
        kfs = [(int(kf.co[0]), round(kf.co[1], 3)) for kf in fc.keyframe_points]
        print("   %s [%d] %s" % (fc.data_path, fc.array_index, kfs), flush=True)
