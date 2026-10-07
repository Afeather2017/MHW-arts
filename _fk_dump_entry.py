# Blender headless: dump LMT entries via FreeKinetics (ground truth).
# blender -b --python _fk_dump_entry.py -- <lmt> <entry> [<entry>...]
import sys
sys.path.append(r"C:\Users\Afeather\AppData\Roaming\Blender Foundation\Blender\4.3\scripts\addons")
import importlib
argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
LMT_PATH = argv[0]
EIDS = [int(x) for x in argv[1:]]
Lmt = importlib.import_module("MHW-FreeKinetics.struct.Lmt")
lmt = Lmt.parseLMT(LMT_PATH)
for eid in EIDS:
    a = [x for x in lmt.ActionHeaders if x.id == eid]
    if not a:
        print("entry %d: MISSING" % eid); continue
    a = a[0]
    print("=" * 80)
    print("entry %d frameCount=%s loopFrame=%s tracks=%d timl=%s" %
          (eid, a.frameCount, a.loopFrame, len(a.bones), "yes" if getattr(a, "timl", None) else "no"))
    for b in a.bones:
        n = len(b.keyframes) if b.keyframes is not None else 0
        frames = []
        if n and b.bufferType in (3, 4, 5):
            frames = [k.frame for k in b.keyframes][:12]
        print("  bone=%4d usage=%d bt=%2d keys=%4d bsize=%s lerp=%s frames[:12]=%s" %
              (b.boneId, b.usage, b.bufferType, n, getattr(b, "bufferSize", "?"),
               "y" if b.lerp else "n", frames))
