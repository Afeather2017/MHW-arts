"""找出 vanilla-reset 导出卡死游戏的根因: 与最后已知可用备份(bak-010916, #1 前身)全量对比。
E = _vanilla_reset_export.lmt (卡死), R = bak-010916 (局内验证过), B = 重置前树的导出(参照)。
对比: entryCount / 逐 entry 头部(frameCount,loopFrame,flags) / nbones / timl(masked-md5) /
motion digest / 文件尾布局; 并测 parse->serialize 字节稳定性。"""
import bpy, importlib, hashlib

LMT_VAN = r"E:\MHW-arts\chunk-extracted\chunk\hm\wp\wp03\mot\w03_00\w03_00.lmt"
E_PATH = r"E:\MHW-arts\valor-remap-pipeline\mhgu\_vanilla_reset_export.lmt"
R_PATH = r"E:\SteamLibrary\steamapps\common\Monster Hunter World\nativePC\hm\wp\wp03\mot\w03_00\w03_00.lmt.bak-20261003-010916-valorcfg"
B_PATH = r"E:\MHW-arts\valor-remap-pipeline\mhgu\_backup_forensic_export.lmt"

Lmt = importlib.import_module("MHW-FreeKinetics.struct.Lmt")
E = Lmt.parseLMT(E_PATH)
R = Lmt.parseLMT(R_PATH)
B = Lmt.parseLMT(B_PATH)
Van = Lmt.parseLMT(LMT_VAN)

Ee = {a.id: a for a in E.ActionHeaders}
Re = {a.id: a for a in R.ActionHeaders}
Be = {a.id: a for a in B.ActionHeaders}
Ve = {a.id: a for a in Van.ActionHeaders}

print("entryCount E/R/B:", E.Header.entryCount, R.Header.entryCount, B.Header.entryCount)
print("headers E/R/B:", len(E.ActionHeaders), len(R.ActionHeaders), len(B.ActionHeaders))

TOL = 0.26

def chans_close(a, b):
    if len(a) != len(b): return False
    for ca, cb in zip(a, b):
        if len(ca) != len(cb): return False
        for (va, ta), (vb, tb) in zip(ca, cb):
            if abs(va - vb) > TOL or ta != tb: return False
    return True

def sig(la):
    return [(int(b.boneId), int(getattr(b, "bufferType", -1)), bool(getattr(b, "fold", False)),
             tuple(round(float(x), 2) for x in tuple(b.basis)),
             tuple(tuple((round(float(v), 2), float(t)) for v, t in ch) for ch in b.channels()))
            for b in la.bones]

def same(sa, sb):
    if len(sa) != len(sb): return False
    for x, y in zip(sa, sb):
        if x[:3] != y[:3]: return False
        bx, by = list(x[3]), list(y[3])
        if len(bx) != len(by): return False
        for k, (a, b) in enumerate(zip(bx, by)):
            if abs(a - b) > TOL and not (k == 3 and abs(a - b) <= 1.0 + 1e-6): return False
        if not chans_close(x[4], y[4]): return False
    return True

def tmd5(a):
    if not a.timlOffset: return None
    raw = bytearray(a.timl.serialize()); raw[0:4] = b"\0\0\0\0"
    return hashlib.md5(bytes(raw)).hexdigest()

hdr_diff = timl_diff = mot_diff = 0
diffs = []
for i in sorted(set(Ee) | set(Re)):
    e, r = Ee.get(i), Re.get(i)
    if e is None or r is None:
        diffs.append((i, "entry missing", "E" if e is None else "R")); continue
    hd = []
    if e.frameCount != r.frameCount: hd.append("fc %s/%s" % (e.frameCount, r.frameCount))
    if e.loopFrame != r.loopFrame: hd.append("loop %s/%s" % (e.loopFrame, r.loopFrame))
    if e.Flags != r.Flags or e.Flags2 != r.Flags2: hd.append("flags")
    if bool(e.fcurveOffset) != bool(r.fcurveOffset): hd.append("has-motion %s/%s" % (bool(e.fcurveOffset), bool(r.fcurveOffset)))
    if bool(e.timlOffset) != bool(r.timlOffset): hd.append("has-timl %s/%s" % (bool(e.timlOffset), bool(r.timlOffset)))
    v = Ve.get(i)
    vanfc = v.frameCount if v else None
    if e.fcurveOffset and e.frameCount == 0:
        hd.append("FC=0!")
    if hd:
        hdr_diff += 1
        diffs.append((i, "HDR", "; ".join(hd)))
    if e.timlOffset and r.timlOffset and tmd5(e) != tmd5(r):
        timl_diff += 1; diffs.append((i, "TIML", "md5 differs"))
    if e.fcurveOffset and r.fcurveOffset:
        if len(e.bones) != len(r.bones) or not same(sig(e), sig(r)):
            mot_diff += 1
            diffs.append((i, "MOT", "nb %s/%s fc %s/%s vanfc=%s" % (
                len(e.bones), len(r.bones), e.frameCount, r.frameCount, vanfc)))
print("E vs R: hdr_diff=%d timl_diff=%d motion_diff=%d" % (hdr_diff, timl_diff, mot_diff))
for d in diffs[:40]:
    print("   ", d)

print("== 141/142/143 detail:")
for i in (141, 142, 143):
    e, r, b = Ee.get(i), Re.get(i), Be.get(i)
    for tag, a in (("E", e), ("R", r), ("B", b)):
        if a is None:
            print("  entry", i, tag, "MISSING"); continue
        print("  entry %d %s: fc=%s nb=%s timl=%s" % (
            i, tag, a.frameCount, len(a.bones) if a.fcurveOffset else 0, bool(a.timlOffset)))
    if e and r and e.fcurveOffset and r.fcurveOffset:
        print("  entry %d E==R motion: %s ; E timl==R timl: %s" % (
            i, same(sig(e), sig(r)), tmd5(e) == tmd5(r)))

# round-trip byte stability of E
raw = open(E_PATH, "rb").read()
rt = E.serialize()
print("round-trip E: file=%d reser=%d identical=%s" % (len(raw), len(rt), raw == rt))
