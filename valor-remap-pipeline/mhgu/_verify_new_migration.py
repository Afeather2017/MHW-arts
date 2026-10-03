"""Verify the MASTER -> -new migration: inventory, wiring, and a full-tree
export whose frameCounts must reflect the migrated moves.
Run: blender -b MHW-Valor-new.blend --python _verify_new_migration.py"""
import bpy, importlib, json

EXPECT_FC = {140: 142, 141: 114, 142: 126, 312: 121, 339: 114, 340: 126,
             358: 12, 359: 12, 360: 12}   # freehk frameCount of wired customs
WIRED = {140: "LMT::w07::192_remap", 141: "divide_short", 142: "divide_long",
         143: "plw_LongSword_100_112_Armature_remap", 307: "plw_LongSword_100_112_Armature_remap",
         312: "LMT::w07::095_remap", 339: "divide_short", 340: "divide_long",
         343: "plw_LongSword_100_112_Armature_remap", 358: "rigit_up_before_divide",
         359: "right_down_before_divide", 360: "left_up_before_divide",
         361: "right_down_before_divide", 362: "right_down_before_divide",
         363: "left_up_before_divide"}
OUT = r"E:\MHW-arts\valor-remap-pipeline\mhgu\_new_migration_export.lmt"

fails = []

# 1) inventory
bone = [a for a in bpy.data.actions if not a.name.startswith("TIML::")]
unbound = [a.name for a in bone
           if getattr(a.freehk, "tetherFrame", None) is None
           and a.name not in ("FK_grip_preview_104", "plw_LongSword_100_002_Armature_remap.001")]
print("[check] bone actions=%d unbound-tether=%s" % (len(bone), unbound))
if unbound:
    fails.append("unbound tethers: %s" % unbound)

# 2) wiring
tree = bpy.data.node_groups["w03_00"]
for entry_num, want in WIRED.items():
    e = next(n for n in tree.nodes if n.bl_idname == "LMTEntryNode" and n.entryNum == entry_num)
    inp = next(i for i in e.inputs if i.name == "LMT Animation")
    ok = len(inp.links) == 1
    if ok:
        a = getattr(inp.links[0].from_node, "input_action", None)
        ok = a is not None and a.name == want
    print("[wiring] entry %-4d %s" % (entry_num, "OK" if ok else "MISMATCH"))
    if not ok:
        fails.append("wiring entry %d != %s" % (entry_num, want))

# 3) export full tree
scn = bpy.context.scene
scn.freehk_tether = bpy.data.objects["m_mesh Armature"]
out_node = [n for n in tree.nodes if n.bl_idname == "LMTFileNode"][0]
out_node.filepath = OUT
eo = importlib.import_module("MHW-FreeKinetics.operators.export_ops")
addon = bpy.context.preferences.addons.get(eo.__package__.split(".")[0])
eo.exportNodeToFile(out_node, addon.preferences)
print("[export] wrote", OUT)

# 4) parse exported LMT
Lmt = importlib.import_module("MHW-FreeKinetics.struct.Lmt")
lmt = Lmt.parseLMT(OUT)
print("[parse] entries=%d" % lmt.Header.entryCount)
hdr = {a.id: a for a in lmt.ActionHeaders}
for entry_num, want in EXPECT_FC.items():
    got = hdr[entry_num].frameCount if entry_num in hdr else None
    ok = got is not None and abs(got - want) <= 2
    print("[frame] entry %-4d frameCount=%-5s expect~%-4d %s" % (entry_num, got, want, "OK" if ok else "FAIL"))
    if not ok:
        fails.append("entry %d frameCount %s != ~%d" % (entry_num, got, want))
# plw-wired entries just need to be present with large counts (not vanilla)
for entry_num in (143, 307, 343):
    got = hdr[entry_num].frameCount if entry_num in hdr else None
    print("[frame] entry %-4d frameCount=%s (plw-wired)" % (entry_num, got))
    if got is None or got < 50:
        fails.append("entry %d plw frameCount suspicious: %s" % (entry_num, got))
# vanilla spot checks (entry 1 idle-ish, entry 86 foresight)
for entry_num in (1, 86):
    print("[frame] entry %-4d frameCount=%s (vanilla)" % (entry_num, hdr.get(entry_num) and hdr[entry_num].frameCount))

print("RESULT: %s" % ("ALL CHECKS PASSED" if not fails else "FAILURES: %s" % fails))
