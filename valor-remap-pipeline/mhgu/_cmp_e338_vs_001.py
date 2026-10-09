# -*- coding: utf-8 -*-
# 判定对比: 新导出 E338 (刚1) vs 0.0.1 部署 E140 (已验证刚1) 的骨骼键逐值对比。
import importlib
L = importlib.import_module("MHW-FreeKinetics.struct.Lmt")

l1 = L.parseLMT(r'E:\MHW-arts\_e338_verify.lmt')
l2 = L.parseLMT(r'E:\SteamLibrary\steamapps\common\Monster Hunter World\nativePC\hm\wp\wp03\mot\w03_00\w03_00_0.0.1.lmt')
a1 = next(a for a in l1.ActionHeaders if a.id == 338)
a2 = next(a for a in l2.ActionHeaders if a.id == 140)
print('frameCount', a1.frameCount, a2.frameCount, '| loopFrame', a1.loopFrame, a2.loopFrame,
      '| bones', len(a1.bones), len(a2.bones), flush=True)

maxd = 0.0
ndiff = 0
ncomp = 0
nkeys = 0
for b1, b2 in zip(a1.bones, a2.bones):
    assert b1.boneId == b2.boneId and b1.bufferType == b2.bufferType, (b1.boneId, b2.boneId)
    assert len(b1.keyframes) == len(b2.keyframes), (b1.boneId, len(b1.keyframes), len(b2.keyframes))
    nkeys += len(b1.keyframes)
    for k1, k2 in zip(b1.keyframes, b2.keyframes):
        v1 = list(k1.vec)
        v2 = list(k2.vec)
        for x, y in zip(v1, v2):
            ncomp += 1
            if x != y:
                ndiff += 1
                maxd = max(maxd, abs(x - y))
print(f'keys={nkeys} comps={ncomp} ndiff={ndiff} max|delta|={maxd:.3e}', flush=True)
