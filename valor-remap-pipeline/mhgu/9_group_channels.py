"""MHGU chain STEP 9 - group every fcurve under its bone's ActionGroup.

Blender's own convention: pose-bone channels live in an ActionGroup named
exactly after the bone ("MhBone_047"), so the dopesheet collapses each bone's
location/rotation/scale components into one editable row.  Curves rebuilt by
7_merge (fcurves.new) come out UNGROUPED, which makes hand-editing miserable.

Object-level curves (root travel) go into an "Object" group.

Idempotent; backs the file up first.  Processes every action in every file.

  blender -b --python mhgu/9_group_channels.py                 # both project files
  GROUP_BLENDS="path1;path2" blender -b --python mhgu/9_group_channels.py
"""
import bpy, os, time, shutil, re, sys

DEFAULT = r"E:\MHW-arts\mhgu-valor-mhw.blend;" + r"E:\MHW-arts\MHW-Valor.blend"
FILES = [p for p in os.environ.get("GROUP_BLENDS", DEFAULT).split(";") if p.strip()]
BONE_RE = re.compile(r'^pose\.bones\["([^"]+)"\]\.')


def group_name(data_path):
    m = BONE_RE.match(data_path)
    return m.group(1) if m else "Object"


def regroup_action(act):
    if not len(act.fcurves):
        return 0, 0
    groups = {g.name: g for g in act.groups}
    moved = kept = 0
    for fc in act.fcurves:
        name = group_name(fc.data_path)
        g = groups.get(name)
        if g is None:
            g = act.groups.new(name)
            groups[name] = g
        if fc.group != g:
            fc.group = g
            moved += 1
        else:
            kept += 1
    return moved, kept


failures = []
for path in FILES:
    if not os.path.isfile(path):
        failures.append((path, "missing"))
        continue
    bak = path + ".bak-" + time.strftime("%Y%m%d-%H%M%S")
    shutil.copyfile(path, bak)
    print("[backup] %s" % bak, flush=True)
    bpy.ops.wm.open_mainfile(filepath=path)
    t_moved = t_kept = t_acts = 0
    for act in bpy.data.actions:
        moved, kept = regroup_action(act)
        if moved:
            t_acts += 1
        t_moved += moved
        t_kept += kept
    # verify: every pose-bone fcurve grouped, every group named after a bone
    bad = []
    for act in bpy.data.actions:
        for fc in act.fcurves:
            want = group_name(fc.data_path)
            if fc.group is None or fc.group.name != want:
                bad.append((act.name, fc.data_path))
    print("[group] %s: %d fcurves regrouped, %d already ok, %d actions touched" %
          (os.path.basename(path), t_moved, t_kept, t_acts), flush=True)
    if bad:
        print("!!! ungrouped/mismatched remain: %s" % bad[:8], flush=True)
        failures.append((path, "%d mismatches" % len(bad)))
        continue
    bpy.ops.wm.save_mainfile()
    print("[done] saved: %s" % path, flush=True)

if failures:
    print("!!! FAILURES: %s" % failures, flush=True)
    sys.exit(1)
print("ALL FILES DONE", flush=True)
