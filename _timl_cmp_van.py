# -*- coding: utf-8 -*-
"""Compare TIML type/transform inventory: deployed vs vanilla, entries 338-346."""
import struct

DEPLOYED = r"E:\SteamLibrary\steamapps\common\Monster Hunter World\nativePC\hm\wp\wp03\mot\w03_00\w03_00.lmt"
VANILLA  = r"E:\MHW-arts\chunk-extracted\chunk\hm\wp\wp03\mot\w03_00\w03_00.lmt"

FHK = r"E:\MHW-arts\research\MHW-Free-HyperKinetics\struct"
import os
def load(fn):
    d = {}
    p = os.path.join(FHK, fn)
    if os.path.exists(p):
        with open(p, encoding="utf-8") as f:
            for line in f:
                line = line.strip()
                if line:
                    h, n = line.split(",", 1)
                    d[int(h)] = n
    return d
TL = load("TIMLTimelineParam.txt")
DT = load("TIMLDatatypes.txt")
tn = lambda h: TL.get(h, "unk%08X" % h)
dn = lambda h: DT.get(h, ("unk%08X" % h, ""))[0] if isinstance(DT.get(h), tuple) else DT.get(h, "unk%08X" % h)

def parse(buf, off):
    typesOffset, typeCount, ix0, ix1, animLen, loopStart, loopCtrl, label = \
        struct.unpack_from("<QQiiffiI", buf, off)
    types = []
    if typesOffset and typeCount:
        for ti in range(typeCount):
            toff, tcount, tlHash, u0 = struct.unpack_from("<QQIi", buf, typesOffset + 24*ti)
            xfs = []
            if toff:
                for xi in range(tcount):
                    xoff, xcount, dtHash, dt = struct.unpack_from("<QQIi", buf, toff + 24*xi)
                    xfs.append((dn(dtHash), xcount, dt))
            types.append((tn(tlHash), tlHash, u0, xfs))
    return dict(typesOffset=typesOffset, typeCount=typeCount, ix0=ix0, ix1=ix1,
                animLen=animLen, loopStart=loopStart, loopCtrl=loopCtrl, types=types)

DB = open(DEPLOYED, "rb").read()
VB = open(VANILLA, "rb").read()
_, _, offsD = struct.unpack_from("<4shh", DB, 0)[:3], None, None
cntD = struct.unpack_from("<h", DB, 6)[0]
offsD = struct.unpack_from("<%dQ" % cntD, DB, 16)
cntV = struct.unpack_from("<h", VB, 6)[0]
offsV = struct.unpack_from("<%dQ" % cntV, VB, 16)

for eid in (338, 339, 340, 341, 342, 343, 344, 345, 346):
    td = parse(DB, offsD[eid] and struct.unpack_from("<Q", DB, offsD[eid]+88)[0])
    tv = parse(VB, struct.unpack_from("<Q", VB, offsV[eid]+88)[0])
    sd = {h for (_, h, _, _) in td["types"]}
    sv = {h for (_, h, _, _) in tv["types"]}
    print("entry %d: deployed types=%d animLen=%.1f | vanilla types=%d animLen=%.1f | missing-in-deployed=%s | extra-in-deployed=%s" %
          (eid, td["typeCount"], td["animLen"], tv["typeCount"], tv["animLen"],
           sorted(tn(h) for h in sv - sd), sorted(tn(h) for h in sd - sv)))
    if eid in (345,):
        print("  deployed ix0=%d ix1=%d | vanilla ix0=%d ix1=%d" % (td["ix0"], td["ix1"], tv["ix0"], tv["ix1"]))
        print("  deployed detail:")
        for name, h, u0, xfs in td["types"]:
            print("    [%s] u0=%d: %s" % (name, u0, ", ".join("%s(n=%d)" % (n, c) for n, c, _ in xfs)))
        print("  vanilla detail:")
        for name, h, u0, xfs in tv["types"]:
            print("    [%s] u0=%d: %s" % (name, u0, ", ".join("%s(n=%d)" % (n, c) for n, c, _ in xfs)))
