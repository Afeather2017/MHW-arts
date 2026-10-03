"""构建 game-safe 部署文件 F:
- 动作字节 = 原版 V 的本体(round-trip 已证明字节稳定)
- TIML     = R(bak-010916, 局内验证过)的编辑版(与 E masked-md5 全等)
- 141/142/143 自定义槽 = R 的完整 ActionHeader(动作+TIML+头部)
这样 F 与"能进游戏的 R"的差异只有: 原版槽动作用真原版字节(替代旧重编码) +
9 个刻意替换槽回原版(= 用户的 vanilla-reset 诉求)。
输出 _vanilla_firstaid.lmt 并做全量自校验。"""
import importlib, hashlib

V_PATH = r"E:\MHW-arts\chunk-extracted\chunk\hm\wp\wp03\mot\w03_00\w03_00.lmt"
R_PATH = r"E:\SteamLibrary\steamapps\common\Monster Hunter World\nativePC\hm\wp\wp03\mot\w03_00\w03_00.lmt.bak-20261003-010916-valorcfg"
OUT = r"E:\MHW-arts\valor-remap-pipeline\mhgu\_vanilla_firstaid.lmt"

Lmt = importlib.import_module("MHW-FreeKinetics.struct.Lmt")
V = Lmt.parseLMT(V_PATH)
R = Lmt.parseLMT(R_PATH)
Rmap = {a.id: a for a in R.ActionHeaders}

swapped = kept = 0
for h in V.ActionHeaders:
    r = Rmap.get(h.id)
    if r is not None and r.timlOffset:
        h.timl = r.timl
        swapped += 1
    else:
        kept += 1
custom_ids = [141, 142, 143]
for cid in custom_ids:
    V.ActionHeaders.append(Rmap[cid])
print("timl swapped=%d kept=%d customs appended=%d" % (swapped, kept, len(custom_ids)))

data = V.serialize()
with open(OUT, "wb") as f:
    f.write(data)
print("written %s (%d bytes, md5 %s)" % (OUT, len(data), hashlib.md5(data).hexdigest()))

# ---- self verification ----
F = Lmt.parseLMT(OUT)
Fm = {a.id: a for a in F.ActionHeaders}
Vm = {a.id: a for a in V.ActionHeaders}
print("F headers=%d entryCount=%d" % (len(F.ActionHeaders), F.Header.entryCount))

def md5_motion(a):
    parts = []
    for b in a.bones:
        chans = tuple(tuple((round(float(v), 2), float(t)) for v, t in ch) for ch in b.channels())
        parts.append(repr((int(b.boneId), int(getattr(b, "bufferType", -1)),
                           bool(getattr(b, "fold", False)),
                           tuple(round(float(x), 2) for x in tuple(b.basis)), chans)))
    return hashlib.md5("\n".join(sorted(parts)).encode()).hexdigest()

def tmd5(a):
    raw = bytearray(a.timl.serialize()); raw[0:4] = b"\0\0\0\0"
    return hashlib.md5(bytes(raw)).hexdigest()

bad = []
for i, v in Vm.items():
    f = Fm.get(i)
    if f is None or not f.fcurveOffset:
        bad.append((i, "motion missing")); continue
    if md5_motion(f) != md5_motion(v):
        bad.append((i, "motion != vanilla"))
    r = Rmap.get(i)
    if r is not None and r.timlOffset and f.timlOffset:
        if tmd5(f) != tmd5(r):
            bad.append((i, "timl != R"))
    elif r is not None and not r.timlOffset and f.timlOffset:
        bad.append((i, "timl present but R has none"))
for cid in custom_ids:
    f, r = Fm.get(cid), Rmap[cid]
    if f is None or md5_motion(f) != md5_motion(r) or tmd5(f) != tmd5(r):
        bad.append((cid, "custom != R"))
print("SELF-CHECK bad=%d %s" % (len(bad), bad[:10]))
print("OK" if not bad else "FAIL")
