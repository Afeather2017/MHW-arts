# -*- coding: utf-8 -*-
"""Restore the ORIGINAL shot-family TIML blocks for 339/340/343 (they still hold the
user's attack windows from set_attack_windows.py) and surgically remove only the
shot-weave (强化射击) parts. SR's rs-LMT diff shows replaced moves keep the full TIML
skeleton (mTrigger/PlMotionInput/EventGroups), so the minimal donor swap was too thin
and lost whatever the damage chain needs.

Shot-ectomy per entry:
- PlMotionCommon.mTrigger: f34=1024 -> 0, f36=1 -> 0   (ignition notify pair)
- mSlingerShellAttach: all values -> 0                   (shell attach anim)
- mClawGimmickState: all values -> 0
- mCameraShakeIndex: -> 0                                (shot signature shake)
- EventGroup09.mTrigger: all values -> 0                 (slinger fire sound 7844)
Kept: EC mFlag windows (already user's frames, family group values), EventGroup01/02/03/06
(generic whooshes), PlMotionInput, PlMotionCommon.mFlag, GameParameter.
"""
import os
import shutil
import struct
import sys
import time

sys.path.insert(0, r"E:\MHW-arts")
import dump_timl_frames as D

LMT = r"E:\SteamLibrary\steamapps\common\Monster Hunter World\nativePC\hm\wp\wp03\mot\w03_00\w03_00.lmt"

FAMILY = {339: 0x314170, 340: 0x319D60, 343: 0x33BD70}

# expected EC mFlag windows (from the set_attack_windows run, family group values)
EXPECT_WINDOWS = {
    339: [(29, 34, 1)],
    340: [(17, 22, 1)],
    343: [(31, 39, 1), (58, 70, 2)],
}

ZERO_TRACKS = {  # category substring -> transform name, all key values -> 0
    "nWwiseTimeline::EventGroup09": ["mTrigger"],
}


def tracks(buf, timl):
    typesOffset, typeCount = struct.unpack_from("<QQ", buf, timl)
    for ti in range(typeCount):
        toff, tcount, tlHash, _ = struct.unpack_from("<QQIi", buf, typesOffset + 24 * ti)
        cat = D.tname(tlHash)
        if not toff:
            continue
        for xi in range(tcount):
            xoff, xcount, dtHash, dt = struct.unpack_from("<QQIi", buf, toff + 24 * xi)
            nm = D.dname(dtHash)[0]
            if not xoff or not xcount or dt not in (0, 1):
                continue
            fmt = "<i" if dt == 0 else "<I"
            keys = [dict(offset=xoff + 20 * k,
                         value=struct.unpack_from(fmt, buf, xoff + 20 * k)[0],
                         frame=struct.unpack_from("<f", buf, xoff + 20 * k + 12)[0])
                    for k in range(xcount)]
            yield cat, nm, keys


def find(buf, timl, cat_sub, name):
    hits = [(c, n, k) for c, n, k in tracks(buf, timl) if cat_sub in c and n == name]
    return hits


def main():
    buf = bytearray(open(LMT, "rb").read())
    n, actions = D.parse_lmt(buf)

    # 1) verify family blocks are the expected ones (EC windows already user's)
    for eid, off in FAMILY.items():
        ec = find(buf, off, "EventCollision00", "mFlag")
        assert len(ec) == 1, "entry %d: EC x%d" % (eid, len(ec))
        wins, cur, start = [], 0, None
        for k in ec[0][2]:
            if k["value"] and not cur:
                start, cur = k["frame"], k["value"]
            elif not k["value"] and cur:
                wins.append((start, k["frame"] - 1, cur)); cur = 0
        assert wins == EXPECT_WINDOWS[eid], "entry %d windows %s != %s" % (eid, wins, EXPECT_WINDOWS[eid])
        assert find(buf, off, "PlMotionInput", "mFlag"), "entry %d: family PlMotionInput missing" % eid
        print("entry %d: family block @0x%X verified, windows %s" % (eid, off, wins))

    # 2) repoint headers back to the family blocks
    for eid, off in FAMILY.items():
        struct.pack_into("<Q", buf, actions[eid]["offset"] + 88, off)
        print("entry %d: timlOffset -> 0x%X" % (eid, off))

    # 3) shot-ectomy on the family blocks
    for eid, off in FAMILY.items():
        for cat, name, keys in tracks(buf, off):
            if cat.endswith("PlMotionCommon") and name == "mTrigger":
                for k in keys:
                    if k["value"] in (1024, 1) and k["frame"] > 0:  # keep f0=0 anchor key
                        print("entry %d mTrigger f%.0f: %d -> 0" % (eid, k["frame"], k["value"]))
                        struct.pack_into("<I", buf, k["offset"], 0)
            elif cat.endswith("PlMotionCommon") and name in ("mSlingerShellAttach", "mClawGimmickState", "mCameraShakeIndex"):
                for k in keys:
                    if k["value"]:
                        print("entry %d %s f%.0f: %d -> 0" % (eid, name, k["frame"], k["value"]))
                        struct.pack_into("<I", buf, k["offset"], 0)
            else:
                for sub, names in ZERO_TRACKS.items():
                    if sub in cat and name in names:
                        for k in keys:
                            if k["value"]:
                                print("entry %d [%s] %s f%.0f: %d -> 0" % (eid, cat.split('::')[-1], name, k["frame"], k["value"]))
                                struct.pack_into("<I", buf, k["offset"], 0)

    backup = LMT + ".bak-" + time.strftime("%Y%m%d-%H%M%S") + "-familytiml"
    shutil.copy2(LMT, backup)
    with open(LMT, "r+b") as h:
        h.write(buf)
    print("backup: %s" % os.path.basename(backup))

    # 4) verify from disk
    buf2 = open(LMT, "rb").read()
    n, actions = D.parse_lmt(buf2)
    for eid in (339, 340, 343):
        off = actions[eid]["timlOffset"]
        print("== entry %d timl@0x%X ==" % (eid, off))
        ign = shell = 0
        for cat, name, keys in tracks(buf2, off):
            if cat.endswith("PlMotionCommon") and name == "mTrigger":
                vals = [(round(k["frame"]), k["value"]) for k in keys]
                ign = sum(1 for _, v in vals if v in (1024, 1) and _ > 0)
                print("   mTrigger: %s" % vals)
            if cat.endswith("PlMotionCommon") and name in ("mSlingerShellAttach", "mClawGimmickState", "mCameraShakeIndex"):
                shell += sum(1 for k in keys if k["value"])
            if cat.endswith("EventCollision00") and name == "mFlag":
                wins, cur, start = [], 0, None
                for k in keys:
                    if k["value"] and not cur:
                        start, cur = k["frame"], k["value"]
                    elif not k["value"] and cur:
                        wins.append("[%d,%d]=g%d" % (start, k["frame"] - 1, cur)); cur = 0
                print("   attack windows: %s" % " ".join(wins))
            if cat.endswith("PlMotionInput"):
                print("   [%s] %s present (%d keys)" % (cat.split('::')[-1], name, len(keys)))
        print("   shot residue: ignition=%d shell/claw/shake=%d  -> %s" % (ign, shell, "CLEAN" if ign == 0 and shell == 0 else "STILL DIRTY"))


if __name__ == "__main__":
    main()
