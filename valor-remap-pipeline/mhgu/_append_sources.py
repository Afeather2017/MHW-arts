"""Append LMT::w07::195/196/095 from mhgu.blend into the work file.

mhgu.blend is opened ONLY as an append source (library load) - never opened
as mainfile, never saved: every action in it is 0-user and a save would
purge them (user warning 2026-10-01).

mhgu_source convention mirrors 0_assemble: "w07.lmt#<id>".
"""
import bpy
import os
import sys
import time

WORK = os.environ.get("MHGU_WORK_BLEND", r"E:\MHW-arts\mhgu-valor-mhw.blend")
SOURCE = r"E:\MHW-arts\mhgu.blend"
NAMES = ["LMT::w07::195", "LMT::w07::196", "LMT::w07::095"]

bak = WORK + ".bak-" + time.strftime("%Y%m%d-%H%M%S")
import shutil
shutil.copyfile(WORK, bak)
print("[backup] %s" % bak, flush=True)

bpy.ops.wm.open_mainfile(filepath=WORK)

with bpy.data.libraries.load(SOURCE, link=False) as (data_from, data_to):
    have = [n for n in data_from.actions if n in NAMES]
    missing = [n for n in NAMES if n not in data_from.actions]
    if missing:
        print("!!! FAIL: not found in source:", missing)
        sys.exit(1)
    data_to.actions = have

for n in NAMES:
    a = bpy.data.actions.get(n)
    if a is None:
        print("!!! FAIL: append produced nothing for", n)
        sys.exit(1)
    a["mhgu_source"] = "w07.lmt#" + n.split("::")[-1]
    a.use_fake_user = True
    fk = a.get("freehk")
    fkinfo = {k: (v if not hasattr(v, "name") else v.name) for k, v in fk.items()} if fk else None
    print("[append] %-16s fcurves=%d range=%s freehk=%s src=%s" % (
        n, len(a.fcurves), tuple(round(x, 1) for x in a.frame_range), fkinfo, a.get("mhgu_source")), flush=True)

bpy.ops.wm.save_mainfile()
print("[done] saved:", WORK, flush=True)
