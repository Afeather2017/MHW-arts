"""MHGU chain STEP 0 - assemble the working blend for the MHGU->MHW transfer.

Creates E:\\MHW-arts\\mhgu-valor-mhw.blend as a copy of the verified
MHW-Valor.blend, then:
- deletes every plw_* action (the MHRS remap set lives on in MHW-Valor.blend;
  the working file must contain ONLY MHGU remaps so the unmodified pipeline
  step filters keep working)
- appends the MHGU armature 'Animation Group' + its source actions from
  mhgu-valor.blend, keeping ORIGINAL names (LMT::w07::NNN,
  Motion[N]_Animation Group); step 1 adds the exact 60fps LMT imports as
  LMT::w07_sa::NNN. Remaps get the ARP-standard <original>_remap name.

Run:
  /e/MHW-arts/tools/blender-4.3.2-windows-x64/blender.exe -b --python mhgu/0_assemble_mhgu_blend.py
"""
import bpy, os, re, shutil, sys, time

MASTER = r"E:\MHW-arts\MHW-Valor.blend"
OUT = os.environ.get("MHGU_WORK_BLEND", r"E:\MHW-arts\mhgu-valor-mhw.blend")
SRC_BLEND = r"E:\MHW-arts\mhgu-valor.blend"
SRC_RIG = "Animation Group"
KEEP_RE = re.compile(r"^(LMT::w07::\d+|Motion\[\d+\]_Animation Group)$")   # skips .001/.002 re-import dups

if os.path.exists(OUT) and os.environ.get("FORCE") != "1":
    print("!!! ABORT: %s exists (FORCE=1 to overwrite)" % OUT)
    sys.exit(1)
bak = OUT + ".bak-" + time.strftime("%Y%m%d-%H%M%S")
if os.path.exists(OUT):
    shutil.copyfile(OUT, bak)
    print("[backup] %s" % bak, flush=True)

shutil.copyfile(MASTER, OUT)
bpy.ops.wm.open_mainfile(filepath=OUT)

# ---- drop the MHRS remap set (kept safe in MHW-Valor.blend) ----
doomed = [a for a in bpy.data.actions if a.name.startswith("plw_")]
print("[purge] plw_* actions in master copy: %d" % len(doomed), flush=True)
for a in doomed:
    bpy.data.actions.remove(a)
# no orphans_purge here: it would also strip native LMT::w03_00_* actions that
# carry no fake_user - steps 2/3/4 and validation all need them kept

# ---- append MHGU rig + actions ----
with bpy.data.libraries.load(SRC_BLEND, link=False) as (src, dst):
    dst.objects = [o for o in src.objects if o == SRC_RIG]
    dst.actions = [a for a in src.actions if KEEP_RE.match(a)]
rig = bpy.data.objects.get(SRC_RIG)
if rig is None or rig.type != "ARMATURE":
    print("!!! FAIL: appended rig missing"); sys.exit(1)
rig.hide_viewport = False
rig.hide_set(False)
rig.location = (0.0, 0.0, 0.0)
print("[append] rig bones=%d actions=%d" % (len(rig.data.bones), len(bpy.context.selected_objects)), flush=True)

# ---- tag sources (original names kept: LMT::w07::NNN, Motion[N]_Animation Group) ----
tagged = 0
for a in bpy.data.actions:
    if KEEP_RE.match(a.name) and "mhgu_source" not in a:
        a["mhgu_source"] = (("w07.lmt#" + a.name.split("::")[-1])
                            if a.name.startswith("LMT::w07::") else ("glb#" + a.name))
        a.use_fake_user = True
        tagged += 1
print("[tag] %d source actions tagged (original names)" % tagged, flush=True)

mhgu_src = [a.name for a in bpy.data.actions if "mhgu_source" in a]
natives = [a.name for a in bpy.data.actions if a.name.startswith("LMT::")]
print("[state] mhgu_sources=%d natives=%d" % (len(mhgu_src), len(natives)), flush=True)
if len(mhgu_src) != tagged or not natives:
    print("!!! FAIL: unexpected action population"); sys.exit(1)

bpy.ops.wm.save_as_mainfile(filepath=OUT)
print("[done] saved:", OUT, flush=True)
