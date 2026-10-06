# -*- coding: utf-8 -*-
"""Per-entry CollisionTimelineObject (damage window) routing report.
Dumps CTO node ids + mFlag windows for every entry, deployed vs vanilla.
"""
import sys
sys.path.insert(0, r"E:\MHW-arts")
import dump_timl_frames as D

DEPLOYED = r"E:\SteamLibrary\steamapps\common\Monster Hunter World\nativePC\hm\wp\wp03\mot\w03_00\w03_00.lmt"
VANILLA = r"E:\MHW-arts\chunk-extracted\chunk\hm\wp\wp03\mot\w03_00\w03_00.lmt"


def is_cto(typ):
    return typ["name"] == "nTimelineParam::CollisionTimelineObject"


def cto_map(path):
    buf = open(path, "rb").read()
    n, actions = D.parse_lmt(buf)
    out = {}
    for eid in sorted(actions):
        act = actions[eid]
        if not act["timlOffset"]:
            continue
        timl = D.parse_timl(buf, act["timlOffset"])
        nodes, flags, param = [], [], None
        for typ in timl["types"]:
            if is_cto(typ):
                param = typ["name"]
                for xf in typ["transforms"]:
                    nm = xf["name"]
                    if nm.startswith("ノードID"):
                        nodes += [(nm, k["frame"], k["value"]) for k in xf["keyframes"]]
                    elif nm == "mFlag":
                        flags += [(k["frame"], k["value"]) for k in xf["keyframes"]]
        if param is not None:
            out[eid] = (param, sorted(nodes), flags)
    return out


def main():
    dep = cto_map(DEPLOYED)
    van = cto_map(VANILLA)
    all_e = sorted(set(dep) | set(van))
    print("entry | vanilla CTO (param/nodes/mFlag) | deployed CTO (param/nodes/mFlag)")
    for e in all_e:
        v = van.get(e)
        d = dep.get(e)
        def fmt(t):
            if t is None:
                return "-"
            param, nodes, flags = t
            nf = ",".join("%s@f%d=%s" % (nm.split(".")[-1][:9], f, v) for nm, f, v in nodes) or "no-node"
            ff = " ".join("f%d=%s" % (f, v) for f, v in flags)
            return "%s nodes[%s] mFlag[%s]" % (param.replace("nTimelineParam::", "")[:22], nf, ff)
        mark = "  " if v == d else "!!"
        print("%s %4d | %s | %s" % (mark, e, fmt(v), fmt(d)))


if __name__ == "__main__":
    main()
