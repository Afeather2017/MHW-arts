"""Append the 3 new remap actions from the work file into MASTER (MHW-Valor.blend),
re-bind freehk.tetherFrame to MASTER's own m_mesh Armature (坑⑬: append does not
transport the pointer), set fake users, save.
"""
import bpy
import time
import shutil

WORK = r"E:\MHW-arts\mhgu-valor-mhw.blend"
MASTER = r"E:\MHW-arts\MHW-Valor.blend"
NAMES = ["LMT::w07::195_remap", "LMT::w07::196_remap", "LMT::w07::095_remap"]

bak = MASTER + ".bak-" + time.strftime("%Y%m%d-%H%M%S")
shutil.copyfile(MASTER, bak)
print("[backup] %s" % bak, flush=True)

bpy.ops.wm.open_mainfile(filepath=MASTER)
clash = [n for n in NAMES if n in bpy.data.actions]
if clash:
    print("!!! FAIL: already present in MASTER:", clash)
    print("!!! refusing to duplicate - wire/export the existing actions instead")
    sys.exit(1)

with bpy.data.libraries.load(WORK, link=False) as (data_from, data_to):
    data_to.actions = [n for n in data_from.actions if n in NAMES]

arm = bpy.data.objects.get("m_mesh Armature")
if arm is None:
    print("!!! FAIL: m_mesh Armature missing in MASTER")
    sys.exit(1)

for n in NAMES:
    a = bpy.data.actions.get(n)
    if a is None:
        print("!!! FAIL: append produced nothing for", n)
        sys.exit(1)
    bound = getattr(a.freehk, "tetherFrame", None)
    if bound is None or bound.name != arm.name:
        a.freehk.tetherFrame = arm
    fk = a.get("freehk")
    print("[transfer] %-22s fc=%d range=%s frameCount=%s tether=%s fake=%s" % (
        n, len(a.fcurves), tuple(round(x, 1) for x in a.frame_range),
        fk.get("frameCount") if fk else "?", a.freehk.tetherFrame.name, a.use_fake_user), flush=True)
    a.use_fake_user = True

bpy.ops.wm.save_mainfile()
print("[done] saved:", MASTER, flush=True)
