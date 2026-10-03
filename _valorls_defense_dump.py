# -*- coding: utf-8 -*-
"""Focused defense-field dump: ValorLS mod LMT vs vanilla.
Prints PlMotionCommon (mFlag/mTrigger) and PlMotionInput (all mFlag* words)
with hex decode; flags b24/b26 windows and 1024/2048 triggers.
"""
import sys

import dump_timl_frames as D

MOD = "MHW-valor/nativePC/hm/wp/wp03/mot/w03_00/w03_00.lmt"
VAN = "chunk-extracted/chunk/hm/wp/wp03/mot/w03_00/w03_00.lmt"

ENTRIES = [101, 106, 107, 109, 110, 140, 154, 156, 157, 158, 162, 170, 174,
           306, 307, 311, 312, 338, 339, 340, 341, 342, 344, 346]

WORDS = ["mFlag", "mFlag1", "mFlag2", "mFlag4", "mFlag6", "mTrigger",
         "mNoHitTime", "mSuperArmorTime", "mHyperArmorTime"]


def intval(v):
    return int(str(v).strip("() ,"))


def flags_str(v):
    bits = []
    names = {31: "b31", 30: "b30", 28: "b28", 27: "b27", 26: "b26-NOHIT",
             25: "b25", 24: "b24-SA", 16: "b16", 8: "b8"}
    for b in sorted(names, reverse=True):
        if v >> b & 1:
            bits.append(names[b])
    return "0x%08X(%s)" % (v & 0xFFFFFFFF, ",".join(bits) if bits else "-")


def load(path):
    buf = open(path, "rb").read()
    n, actions = D.parse_lmt(buf)
    return buf, actions


def get_tracks(buf, act):
    """{(cat, name): [(frame, rawvalue)]} only uint tracks."""
    if not act or not act["timlOffset"]:
        return {}
    timl = D.parse_timl(buf, act["timlOffset"])
    out = {}
    for typ in timl["types"]:
        for xf in typ["transforms"]:
            if xf["dataType"] != 1:
                continue
            out[(typ["name"], xf["name"])] = [
                (round(k["frame"]), k["value"]) for k in xf["keyframes"]]
    return out


def report(tag, buf, acts, entries):
    for e in entries:
        act = acts.get(e)
        if not act or not act["timlOffset"]:
            print("%s entry %d: NO TIML" % (tag, e))
            continue
        print("%s entry %d (frameCount=%d):" % (tag, e, act["frameCount"]))
        tracks = get_tracks(buf, act)
        for (cat, name), keys in sorted(tracks.items()):
            if cat != "nTimelineParam::PlMotionCommon" and cat != "nTimelineParam::PlMotionInput":
                continue
            if name not in WORDS:
                continue
            if name in ("mNoHitTime", "mSuperArmorTime", "mHyperArmorTime"):
                cells = ["f%d=%s" % (f, v) for f, v in keys]
            else:
                cells = []
                for f, v in keys:
                    try:
                        v = intval(v)
                    except ValueError:
                        continue
                    mark = ""
                    if v & 0x04000000:
                        mark += "*26"
                    if v & 0x01000000:
                        mark += "*24"
                    if v == 2048:
                        mark += "*T2048"
                    if v == 1024:
                        mark += "*t1024"
                    cells.append("f%d=%s%s" % (f, flags_str(v), mark))
            if not cells:
                continue
            print("  [%s] %s" % (cat.split("::")[1], name))
            print("    " + "  ".join(cells))


def main():
    mbuf, macts = load(MOD)
    vbuf, vacts = load(VAN)
    which = sys.argv[1] if len(sys.argv) > 1 else "all"
    if which in ("all", "mod"):
        report("MOD", mbuf, macts, ENTRIES)
    if which in ("all", "van"):
        report("VAN", vbuf, vacts, ENTRIES)


if __name__ == "__main__":
    main()
