# -*- coding: utf-8 -*-
"""Focused TIML window summary: deployed vs vanilla baselines (no Blender)."""
import os
import sys
import dump_timl_frames as D

DEPLOYED = r"E:\SteamLibrary\steamapps\common\Monster Hunter World\nativePC\hm\wp\wp03\mot\w03_00\w03_00.lmt"
VANILLA = r"E:\SteamLibrary\steamapps\common\Monster Hunter World\nativePC\hm\wp\wp03\mot\w03_00\w03_00-origin.lmt"

MFLAG = "mFlag"
MTRIG = "mTrigger"


def as_num(v):
    if isinstance(v, str):
        v = float(v.strip("()").split(",")[0])
    return v


def windows(keys):
    """closed-interval active windows from (frame,value) pairs"""
    wins, cur, start = [], 0, None
    for k in keys:
        f, v = k["frame"], as_num(k["value"])
        on = v != 0
        if on and not cur:
            start, cur = f, v
        elif not on and cur:
            wins.append("[%d,%d]=%s" % (start, f - 1, cur))
            cur = 0
    if cur:
        wins.append("[%d,...]=%s" % (start, cur))
    return " ".join(wins) if wins else "(none)"


def summary(path, entries, title):
    buf = open(path, "rb").read()
    entryCount, actions = D.parse_lmt(buf)
    print("### %s (%s)" % (title, os.path.basename(path)))
    for eid in entries:
        if eid not in actions:
            print("  entry %d: MISSING" % eid)
            continue
        act = actions[eid]
        print("  == entry %d frameCount=%d" % (eid, act["frameCount"]))
        if not act["timlOffset"]:
            print("     (no TIML)")
            continue
        timl = D.parse_timl(buf, act["timlOffset"])
        for typ in timl["types"]:
            short = typ["name"].split("::")[-1]
            for xf in typ["transforms"]:
                if xf["name"] in (MFLAG, MTRIG):
                    print("     [%s] %-10s %s" % (short, xf["name"], windows(xf["keyframes"])))
    print()


if __name__ == "__main__":
    summary(DEPLOYED, [140, 339, 340, 343], "DEPLOYED target entries")
    summary(VANILLA, [103, 104, 105, 107, 140, 339, 340, 343], "VANILLA baselines")
