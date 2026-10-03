"""Migrate the MHRS/MHGU/user actions from MHW-Valor.blend (MASTER) into
MHW-Valor-new.blend (-new, freshly imported clean vanilla LMT + skeleton),
then replicate MASTER's non-vanilla entry wiring.

- copy set   : bone actions present in MASTER but not in -new (vanilla moves
               are NOT copied - -new has the clean fresh import).
- tether     : append does not transport freehk.tetherFrame (坑⑬); rebind to
               -new's own 'm_mesh Armature' for every action that had one.
- wiring     : for every entry MASTER wires to a non-vanilla action, rewire
               the -new tree the same way (single link on the entry input,
               per 坑⑭), and fake_user the replaced vanilla action.

MASTER is only read (append source), never saved.

Reusable for future re-import rounds (defaults preserved):
  NEW_BLEND=/path/to/fresh.blend MASTER_BLEND=/path/to/master.blend \
    blender -b --python _migrate_master_into_new.py
(the FreeKinetics addon must be enabled in user prefs - freehk properties
must be registered; do NOT pass --factory-startup). Idempotent: a second
run copies nothing (copy-set is computed live) and rewires nothing.
"""
import bpy, json, os, shutil, sys, time

MASTER = os.environ.get("MASTER_BLEND", r"E:\MHW-arts\MHW-Valor.blend")
NEW = os.environ.get("NEW_BLEND", r"E:\MHW-arts\MHW-Valor-new.blend")
ARMATURE = "m_mesh Armature"
REPORT = r"E:\MHW-arts\valor-remap-pipeline\mhgu\_migrate_report.json"

report = {}

# ---------- 1) read MASTER ----------
bpy.ops.wm.open_mainfile(filepath=MASTER)

def star(a):
    try: return a.freehk.starType
    except Exception: return None

mas_bone = {}
for a in bpy.data.actions:
    if a.name.startswith("TIML::"):
        continue
    mas_bone[a.name] = {
        "tether": a.freehk.tetherFrame.name if getattr(a.freehk, "tetherFrame", None) else None,
        "star": star(a),
        "fc": len(a.fcurves),
    }

tree = bpy.data.node_groups["w03_00"]
mas_wiring = {}
for n in tree.nodes:
    if n.bl_idname != "LMTEntryNode":
        continue
    inp = next(i for i in n.inputs if i.name == "LMT Animation")
    act = None
    if inp.links:
        src = inp.links[0].from_node
        a = getattr(src, "input_action", None)
        act = a.name if a else None
    mas_wiring[n.entryNum] = act
print("[master] bone actions=%d wired entries=%d" % (len(mas_bone), len(mas_wiring)), flush=True)

# ---------- 2) open -new, backup ----------
bak = NEW + ".bak-" + time.strftime("%Y%m%d-%H%M%S")
shutil.copyfile(NEW, bak)
print("[backup] %s" % bak, flush=True)

bpy.ops.wm.open_mainfile(filepath=NEW)
new_bone = {a.name for a in bpy.data.actions if not a.name.startswith("TIML::")}
copy_names = sorted(set(mas_bone) - new_bone)
report["backup"] = bak
report["copy_count"] = len(copy_names)
report["wiring_before"] = {}
print("[copy-set] %d actions" % len(copy_names), flush=True)

arm = bpy.data.objects.get(ARMATURE)
assert arm is not None, "m_mesh Armature missing in -new"

# ---------- 3) append + rebind ----------
with bpy.data.libraries.load(MASTER, link=False) as (data_from, data_to):
    data_to.actions = [n for n in data_from.actions if n in set(copy_names)]

problems = []
rebound = 0
for name in copy_names:
    a = bpy.data.actions.get(name)
    if a is None:
        problems.append("append produced nothing: " + name)
        continue
    had_tether = mas_bone[name]["tether"]
    if had_tether:
        a.freehk.tetherFrame = arm
        rebound += 1
    a.use_fake_user = True
    if len(a.fcurves) != mas_bone[name]["fc"]:
        problems.append("fcurve count mismatch %s: %d != %d" %
                        (name, len(a.fcurves), mas_bone[name]["fc"]))
report["tether_rebound"] = rebound
print("[append] %d/%d appended, %d tethers rebound" % (len(copy_names) - len(problems), len(copy_names), rebound), flush=True)

# ---------- 4) replicate non-vanilla wiring ----------
tree = bpy.data.node_groups["w03_00"]
rewired, kept = [], []
for entry_num, target in sorted(mas_wiring.items()):
    if target is None or target in new_bone:
        continue  # vanilla wiring - -new already has it fresh
    if target not in bpy.data.actions:
        problems.append("wiring target missing: entry %s -> %s" % (entry_num, target))
        continue
    entry = next(n for n in tree.nodes if n.bl_idname == "LMTEntryNode" and n.entryNum == entry_num)
    inp = next(i for i in entry.inputs if i.name == "LMT Animation")
    old_src = inp.links[0].from_node if inp.links else None
    old_act = getattr(old_src, "input_action", None)
    report["wiring_before"][str(entry_num)] = old_act.name if old_act else None
    node = tree.nodes.new("LMTActionNode")
    node.location = ((old_src.location.x, old_src.location.y - 200) if old_src else (0, 0))
    node.input_action = bpy.data.actions[target]
    for l in list(inp.links):
        tree.links.remove(l)
    tree.links.new(node.outputs[0], inp)
    if old_act is not None:
        old_act.use_fake_user = True   # keep the clean vanilla move alive
    rewired.append((entry_num, target, old_act.name if old_act else None))
report["rewired"] = [{"entry": e, "to": t, "was": o} for e, t, o in rewired]
print("[wiring] rewired %d entries:" % len(rewired), flush=True)
for e, t, o in rewired:
    print("   entry %-4d %s <- %s" % (e, o, t), flush=True)

report["problems"] = problems
if problems:
    print("!!! PROBLEMS:", flush=True)
    for p in problems:
        print("   ", p, flush=True)

# ---------- 5) save ----------
bpy.ops.wm.save_mainfile()
report["saved"] = NEW
json.dump(report, open(REPORT, "w", encoding="utf-8"), ensure_ascii=False, indent=1)
print("[done] saved:", NEW, flush=True)
