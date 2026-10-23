# EG02 原始结构对比: python _shout_eg02_compare.py <lmt> <entry>...
import struct, sys, os
sys.path.insert(0, r"E:\MHW-arts")
import dump_timl_frames as D

path = sys.argv[1]
entries = [int(x) for x in sys.argv[2:]]
buf = open(path, "rb").read()
_, actions = D.parse_lmt(buf)

for eid in entries:
    act = actions[eid]
    timl = D.parse_timl(buf, act["timlOffset"])
    print("=" * 90)
    print("entry %d frameCount=%d TIML: animLen=%.3f loop=%d dataIx=%d,%d label=%08X types=%d" % (
        eid, act["frameCount"], timl["animationLength"], timl["loopControl"],
        timl["dataIx0"], timl["dataIx1"], timl["labelHash"], timl["typeCount"]))
    for typ in timl["types"]:
        if "Group02" not in typ["name"] and "Group01" not in typ["name"]:
            continue
        print("  [%s] unkn0=%s transforms=%d" % (typ["name"], typ["unkn0"], len(typ["transforms"])))
        for xf in typ["transforms"]:
            kfs = "  ".join("f%.1f=%s(t%d,e%d)" % (k["frame"], k["value"], k["transition"], k["easing"])
                            for k in xf["keyframes"])
            print("    %-24s dt=%d %s" % (xf["name"], xf["dataType"], kfs))
