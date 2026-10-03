"""F2 v3 (final): 原版动作字节 + R 的 TIML(剥离 0x4BD36121 占位类型, 同步 count) + R 的 141-143。
占位类型 = 2026-10-02 深夜 addcto 链在 339/340/343 共享块上追加的
(timelineParam=0x4BD36121, transform hash=0x1, 0 keyframe) 结构, 进存档卡死元凶(待局内终验)。
注意: 剥离 types 必须同步 TIML_Data.count, 否则解析器按旧 count 多读一个类型(错位假象)。"""
import importlib, hashlib

V_PATH = r"E:\MHW-arts\chunk-extracted\chunk\hm\wp\wp03\mot\w03_00\w03_00.lmt"
R_PATH = r"E:\SteamLibrary\steamapps\common\Monster Hunter World
ativePC\hm\wp\wp03\mot\w03_00\w03_00.lmt.bak-20261003-010916-valorcfg"
OUT = r"E:\MHW-artsalor-remap-pipeline\mhgu\_vanilla_firstaid_f2.lmt"
PLACEHOLDER = 0x4BD36121

Lmt = importlib.import_module("MHW-FreeKinetics.struct.Lmt")
V = Lmt.parseLMT(V_PATH)
R = Lmt.parseLMT(R_PATH)
Rmap = {a.id: a for a in R.ActionHeaders}

stripped = 0
for h in V.ActionHeaders:
    r = Rmap.get(h.id)
    if r is not None and r.timlOffset:
        t = r.timl
        n0 = len(t.types)
        t.types = [ty for ty in t.types if int(ty.timelineParameterHash) != PLACEHOLDER]
        if len(t.types) != n0:
            stripped += 1
            t.count = len(t.types)
        h.timl = t
for cid in (141, 142, 143):
    V.ActionHeaders.append(Rmap[cid])
print("stripped:", stripped)
data = V.serialize()
open(OUT, "wb").write(data)
print("written", len(data), hashlib.md5(data).hexdigest())

F = Lmt.parseLMT(OUT)
Fm = {a.id: a for a in F.ActionHeaders}
bad = [i for i in ({a.id for a in V.ActionHeaders})
       if Fm.get(i) and Fm[i].timlOffset
       and any(int(ty.timelineParameterHash) == PLACEHOLDER for ty in Fm[i].timl.types)]
print("headers:", len(F.ActionHeaders), "| placeholder remaining:", bad)
print("OK" if not bad and stripped == 3 else "CHECK")
