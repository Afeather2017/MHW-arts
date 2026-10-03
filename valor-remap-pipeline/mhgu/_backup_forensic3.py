"""裁决: 备份导出 vs 部署#1 逐 entry 动作比对。
一致 => 备份树 = 局内在跑的状态, "乱掉"是编辑器观感(共享命名/布局);
不一致 => blend 与游戏漂移, 原版动作确实被改过。
另: 抽样 entry 1 备份导出 vs 原版#2 的逐通道差异直方图(定性"重编码"还是"内容变了")。"""
import importlib, json

LMT_VAN = r"E:\MHW-arts\chunk-extracted\chunk\hm\wp\wp03\mot\w03_00\w03_00.lmt"
LMT_MOD = r"E:\SteamLibrary\steamapps\common\Monster Hunter World\nativePC\hm\wp\wp03\mot\w03_00\w03_00.lmt"
SCRATCH = r"E:\MHW-arts\valor-remap-pipeline\mhgu\_backup_forensic_export.lmt"

Lmt = importlib.import_module("MHW-FreeKinetics.struct.Lmt")
van = Lmt.parseLMT(LMT_VAN)
mod = Lmt.parseLMT(LMT_MOD)
bak = Lmt.parseLMT(SCRATCH)
van_e = {a.id: a for a in van.ActionHeaders}
mod_e = {a.id: a for a in mod.ActionHeaders}
bak_e = {a.id: a for a in bak.ActionHeaders}

TOL = 0.26

def chans_close(a, b):
    if len(a) != len(b):
        return False
    for ca, cb in zip(a, b):
        if len(ca) != len(cb):
            return False
        for (va, ta), (vb, tb) in zip(ca, cb):
            if abs(va - vb) > TOL or ta != tb:
                return False
    return True

def sig(la):
    return [(int(b.boneId), int(getattr(b, "bufferType", -1)), bool(getattr(b, "fold", False)),
             tuple(round(float(x), 2) for x in tuple(b.basis)),
             tuple(tuple((round(float(v), 2), float(t)) for v, t in ch) for ch in b.channels()))
            for b in la.bones]

def same(sa, sb):
    if len(sa) != len(sb):
        return False
    for x, y in zip(sa, sb):
        if x[:3] != y[:3]:
            return False
        bx, by = list(x[3]), list(y[3])
        if len(bx) != len(by):
            return False
        for k, (a, b) in enumerate(zip(bx, by)):
            if abs(a - b) > TOL and not (k == 3 and abs(a - b) <= 1.0 + 1e-6):
                return False
        if not chans_close(x[4], y[4]):
            return False
    return True

common = [i for i in van_e if van_e[i].fcurveOffset and i in bak_e and bak_e[i].fcurveOffset
          and i in mod_e and mod_e[i].fcurveOffset]
same_mod = [i for i in common if i in bak_e and mod_e.get(i) and mod_e[i].fcurveOffset
            and same(sig(bak_e[i]), sig(mod_e[i]))]
diff_mod = [i for i in common if i not in same_mod]
print("BACKUP-EXPORT vs DEPLOYED#1: common=%d SAME=%d DIFF=%d" % (len(common), len(same_mod), len(diff_mod)))
print("  diff entries:", diff_mod[:40])

# detail: entry 1 backup vs vanilla channel stats
eid = 1
ba, va = bak_e[eid], van_e[eid]
print("== entry", eid, "backup nb=%d van nb=%d" % (len(ba.bones), len(va.bones)))
van_bones = {int(b.boneId): b for b in va.bones}
nk_diff = timing_missing = val_max = 0
matched_channels = 0
total_channels = 0
for b in ba.bones:
    vb = van_bones.get(int(b.boneId))
    if vb is None:
        print("   bone", b.boneId, "not in vanilla!")
        continue
    for chx, chy in zip(b.channels(), vb.channels()):
        total_channels += 1
        kx = [(round(float(v), 2), float(t)) for v, t in chx]
        ky = [(round(float(v), 2), float(t)) for v, t in chy]
        tx = set(t for _, t in kx); ty = set(t for _, t in ky)
        if len(kx) != len(ky):
            nk_diff += 1
        else:
            if tx != ty:
                timing_missing += 1
            else:
                m = max((abs(a[0] - b[0]) for a, b in zip(kx, ky)), default=0)
                val_max = max(val_max, m)
                if m <= TOL:
                    matched_channels += 1
print("   channels total=%d keycount-diff=%d timing-diff=%d withinTol=%d maxValDiff=%.4f" %
      (total_channels, nk_diff, timing_missing, matched_channels, val_max))
# which bone is missing in backup vs vanilla
missing_bones = set(van_bones) - {int(b.boneId) for b in ba.bones}
print("   bones missing in backup:", missing_bones)
