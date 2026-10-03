import bpy, sys

argv = sys.argv[sys.argv.index("--")+1:]
lmt_path = argv[0]

mod = "MHW-FreeKinetics"
if mod not in (m.__name__ for m in bpy.context.preferences.addons.__iter__()) if False else True:
    try:
        bpy.ops.preferences.addon_enable(module=mod)
    except Exception as e:
        print("ADDON_ENABLE failed:", e)

registered = mod in {m.__name__ for m in __import__("addon_utils").modules()}
enabled = mod in bpy.context.preferences.addons
print("ADDON registered=%s enabled=%s" % (registered, enabled))
if not enabled:
    raise SystemExit(1)

bpy.ops.freehk.import_lmt(filepath=lmt_path)

timl_actions = [a for a in bpy.data.actions if a.name.startswith("TIML::")]
lmt_actions = [a for a in bpy.data.actions if a.freehk.get("starType") == "LMT_Action"
               or (hasattr(a.freehk, "starType") and a.freehk.starType == "LMT_Action")]
print("RESULT timl_actions=%d lmt_actions=%d total_actions=%d" %
      (len(timl_actions), len(lmt_actions), len(bpy.data.actions)))

placeholder = []
for a in timl_actions:
    for f in a.fcurves:
        if f.data_path == "FreeHKTiml.m00000001":
            placeholder.append((a.name, len(f.keyframe_points)))
print("RESULT m00000001_fcurves=%d %s" % (len(placeholder), placeholder[:6]))

raws = {a.freehk.timelineParamRaw for a in timl_actions if a.freehk.get("timelineParamRaw")}
print("RESULT distinct_timelineParamRaw=%d sample=%s" % (len(raws), sorted(raws)[:8]))

# spot-check a known property landed with real keyframes (entry 339 mTrigger etc.)
sample = [f.keyframe_points[0].co for a in timl_actions for f in a.fcurves
          if f.data_path == "FreeHKTiml.m99897C43"][:4]
print("RESULT mTrigger_sample_kfs=%s" % (sample,))
print("BLENDER_IMPORT_OK")
