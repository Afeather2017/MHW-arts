"""解析导出产物,核对 entry 轨数/Root/挂点形态。用法:
blender -b --python mhgu/_check_repro_entry.py -- <lmt路径> <entryId>
缺省 = E:/MHW-arts/mhgu-export-repro.lmt 的 entry 103。"""
import sys
sys.path.append(r"C:\Users\Afeather\AppData\Roaming\Blender Foundation\Blender\4.3\scripts\addons")
import importlib
argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
LMT_PATH = argv[0] if argv else r"E:\MHW-arts\mhgu-export-repro.lmt"
ENTRY_ID = int(argv[1]) if len(argv) > 1 else 103
Lmt = importlib.import_module("MHW-FreeKinetics.struct.Lmt")
lmt = Lmt.parseLMT(LMT_PATH)
a = [x for x in lmt.ActionHeaders if x.id == ENTRY_ID][0]
bones = a.bones
root = [b for b in bones if b.boneId < 0]
s30 = [b for b in bones if b.boneId == 30 and b.usage == 2]
s47 = [b for b in bones if b.boneId == 47 and b.usage == 2]
print("entry%d tracks=%d frameCount=%s rootTracks=%s" % (
    ENTRY_ID, len(bones), a.frameCount, [(b.boneId, b.usage) for b in root]))
for b in s30 + s47:
    vals = None
    try:
        vals = getattr(b, "keyframeBuffer", None)
    except Exception:
        pass
    print("  socket boneId=%s usage=%s bufType=%s" % (b.boneId, b.usage, b.bufferType))
# 000 loc
b0 = [b for b in bones if b.boneId == 0 and b.usage % 3 == 1]
print("  000-loc present:", bool(b0), "bufType:", b0[0].bufferType if b0 else None)
