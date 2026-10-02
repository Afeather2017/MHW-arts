# -*- coding: utf-8 -*-
"""Set EventCollision00 damage windows for the valor LS moves (2026-10-02).

Attack window track = nWwiseTimeline::EventCollision00.mFlag (0=off, 1/2/3=collision
slot group). User frames are CLOSED intervals [a,b]: on-key at a, off-key at b+1.
Unused donor windows are zeroed (value -> 0) per "no unlisted frames" rule.
Entry 140 also: CollisionTimelineObject envelope moved alongside (vanilla kept a
2-frame margin around the damage window) and PlMotionCommon b26 i-frame bit cleared
(刚1 inherits vanilla 见切 iframes f0-130 otherwise).
"""
import os
import shutil
import struct
import sys
import time

import dump_timl_frames as D
from patch_timl_defense import iter_tracks

LMT = r"E:\SteamLibrary\steamapps\common\Monster Hunter World\nativePC\hm\wp\wp03\mot\w03_00\w03_00.lmt"

EC = "nWwiseTimeline::EventCollision00"   # category substring
CTO = "CollisionTimelineObject"           # category substring
PMC = "PlMotionCommon"                    # category substring

# (entry, category_substr, transform, [(op, args...)])  op: move old->new expect | setval frame val expect
PLAN = [
    (140, EC, "mFlag", [
        ("move", 72, 45, 2),          # real 见切 slash window (group 2) -> 刚1 [45,55]
        ("move", 78, 56, 0),
        ("setval", 3, 0, 4),          # zero every other donor window
        ("setval", 9, 0, 1),
        ("setval", 31, 0, 2),
        ("setval", 83, 0, 1),
        ("setval", 136, 0, 1),
        ("setval", 163, 0, 2),
    ]),
    (140, CTO, "mFlag", [
        ("move", 70, 43, 1),          # envelope with vanilla's 2-frame margin
        ("move", 80, 58, 0),
    ]),
    (140, PMC, "mFlag", [
        ("setval", 0, 2147483648, 2151677952),   # 0x80400000 -> 0x80000000: clear b26 iframes, keep b31
    ]),
    (340, EC, "mFlag", [              # 一文字短 [17,22], group 1
        ("move", 9, 17, 1),
        ("move", 18, 23, 0),
        ("setval", 19, 0, 2),
        ("move", 19, 24, 0),          # keep array sorted (f24 > f23)
        ("move", 27, 30, 0),
        ("setval", 94, 0, 1),
        ("setval", 112, 0, 2),
    ]),
    (339, EC, "mFlag", [              # 一文字长 [29,34], group 1
        ("move", 5, 29, 1),
        ("move", 13, 35, 0),
        ("setval", 14, 0, 2),
        ("move", 14, 36, 0),
        ("move", 22, 37, 0),
        ("setval", 94, 0, 1),
        ("setval", 111, 0, 2),
    ]),
    (343, EC, "mFlag", [              # 无双斩 [31,39] g1 + [58,70] g2
        ("move", 11, 31, 1),
        ("move", 18, 40, 0),
        ("move", 21, 58, 2),
        ("move", 29, 71, 0),
        ("setval", 94, 0, 1),
        ("setval", 109, 0, 2),
    ]),
]


def find_track(buf, entry, cat_sub, transform):
    matches = [t for t in iter_tracks(buf, entry)
               if cat_sub.lower() in t["cat"].lower() and t["name"] == transform]
    if len(matches) != 1:
        raise SystemExit("entry %d: %d tracks match [%s] %s" % (entry, len(matches), cat_sub, transform))
    return matches[0]


def key_at(track, frame):
    for k in track["keys"]:
        if abs(k["frame"] - frame) < 0.01:
            return k
    raise SystemExit("no key at frame %s in [%s] %s" % (frame, track["cat"], track["name"]))


def main():
    buf = bytearray(open(LMT, "rb").read())
    for entry, cat_sub, transform, ops in PLAN:
        track = find_track(buf, entry, cat_sub, transform)
        for op in ops:
            if op[0] == "move":
                _, old, new, expect = op
                k = key_at(track, old)
                if expect is not None and k["value"] != expect:
                    raise SystemExit("entry %d [%s] %s: f%.0f value %d != expect %s" %
                                     (entry, cat_sub, transform, old, k["value"], expect))
                struct.pack_into("<f", buf, k["offset"] + 12, float(new))
                k["frame"] = float(new)
                print("entry %-3d [%s] %s: f%.0f -> f%.0f (value=%d)" %
                      (entry, cat_sub, transform, old, new, k["value"]))
            else:
                _, frame, val, expect = op
                k = key_at(track, frame)
                if expect is not None and k["value"] != expect:
                    raise SystemExit("entry %d [%s] %s: f%.0f value %d != expect %s" %
                                     (entry, cat_sub, transform, frame, k["value"], expect))
                struct.pack_into("<I", buf, k["offset"], val)
                k["value"] = val
                print("entry %-3d [%s] %s: f%.0f value -> %d" % (entry, cat_sub, transform, frame, val))
        frames = [k["frame"] for k in track["keys"]]
        assert all(a <= b for a, b in zip(frames, frames[1:])), "unsorted keys entry %d" % entry

    backup = LMT + ".bak-" + time.strftime("%Y%m%d-%H%M%S") + "-attackframes"
    shutil.copy2(LMT, backup)
    with open(LMT, "r+b") as h:
        h.write(buf)
    print("backup: %s" % os.path.basename(backup))
    print("written: %s" % LMT)

    # verify pass: reparse from disk and print the resulting windows
    buf2 = open(LMT, "rb").read()
    for entry in (140, 340, 339, 343):
        print("== entry %d ==" % entry)
        for cat_sub, transform in ((EC, "mFlag"), (CTO, "mFlag"), (PMC, "mFlag")):
            try:
                track = find_track(buf2, entry, cat_sub, transform)
            except SystemExit:
                continue
            wins, cur = [], 0
            start = None
            for f, v in [(k["frame"], k["value"]) for k in track["keys"]]:
                if v and not cur:
                    start, cur = f, v
                elif not v and cur:
                    wins.append("[%d,%d]=g%d" % (start, f - 1, cur))
                    cur = 0
            if cur:
                wins.append("[%d,...]=g%d" % (start, cur))
            print("  [%s] %s: %s" % (cat_sub, transform, " ".join("f%.0f=%d" % (k["frame"], k["value"]) for k in track["keys"])))
            print("      active windows: %s" % (" ".join(wins) if wins else "(none)"))


if __name__ == "__main__":
    main()
