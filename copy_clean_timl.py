# -*- coding: utf-8 -*-
"""Replace the shot-weave TIML of entries 339/340/343 with clean vanilla attack TIMLs.

The shot family TIML (mSlingerShellAttach / mClawGimmickState / mTrigger f34=1024+f36=1
/ EventGroup06+09 shot sounds) rides on slots 332-349; user-verified that swapping in
another move's TIML removes the built-in 强化射击.

Donors: 130 (single-window clean attack) -> 339/340; 113 (two-window clean attack) -> 343.
Each target gets an INDEPENDENT copy of the donor block appended at EOF (pointers fixed),
TIML_Data timing fields (animationLength/loopStart/loopControl) taken from the target's
old block, then attack windows re-applied (closed intervals -> on@a, off@b+1).
"""
import os
import shutil
import struct
import sys
import time

sys.path.insert(0, r"E:\MHW-arts")
import dump_timl_frames as D

LMT = r"E:\SteamLibrary\steamapps\common\Monster Hunter World\nativePC\hm\wp\wp03\mot\w03_00\w03_00.lmt"

COPIES = [(339, 130), (340, 130), (343, 113)]

# (entry, [(old_frame, new_frame, expect_value)]) on EventCollision00.mFlag of the NEW block
WINDOWS = [
    (339, [(3, 29, 2), (10, 35, 0)]),              # 一文字长 [29,34] g2
    (340, [(3, 17, 2), (10, 23, 0)]),              # 一文字短 [17,22] g2
    (343, [(3, 31, 2), (8, 40, 0), (23, 58, 1), (31, 71, 0)]),  # 无双斩 [31,39] g2 + [58,70] g1
]

SHOT_MARKERS = ("mSlingerShellAttach", "mClawGimmickState")


def align16(p):
    return p + ((-p) % 16)


def walk_block(buf, timl):
    """Return (extent_end, [(file_pos_of_qword_ptr, )...], meta) for the TIML block."""
    ptrs = [timl + 0]  # typesOffset
    typesOffset, typeCount = struct.unpack_from("<QQ", buf, timl)
    end = timl + 40
    end = max(end, typesOffset + 24 * typeCount)
    for ti in range(typeCount):
        toff, tcount, _h, _u = struct.unpack_from("<QQIi", buf, typesOffset + 24 * ti)
        if not toff:
            continue
        ptrs.append(typesOffset + 24 * ti)  # the toff field itself
        end = max(end, toff + 24 * tcount)
        for xi in range(tcount):
            xoff, xcount, _dh, _dt = struct.unpack_from("<QQIi", buf, toff + 24 * xi)
            if not xoff:
                continue
            ptrs.append(toff + 24 * xi)  # the xoff field itself
            end = max(end, xoff + 20 * xcount)
    return end, ptrs


def read_meta(buf, timl):
    animLen, loopStart, loopCtrl = struct.unpack_from("<ffi", buf, timl + 24)
    return dict(animLen=animLen, loopStart=loopStart, loopCtrl=loopCtrl)


def iter_tracks(buf, entry, actions):
    act = actions[entry]
    timl = act["timlOffset"]
    typesOffset, typeCount = struct.unpack_from("<QQ", buf, timl)
    for ti in range(typeCount):
        toff, tcount, tlHash, _ = struct.unpack_from("<QQIi", buf, typesOffset + 24 * ti)
        cat = D.tname(tlHash)
        if not toff:
            continue
        for xi in range(tcount):
            xoff, xcount, dtHash, dataType = struct.unpack_from("<QQIi", buf, toff + 24 * xi)
            tname = D.dname(dtHash)[0]
            if not xoff or not xcount or dataType != 1:
                continue
            keys = [dict(offset=xoff + 20 * k,
                         value=struct.unpack_from("<I", buf, xoff + 20 * k)[0],
                         frame=struct.unpack_from("<f", buf, xoff + 20 * k + 12)[0])
                    for k in range(xcount)]
            yield dict(cat=cat, name=tname, keys=keys)


def main():
    buf = bytearray(open(LMT, "rb").read())
    n, actions = D.parse_lmt(buf)

    # snapshot target meta before repointing
    target_meta = {t: read_meta(buf, actions[t]["timlOffset"]) for t, _ in COPIES}

    for target, donor in COPIES:
        d_timl = actions[donor]["timlOffset"]
        end, ptrs = walk_block(buf, d_timl)
        new_start = align16(len(buf))
        delta = new_start - d_timl
        block = bytes(buf[d_timl:end])
        buf.extend(b"\x00" * (new_start - len(buf)))  # land the block exactly on new_start
        buf += block
        for pos in ptrs:
            p = pos + delta  # patch the COPY's pointer fields, not the donor's
            old = struct.unpack_from("<Q", buf, p)[0]
            if old:
                struct.pack_into("<Q", buf, p, old + delta)
        # timing meta from the target's own animation
        meta = target_meta[target]
        struct.pack_into("<ffi", buf, new_start + 24, meta["animLen"], meta["loopStart"], meta["loopCtrl"])
        # repoint target action header
        struct.pack_into("<Q", buf, actions[target]["offset"] + 88, new_start)
        print("entry %d: TIML <- entry %d block [0x%X..0x%X) -> 0x%X (delta %d), meta animLen=%.0f" %
              (target, donor, d_timl, end, new_start, delta, meta["animLen"]))

    # re-apply attack windows on the fresh blocks
    n, actions = D.parse_lmt(buf)
    for target, moves in WINDOWS:
        tracks = [t for t in iter_tracks(buf, target, actions)
                  if t["cat"].endswith("EventCollision00") and t["name"] == "mFlag"]
        assert len(tracks) == 1, "entry %d: EC mFlag x%d" % (target, len(tracks))
        track = tracks[0]
        for old, new, expect in moves:
            keys = [k for k in track["keys"] if abs(k["frame"] - old) < 0.01]
            assert len(keys) == 1 and keys[0]["value"] == expect, \
                "entry %d: f%d expect %d got %s" % (target, old, expect, keys)
            struct.pack_into("<f", buf, keys[0]["offset"] + 12, float(new))
            print("entry %d EC mFlag: f%d -> f%d (g%d)" % (target, old, new, expect))
        frames = [k["frame"] for k in track["keys"]]
        assert all(a <= b for a, b in zip(frames, frames[1:])), "unsorted entry %d" % target

    backup = LMT + ".bak-" + time.strftime("%Y%m%d-%H%M%S") + "-cleantiml"
    shutil.copy2(LMT, backup)
    with open(LMT, "r+b") as h:
        h.write(buf)
    print("backup: %s" % os.path.basename(backup))

    # verify from disk: no shot markers, windows correct, donor untouched
    buf2 = open(LMT, "rb").read()
    n, actions = D.parse_lmt(buf2)
    for target in (339, 340, 343, 130, 113):
        print("== entry %d ==" % target)
        bad = False
        for t in iter_tracks(buf2, target, actions):
            if t["cat"].endswith("PlMotionCommon") and t["name"] in SHOT_MARKERS:
                print("   SHOT MARKER PRESENT: %s" % t["name"]); bad = True
            if t["cat"].endswith("PlMotionCommon") and t["name"] == "mTrigger":
                vals = [(round(k["frame"]), k["value"]) for k in t["keys"]]
                if any(v == 1024 for _, v in vals):
                    print("   SHOT IGNITION PAIR PRESENT: %s" % vals); bad = True
            if t["cat"].endswith("EventCollision00") and t["name"] == "mFlag":
                wins, cur, start = [], 0, None
                for k in t["keys"]:
                    if k["value"] and not cur:
                        start, cur = k["frame"], k["value"]
                    elif not k["value"] and cur:
                        wins.append("[%d,%d]=g%d" % (start, k["frame"] - 1, cur)); cur = 0
                if cur:
                    wins.append("[%d,..]=g%d" % (start, cur))
                print("   attack windows: %s" % (" ".join(wins) or "(none)"))
        if not bad and target in (339, 340, 343):
            print("   clean: no shot markers")


if __name__ == "__main__":
    main()
