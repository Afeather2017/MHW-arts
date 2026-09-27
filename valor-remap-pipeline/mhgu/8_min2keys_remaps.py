"""MHGU chain STEP 8 - bump every MHGU remap's 1-key constants to 2 keys.

bounds() in the exporter drops the first keyframe, so single-key constant
transforms (the 106-fill fingers/sockets) crash the export.  After this pass
every MHGU remap can be wired directly into an entry node and exported.
Idempotent.  Run:
  blender -b --python mhgu/8_min2keys_remaps.py
"""
import bpy, os, sys, shutil, time

WORK = os.environ.get("MHGU_WORK_BLEND", r"E:\MHW-arts\mhgu-valor-mhw.blend")
bak = WORK + ".bak-" + time.strftime("%Y%m%d-%H%M%S")
shutil.copyfile(WORK, bak)
print("[backup] %s" % bak, flush=True)
bpy.ops.wm.open_mainfile(filepath=WORK)

done = []
for a in sorted([x for x in bpy.data.actions
                 if x.name.endswith("_remap") and "mhgu_source" in x], key=lambda x: x.name):
    n = 0
    for fc in a.fcurves:
        kp = fc.keyframe_points
        if len(kp) == 1:
            f0, v0 = kp[0].co[0], kp[0].co[1]
            k = len(kp)
            kp.add(1)
            kp[k].co = (f0 + 1.0, v0)
            kp[k].interpolation = kp[0].interpolation
            kp.sort()
            fc.update()
            n += 1
    a.use_fake_user = True
    done.append((a.name, n))
total = sum(n for _, n in done)
print("[min2] %d remaps, %d single-key curves bumped" % (len(done), total), flush=True)
for name, n in done:
    if n:
        print("   %-40s +%d" % (name, n), flush=True)
bpy.ops.wm.save_mainfile()
print("[done] saved:", WORK, flush=True)
