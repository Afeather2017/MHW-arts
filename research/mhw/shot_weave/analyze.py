# -*- coding: utf-8 -*-
"""强化射击 (LS special shot) TIML signature analysis.

1. scan   : find every entry whose PlMotionCommon.mTrigger carries value 1024/1 (the shot pair)
2. diff   : per-entry TIML track-set comparison between two LMT files (vanilla vs deployed)
3. windows: bit-decode PlMotionCommon/PlMotionInput flag+trigger windows for chosen entries
"""
import struct
import sys

sys.path.insert(0, r"E:\MHW-arts")
import dump_timl_frames as D

VANILLA = r"E:\MHW-arts\chunk-extracted\chunk\hm\wp\wp03\mot\w03_00\w03_00.lmt"
DEPLOYED = r"E:\SteamLibrary\steamapps\common\Monster Hunter World\nativePC\hm\wp\wp03\mot\w03_00\w03_00.lmt"
ORIGIN = r"E:\SteamLibrary\steamapps\common\Monster Hunter World\nativePC\hm\wp\wp03\mot\w03_00\w03_00-origin.lmt"


def iter_tracks(buf, entry):
    n, actions = D.parse_lmt(buf)
    act = actions[entry]
    timl = act["timlOffset"]
    if not timl:
        return
    typesOffset, typeCount = struct.unpack_from("<QQ", buf, timl)
    for ti in range(typeCount):
        toff, tcount, tlHash, _ = struct.unpack_from("<QQIi", buf, typesOffset + 24 * ti)
        cat = D.tname(tlHash)
        if not toff:
            continue
        for xi in range(tcount):
            xoff, xcount, dtHash, dataType = struct.unpack_from("<QQIi", buf, toff + 24 * xi)
            tname = D.dname(dtHash)[0]
            if not xoff or not xcount:
                continue
            keys = [(struct.unpack_from("<I", buf, xoff + 20 * k)[0],
                     struct.unpack_from("<f", buf, xoff + 20 * k + 12)[0])
                    for k in range(xcount)]
            yield dict(cat=cat, name=tname, keys=keys)


def bits(v):
    return "".join("1" if v >> i & 1 else "." for i in range(31, -1, -1))


def short(cat):
    return cat.replace("nTimelineParam::", "")


def scan(lmt):
    with open(lmt, "rb") as f:
        buf = f.read()
    n, actions = D.parse_lmt(buf)
    print("== scan mTrigger values 1024/1 (and any mTrigger pair) ==")
    for eid in sorted(actions):
        if not actions[eid]["timlOffset"]:
            continue
        for tr in iter_tracks(buf, eid):
            if tr["cat"].endswith("PlMotionCommon") and tr["name"] == "mTrigger":
                ks = [(round(fr), v) for v, fr in tr["keys"]]
                if any(v in (1, 1024) for _, v in ks):
                    print("  entry %-4d mTrigger %s" % (eid, ks))


def trackset(lmt):
    with open(lmt, "rb") as f:
        buf = f.read()
    n, actions = D.parse_lmt(buf)
    out = {}
    for eid in sorted(actions):
        if not actions[eid]["timlOffset"]:
            continue
        s = set()
        for tr in iter_tracks(buf, eid):
            sig = tuple((round(fr), v) for v, fr in tr["keys"])
            s.add((short(tr["cat"]), tr["name"], sig))
        out[eid] = s
    return out


def diff(a_path, b_path, only=None):
    a, b = trackset(a_path), trackset(b_path)
    print("== diff %s -> %s ==" % (a_path.split("\\")[-1].split("/")[-1],
                                  b_path.split("\\")[-1].split("/")[-1]))
    total_lost = 0
    for eid in sorted(set(a) | set(b)):
        if only and eid not in only:
            continue
        sa, sb = a.get(eid, set()), b.get(eid, set())
        lost, gained = sa - sb, sb - sa
        if lost or gained:
            total_lost += len(lost)
            print("  entry %d: lost=%d gained=%d" % (eid, len(lost), len(gained)))
            for cat, name, sig in sorted(lost):
                print("    - [%s] %s %s" % (cat, name, sig))
            for cat, name, sig in sorted(gained):
                print("    + [%s] %s %s" % (cat, name, sig))
    print("  total lost tracks: %d" % total_lost)


def windows(lmt, entries):
    with open(lmt, "rb") as f:
        buf = f.read()
    n, actions = D.parse_lmt(buf)
    for eid in entries:
        print("== entry %d ==" % eid)
        if eid not in actions or not actions[eid]["timlOffset"]:
            print("  (no timl)")
            continue
        for tr in iter_tracks(buf, eid):
            cat = short(tr["cat"])
            if not (cat.endswith("PlMotionCommon") or cat.endswith("PlMotionInput")):
                continue
            ks = [(fr, v) for v, fr in tr["keys"]]
            seg = "  ".join("f%.0f=%08x[%s]" % (fr, v & 0xFFFFFFFF, bits(v & 0xFFFFFFFF)) for fr, v in ks)
            print("  [%s] %s" % (cat, tr["name"]))
            print("      %s" % seg)


def main():
    cmd = sys.argv[1] if len(sys.argv) > 1 else "all"
    if cmd in ("scan", "all"):
        scan(VANILLA)
    if cmd in ("diff", "all"):
        diff(VANILLA, DEPLOYED)
    if cmd in ("windows", "all"):
        es = [int(x) for x in sys.argv[2].split(",")] if len(sys.argv) > 2 else \
             [338, 341, 345, 104, 105, 306, 312]
        windows(VANILLA, es)


if __name__ == "__main__":
    main()
