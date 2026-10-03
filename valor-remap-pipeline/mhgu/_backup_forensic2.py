"""给备份里 151 个内容不符的原版槽定性: 每个槽的导出内容去匹配全部 164 条原版动作,
看它到底是"别条原版动作"(接线乱)还是"哪条都不像"(被改过/重编码)。"""
import importlib, json

LMT_VAN = r"E:\MHW-arts\chunk-extracted\chunk\hm\wp\wp03\mot\w03_00\w03_00.lmt"
SCRATCH = r"E:\MHW-arts\valor-remap-pipeline\mhgu\_backup_forensic_export.lmt"
FR = r"E:\MHW-arts\valor-remap-pipeline\mhgu\_backup_forensic.json"

Lmt = importlib.import_module("MHW-FreeKinetics.struct.Lmt")
van = Lmt.parseLMT(LMT_VAN)
exp = Lmt.parseLMT(SCRATCH)
van_e = {a.id: a for a in van.ActionHeaders}
exp_e = {a.id: a for a in exp.ActionHeaders}

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

van_sig = {i: sig(a) for i, a in van_e.items() if a.fcurveOffset}
fr = json.load(open(FR))
bad = fr["content_bad"]
print("flagged:", len(bad))

rewired, foreign = [], {}
for i in bad:
    if i not in exp_e or not exp_e[i].fcurveOffset:
        foreign[i] = "missing"
        continue
    s = sig(exp_e[i])
    hits = [j for j, vs in van_sig.items() if same(s, vs)]
    if hits:
        rewired.append((i, hits))
    else:
        # coarse similarity: frameCount + nbones vs own vanilla
        v = van_e[i]
        foreign[i] = "fc %s/%s nb %s/%s" % (
            exp_e[i].frameCount, v.frameCount, len(s), len(van_sig[i]))
print("matches ANOTHER vanilla id (wiring mess):", len(rewired), rewired[:25])
print("matches nothing:", len(foreign))
for k in sorted(foreign)[:30]:
    print("   entry", k, foreign[k])
