# -*- coding: utf-8 -*-
"""One-off: compare ValorLS mod LMT vs vanilla extracted LMT.
Lists entries whose frameCount or TIML tracks differ; flags defense-relevant
fields (PlMotionCommon mFlag b24/b26 windows, mTrigger 2048 keys).
"""
import hashlib
import sys

import dump_timl_frames as D

MOD = "MHW-valor/nativePC/hm/wp/wp03/mot/w03_00/w03_00.lmt"
VAN = "chunk-extracted/chunk/hm/wp/wp03/mot/w03_00/w03_00.lmt"

B24 = 0x01000000  # super armor
B26 = 0x04000000  # no-hit / i-frame window


def load(path):
    buf = open(path, "rb").read()
    n, actions = D.parse_lmt(buf)
    return buf, n, actions


def timl_sig(buf, act):
    """{cat: {transform: [(frame, value), ...]}} for one entry."""
    if not act or not act["timlOffset"]:
        return None
    timl = D.parse_timl(buf, act["timlOffset"])
    sig = {}
    for typ in timl["types"]:
        per = {}
        for xf in typ["transforms"]:
            per[xf["name"]] = [(round(k["frame"]), k["value"]) for k in xf["keyframes"]]
        sig[typ["name"]] = per
    return sig


def defense_notes(sig):
    """Human flags for defense-relevant content in one entry's TIML sig."""
    notes = []
    if sig is None:
        return notes
    for cat, per in sig.items():
        if "PlMotion" not in cat:
            continue
        for name, keys in per.items():
            if name == "mFlag":
                b24 = [f for f, v in keys if isinstance(v, int) and v & B24]
                b26 = [f for f, v in keys if isinstance(v, int) and v & B26]
                if b24:
                    notes.append("b24-SA@%s" % b24)
                if b26:
                    notes.append("b26-NOHIT@%s" % b26)
            if name == "mTrigger":
                t2048 = [f for f, v in keys if isinstance(v, int) and v == 2048]
                if t2048:
                    notes.append("TRIG2048@%s" % t2048)
    return notes


def main():
    mbuf, mn, macts = load(MOD)
    vbuf, vn, vacts = load(VAN)
    mh = hashlib.md5(mbuf).hexdigest()
    vh = hashlib.md5(vbuf).hexdigest()
    print("mod : %s  %d bytes  entries=%d  md5=%s" % (MOD, len(mbuf), mn, mh))
    print("van : %s  %d bytes  entries=%d  md5=%s" % (VAN, len(vbuf), vn, vh))

    diff_entries = []
    for ix in sorted(set(list(macts.keys()) + list(vacts.keys()))):
        ma = macts.get(ix)
        va = vacts.get(ix)
        ms = timl_sig(mbuf, ma)
        vs = timl_sig(vbuf, va)
        mfc = ma["frameCount"] if ma else None
        vfc = va["frameCount"] if va else None
        if ms != vs or mfc != vfc:
            diff_entries.append((ix, ma, va, ms, vs, mfc, vfc))

    print("\n== entries differing (frameCount or TIML): %d ==" % len(diff_entries))
    for ix, ma, va, ms, vs, mfc, vfc in diff_entries:
        print("\n--- entry %d: frameCount van=%s mod=%s  timl van=%s mod=%s" %
              (ix, vfc, mfc,
               "none" if vs is None else "yes", "none" if ms is None else "yes"))
        dn_v = defense_notes(vs)
        dn_m = defense_notes(ms)
        if dn_v or dn_m:
            print("    DEFENSE van: %s" % (dn_v or "-"))
            print("    DEFENSE mod: %s" % (dn_m or "-"))
        if ms is not None and vs is not None:
            cats = sorted(set(list(ms.keys()) + list(vs.keys())))
            for cat in cats:
                if ms.get(cat) != vs.get(cat):
                    print("    [%s]" % cat)
                    transforms = sorted(set(list(ms.get(cat, {}).keys()) +
                                            list(vs.get(cat, {}).keys())))
                    for tname in transforms:
                        if ms.get(cat, {}).get(tname) != vs.get(cat, {}).get(tname):
                            print("      %s" % tname)
                            print("        van: %s" % (vs.get(cat, {}).get(tname),))
                            print("        mod: %s" % (ms.get(cat, {}).get(tname),))
        elif ms is not None:
            print("    mod-only TIML:")
            for cat, per in ms.items():
                for tname, keys in per.items():
                    print("      [%s] %s: %s" % (cat, tname, keys))


if __name__ == "__main__":
    sys.exit(main())
