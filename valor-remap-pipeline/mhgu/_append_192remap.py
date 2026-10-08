# 从幸存备份 append 回 LMT::w07::192_remap 并设 fake_user, 然后保存当前文件。
# SRC 通过环境变量传。
import bpy, os, sys

SRC = os.environ["SRC"]
NEED = "LMT::w07::192_remap"

if NEED in bpy.data.actions:
    print("ALREADY-PRESENT", NEED)
else:
    before = set(bpy.data.actions)
    with bpy.data.libraries.load(SRC, link=False) as (data_from, data_to):
        assert NEED in data_from.actions, f"{NEED} not in {SRC}"
        data_to.actions = [NEED]
    new = [a for a in bpy.data.actions if a not in before]
    print("APPENDED:", [a.name for a in new])
    a = bpy.data.actions[NEED]
    a.use_fake_user = True
    print("fake_user =", a.use_fake_user, "fcurves =", len(a.fcurves))
    bpy.ops.wm.save_mainfile()
    print("SAVED", bpy.data.filepath)
