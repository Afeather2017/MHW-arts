"""对比已部署 #1 vs 原版 #2 的 motion 漂移签名:量化漂移幅度分布 + loc basis w。
若 #1 同样带这些签名 => 工具链既有行为(用户已在局内验证过), 非本次导出独有。
另: 统计 #2 全部 loc basis w 取值分布。
"""
import bpy, importlib

LMT_MOD = r"E:\SteamLibrary\steamapps\common\Monster Hunter World\nativePC\hm\wp\wp03\mot\w03_00\w03_00.lmt"
LMT_VAN = r"E:\MHW-arts\chunk-extracted\chunk\hm\wp\wp03\mot\w03_00\w03_00.lmt"
SCRATCH = r"E:\MHW-arts\valor-remap-pipeline\mhgu\_vanilla_reset_export.lmt"

Lmt = importlib.import_module("MHW-FreeKinetics.struct.Lmt")
mod = Lmt.parseLMT(LMT_MOD)
van = Lmt.parseLMT(LMT_VAN)
exp = Lmt.parseLMT(SCRATCH)
mod_e = {a.id: a for a in mod.ActionHeaders}
van_e = {a.id: a for a in van.ActionHeaders}
exp_e = {a.id: a for a in exp.ActionHeaders}

def maxdrift(a, b):
    """(kf值最大绝对差, kf时间最大差, basis差列表, tail差列表)"""
    dv = dt = 0.0
    bd, td = [], []
    for x, y in zip(a.bones, b.bones):
        for chx, chy in zip(x.channels(), y.channels()):
            for (vx, tx), (vy, ty) in zip(chx, chy):
                dv = max(dv, abs(vx - vy)); dt = max(dt, abs(tx - ty))
        bd.append(max((abs(p - q) for p, q in zip(tuple(x.basis), tuple(y.basis))), default=0.0))
        if hasattr(x, "tail") and hasattr(y, "tail"):
            td.append(max((abs(p - q) for p, q in zip(tuple(x.tail), tuple(y.tail))), default=0.0))
    return dv, dt, max(bd, default=0.0), max(td, default=0.0)

def drift_profile(pairs, label):
    import statistics
    dv_all, dt_all, bd_all = [], [], []
    worst = (0, 0, 0, 0)
    for i, a, b in pairs:
        dv, dt, db, dtl = maxdrift(a, b)
        dv_all.append(dv); dt_all.append(dt); bd_all.append(db)
        if (dv, dt, db, dtl) > worst[:4] if False else dv > worst[0]:
            worst = (dv, dt, db, dtl, i)
    print("%s: entries=%d kfValDrift max=%.6f med=%.6f | kfTimeDrift max=%.4f | basisDrift max=%.6f | worst entry=%s" % (
        label, len(pairs), max(dv_all), statistics.median(dv_all), max(dt_all),
        max(bd_all), worst[-1]))

common = [i for i in van_e if van_e[i].fcurveOffset and i in mod_e and mod_e[i].fcurveOffset]
print("common vanilla-id motion entries: mod-vs-van %d, exp-vs-van %d" % (len(common), len(common)))
drift_profile([(i, mod_e[i], van_e[i]) for i in common], "DEPLOYED#1 vs VAN#2")
drift_profile([(i, exp_e[i], van_e[i]) for i in common], "SCRATCH-EXP vs VAN#2")

# loc basis w distribution in vanilla
w_dist = {}
nloc = 0
for a in van_e.values():
    if not a.fcurveOffset:
        continue
    for b in a.bones:
        basis = tuple(b.basis)
        if len(basis) == 4:
            w_dist[round(float(basis[3]), 3)] = w_dist.get(round(float(basis[3]), 3), 0) + 1
print("vanilla 4-comp basis w distribution:", w_dist)

# does DEPLOYED #1 carry w=0 on those?
w1 = {}
for i in common:
    for b in mod_e[i].bones:
        basis = tuple(b.basis)
        if len(basis) == 4:
            w1[round(float(basis[3]), 3)] = w1.get(round(float(basis[3]), 3), 0) + 1
print("deployed#1 4-comp basis w distribution:", w1)
we = {}
for i in common:
    for b in exp_e[i].bones:
        basis = tuple(b.basis)
        if len(basis) == 4:
            we[round(float(basis[3]), 3)] = we.get(round(float(basis[3]), 3), 0) + 1
print("scratch-exp 4-comp basis w distribution:", we)
