# -*- coding: utf-8 -*-
"""One-shot valor attack config for a FRESH blend export of w03_00.lmt.

Run after every re-export. Chain per entry (2026-10-02 verified recipe):
  1. shot-ectomy (强化射击 removal): mTrigger 1024/1 ignition pair -> 0,
     mSlingerShellAttach / mClawGimmickState / mCameraShakeIndex -> 0,
     EventGroup09 (slinger fire sound) triggers -> 0
  2. EventCollision00.mFlag damage-request windows -> user frames, group v2
     (vanilla damage-bearing windows are all v2; v1 = whoosh slots)
  3. CollisionTimelineObject (THE damage window, proof: xfsm+rs entry 339
     CTO[23,34] == observed damage [24,34]) cloned byte-faithfully from the
     xfsm+rs reference entry 339 (dual config node 59 + 72), frames patched
  4. entry 140 (刚1): EC v2 window -> [45,55], CTO -> [43,58], other donor
     windows zeroed, PlMotionCommon b26 (inherited foresight iframes) cleared

Window assignment (user-corrected 2026-10-02): action 105 = entry 340 (126f)
= 一文字长 [29,34]; action 106 = entry 339 (114f) = 一文字短 [17,22];
343 = 无双斩 [31,39]+[58,70].
"""
import os
import shutil
import struct
import sys
import time

sys.path.insert(0, r"E:\MHW-arts")
import dump_timl_frames as D

LMT = r"E:\SteamLibrary\steamapps\common\Monster Hunter World\nativePC\hm\wp\wp03\mot\w03_00\w03_00.lmt"
REF = r"E:\SteamLibrary\steamapps\common\Monster Hunter World\nativePC -xfsm+rs\hm\wp\wp03\mot\w03_00\w03_00.lmt"

# entry -> (closed-interval attack windows, donor EC mFlag on-key frame to repurpose)
PLAN = {
    339: ([(17, 23)], 5),                 # 一文字短
    340: ([(29, 35)], 9),                 # 一文字长
    343: ([(31, 40), (58, 71)], 11),      # 无双斩
}

SHOT_FLOAT_TRACKS = ("mSlingerShellAttach", "mClawGimmickState", "mCameraShakeIndex")


def align16(p):
    return p + ((-p) % 16)


def iter_tracks(buf, timl):
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


def find_one(buf, timl, cat_sub, name):
    hits = [(c, n, k) for c, n, k in iter_tracks(buf, timl) if cat_sub in c and n == name]
    assert len(hits) == 1, "x%d matches for [%s] %s" % (len(hits), cat_sub, name)
    return hits[0]


def main():
    ref = open(REF, 'rb').read()
    _, racts = D.parse_lmt(ref)
    rtypesOffset, rtypeCount = struct.unpack_from("<QQ", ref, racts[339]["timlOffset"])
    r_cto = None
    for ti in range(rtypeCount):
        toff, tcount, h, u = struct.unpack_from("<QQIi", ref, rtypesOffset + 24 * ti)
        if toff and D.tname(h).endswith("CollisionTimelineObject"):
            r_cto = (toff, tcount, h, u)
    rtoffee, rtc, rth, ru = r_cto
    order, rkeys = [], {}
    for xi in range(rtc):
        xoff, xcount, dh, dt = struct.unpack_from("<QQIi", ref, rtoffee + 24 * xi)
        nm = D.dname(dh)[0]
        order.append(nm)
        rkeys[nm] = bytes(ref[xoff:xoff + 20 * xcount])

    buf = bytearray(open(LMT, "rb").read())
    n, actions = D.parse_lmt(buf)

    # ---- shot family: eectomy + EC windows + CTO clone ----
    for eid, (wins, donor_on_frame) in PLAN.items():
        timl = actions[eid]["timlOffset"]
        # 1) shot-ectomy
        for cat, nm, keys in iter_tracks(buf, timl):
            if cat.endswith("PlMotionCommon") and nm == "mTrigger":
                for k in keys:
                    if k["value"] in (1024, 1) and k["frame"] > 0:
                        struct.pack_into("<I", buf, k["offset"], 0)
            elif cat.endswith("PlMotionCommon") and nm in SHOT_FLOAT_TRACKS:
                for k in keys:
                    if k["value"]:
                        struct.pack_into("<I", buf, k["offset"], 0)
            elif "EventGroup09" in cat and nm == "mTrigger":
                for k in keys:
                    if k["value"]:
                        struct.pack_into("<I", buf, k["offset"], 0)
        # 2) EC mFlag windows (group v2). Idempotent: if already configured
        #    (nonzero keys sit exactly on the planned window bounds), just
        #    refresh the group value; else consume donor key-pairs.
        _, nm, eck = find_one(buf, timl, "EventCollision00", "mFlag")
        eck_sorted = sorted(eck, key=lambda k: k["frame"])
        nz = [k for k in eck_sorted if k["value"] != 0]
        want_bounds = {float(f) for w in wins for f in w}
        configured = len(nz) == len(wins) and all(
            any(abs(k["frame"] - fb) < 0.01 for fb in want_bounds) for k in nz)
        if configured:
            for k in nz:
                struct.pack_into("<I", buf, k["offset"], 2)
            print("entry %d: EC v2 windows %s" % (eid, wins))
        else:
            assert len(nz) >= 2 * len(wins), "entry %d: %d donor windows < %d needed" % (
                eid, len(nz) // 2, len(wins))
            slot_frames = [None] * len(eck_sorted)
            used_slots = set()
            for w_i, (on_f, off_f) in enumerate(wins):
                onk = nz[w_i * 2]
                later0 = [k for k in eck_sorted
                          if k["frame"] > onk["frame"] + 0.01 and k["value"] == 0]
                assert later0, "entry %d: no off-key after f%.0f" % (eid, onk["frame"])
                offk = min(later0, key=lambda k: k["frame"])
                struct.pack_into("<I", buf, onk["offset"], 2)   # group v2 = reference match
                slot_frames[eck_sorted.index(onk)] = float(on_f)
                slot_frames[eck_sorted.index(offk)] = float(off_f)
                used_slots.update((id(onk), id(offk)))
            last = 0.0
            i = 0
            while i < len(slot_frames):
                if slot_frames[i] is None:
                    j = i
                    while j < len(slot_frames) and slot_frames[j] is None:
                        j += 1
                    nxt = slot_frames[j] if j < len(slot_frames) else last + (j - i) + 1
                    step = max(1, int(nxt - last) // (j - i + 1))
                    for t in range(i, j):
                        v = int(last) + step * (t - i + 1)
                        if j < len(slot_frames) and v >= nxt:
                            v = int(nxt) - 1
                        slot_frames[t] = float(v)
                    if j < len(slot_frames):
                        last = slot_frames[j]
                    i = j + 1
                else:
                    last = slot_frames[i]
                    i += 1
            for k, fr in zip(eck_sorted, slot_frames):
                assert fr == int(fr), "entry %d: fractional frame %s" % (eid, fr)
                struct.pack_into("<f", buf, k["offset"] + 12, fr)
                if id(k) not in used_slots and k["value"]:
                    struct.pack_into("<I", buf, k["offset"], 0)   # leftovers inert
            assert all(a <= b for a, b in zip(slot_frames, slot_frames[1:])), \
                "entry %d: unsorted EC keys" % eid
        print("entry %d: EC v2 windows %s" % (eid, wins))
        # 3) CTO clone (dual config 59/72) with user windows; idempotent:
        #    if already cloned, just refresh the mFlag window frames
        typesOffset, typeCount = struct.unpack_from("<QQ", buf, timl)
        pos = toff = tcount = th = tu = None
        for ti in range(typeCount):
            p2 = typesOffset + 24 * ti
            t2, c2, h2, u2 = struct.unpack_from("<QQIi", buf, p2)
            if t2 and D.tname(h2).endswith("CollisionTimelineObject"):
                pos, toff, tcount, th, tu = p2, t2, c2, h2, u2
        if pos is not None:
            _, _, ctk = find_one(buf, timl, "CollisionTimelineObject", "mFlag")
            want = [0.0] + [float(f) for w in wins for f in w]
            assert len(ctk) == len(want), "entry %d CTO keys %d != %d" % (eid, len(ctk), len(want))
            for k, fr in zip(ctk, want):
                struct.pack_into("<f", buf, k["offset"] + 12, fr)
            print("entry %d: CTO exists, windows refreshed %s" % (eid, wins))
            continue
        r0, r1, r2 = rkeys["mFlag"][:20], rkeys["mFlag"][20:40], rkeys["mFlag"][40:60]
        mf = bytearray(r0)
        for on_f, off_f in wins:
            a = bytearray(r1); struct.pack_into("<f", a, 12, float(on_f)); mf += a
            b = bytearray(r2); struct.pack_into("<f", b, 12, float(off_f)); mf += b
        newkeys = [rkeys["バンク[0]"], rkeys["ノードID[0]"], rkeys["バンク[1]"],
                   rkeys["ノードID[1]"], bytes(mf)]
        start = align16(len(buf))
        buf.extend(b"\x00" * (start - len(buf)))
        new_types_pos = start
        buf += bytes(buf[typesOffset:typesOffset + 24 * typeCount])  # copy types array!
        buf += b"\x00" * 24                                          # slot for the CTO type
        xf_pos = start + 24 * (typeCount + 1)
        xf = bytearray(ref[rtoffee:rtoffee + 24 * rtc])
        buf += xf
        p = xf_pos + len(xf)
        for idx, blob in enumerate(newkeys):
            struct.pack_into("<QQ", xf, 24 * idx, p, len(blob) // 20)
            buf += blob
            p += len(blob)
        buf[xf_pos:xf_pos + len(xf)] = xf
        # CTO type entry into the copied array's new slot; retarget TIML_Data
        struct.pack_into("<QQIi", buf, new_types_pos + 24 * typeCount,
                         xf_pos, len(order), rth, ru)
        struct.pack_into("<QQ", buf, timl, new_types_pos, typeCount + 1)
        print("entry %d: CTO node 59/72 cloned, windows %s" % (eid, wins))

    # ---- entry 140 (刚1): vanilla foresight TIML -> attack [45,55] (idempotent) ----
    timl = actions[140]["timlOffset"]
    _, _, eck = find_one(buf, timl, "EventCollision00", "mFlag")
    eck_s = sorted(eck, key=lambda k: k["frame"])
    nz140 = [k for k in eck_s if k["value"]]
    if len(nz140) == 1:
        onk = nz140[0]                      # already configured: single v2 window
    else:
        cand = [k for k in nz140 if abs(k["frame"] - 72) < 0.01]
        assert cand, "140: donor slash window (f72 v2) not found"
        onk = cand[0]
    struct.pack_into("<I", buf, onk["offset"], 2)
    struct.pack_into("<f", buf, onk["offset"] + 12, 45.0)
    later = [k for k in eck_s if k["frame"] > 45.01 and k["value"] == 0]
    assert later, "140: no off-key after the slash window"
    offk = min(later, key=lambda k: k["frame"])
    struct.pack_into("<f", buf, offk["offset"] + 12, 56.0)
    for k in eck_s:
        if k["value"] and id(k) != id(onk):
            struct.pack_into("<I", buf, k["offset"], 0)
    _, _, ctk = find_one(buf, timl, "CollisionTimelineObject", "mFlag")
    for k in ctk:
        if abs(k["frame"] - 70) < 0.01:
            struct.pack_into("<f", buf, k["offset"] + 12, 43.0)
        elif abs(k["frame"] - 80) < 0.01:
            struct.pack_into("<f", buf, k["offset"] + 12, 58.0)
    _, _, pmk = find_one(buf, timl, "PlMotionCommon", "mFlag")
    for k in pmk:
        if k["frame"] == 0 and k["value"] == 2151677952:
            struct.pack_into("<I", buf, k["offset"], 2147483648)  # clear b26
    print("entry 140: EC v2 [45,55], CTO [43,58], b26 cleared")

    # ---- input windows (PlMotionInput) ------------------------------------
    # entry 140 刚1: 気刃斬り1 (entry 104) skeleton; roll (b8) and next-move
    # (full window) both open at f56 per user spec 2026-10-03. Values are the
    # vanilla acceptance sets; mFlag1/mFlag2 companion tracks follow.
    INPUT_PLAN = {
        # 刚1: 気刃斬り1 skeleton, roll/next-move at 56, tail closes at 110/121
        140: {
            "keys": {
                "mFlag":  [(0, 0x00000000), (36, 0x40014000), (56, 0xC003C300),
                           (110, 0x00000300), (121, 0x00000301)],
                "mFlag1": [(0, 0x00000000), (36, 0x04001000), (56, 0x0C003000),
                           (110, 0x00000000)],
                "mFlag2": [(0, 0x00000000), (36, 0x00400150), (56, 0x00C003F0),
                           (110, 0x00000000)],
            },
        },
        # 一文字长 340: roll/next-move from [38,...); buffer 24; open-ended
        340: {"open": True, "keys": {
            "mFlag":  [(0, 0x00000000), (24, 0x40014000), (38, 0xC003C300)],
            "mFlag1": [(0, 0x00000000), (24, 0x04001000), (38, 0x0C003000)],
            "mFlag2": [(0, 0x00000000), (24, 0x00400150), (38, 0x00C003F0)],
        }},
        # 一文字短 339: roll/next-move from [26,...); buffer 17; open-ended
        339: {"open": True, "keys": {
            "mFlag":  [(0, 0x00000000), (17, 0x40014000), (26, 0xC003C300)],
            "mFlag1": [(0, 0x00000000), (17, 0x04001000), (26, 0x0C003000)],
            "mFlag2": [(0, 0x00000000), (17, 0x00400150), (26, 0x00C003F0)],
        }},
        # 无双斩 343: roll/next-move from [92,...); buffer 59; open-ended
        343: {"open": True, "keys": {
            "mFlag":  [(0, 0x00000000), (59, 0x40014000), (92, 0xC003C300)],
            "mFlag1": [(0, 0x00000000), (59, 0x04001000), (92, 0x0C003000)],
            "mFlag2": [(0, 0x00000000), (59, 0x00400150), (92, 0x00C003F0)],
        }},
    }
    for eid, plan in INPUT_PLAN.items():
        tracks, open_ended = plan["keys"], plan.get("open", False)
        timl = actions[eid]["timlOffset"]
        for track_name, new_keys in tracks.items():
            _, _, keys = find_one(buf, timl, "PlMotionInput", track_name)
            ks = sorted(keys, key=lambda k: k["frame"])
            assert len(ks) >= len(new_keys), "entry %d %s: %d keys < %d" % (
                eid, track_name, len(ks), len(new_keys))
            frames = [float(f) for f, _ in new_keys]
            for k, (fr, v) in zip(ks, new_keys):
                struct.pack_into("<I", buf, k["offset"], v)
                struct.pack_into("<f", buf, k["offset"] + 12, fr)
            leftovers = ks[len(new_keys):]
            last_v = new_keys[-1][1]
            for j, k in enumerate(leftovers):
                # open-ended: leftovers carry the last acceptance set so the
                # window runs to the move's end; otherwise they close it
                struct.pack_into("<I", buf, k["offset"], last_v if open_ended else 0)
                struct.pack_into("<f", buf, k["offset"] + 12,
                                 frames[-1] + 10.0 * (j + 1))
            allfr = frames + [frames[-1] + 10.0 * (j + 1) for j in range(len(leftovers))]
            assert all(a <= b for a, b in zip(allfr, allfr[1:])) and \
                all(f == int(f) for f in allfr), "entry %d %s input keys" % (eid, track_name)
        print("entry %d: input windows applied (%s)%s" % (
            eid, ", ".join(tracks), " open-ended" if open_ended else ""))

    frames_sorted = True
    backup = LMT + ".bak-" + time.strftime("%Y%m%d-%H%M%S") + "-valorcfg"
    shutil.copy2(LMT, backup)
    with open(LMT, "r+b") as h:
        h.write(buf)
    print("backup:", os.path.basename(backup))

    # ---- verify from disk ----
    buf2 = open(LMT, "rb").read()
    n, actions = D.parse_lmt(buf2)
    for eid in list(PLAN) + [140]:
        timl = actions[eid]["timlOffset"]
        ign = 0
        for cat, nm, keys in iter_tracks(buf2, timl):
            if cat.endswith("PlMotionCommon") and nm == "mTrigger":
                ign += sum(1 for k in keys if k["value"] in (1024, 1) and k["frame"] > 0)
            if cat.endswith("EventCollision00") and nm == "mFlag":
                wins, cur, start = [], 0, None
                for k in keys:
                    if k["value"] and not cur:
                        start, cur = k["frame"], k["value"]
                    elif not k["value"] and cur:
                        wins.append("[%d,%d]=g%d" % (start, k["frame"] - 1, cur)); cur = 0
                print("entry %d EC: %s" % (eid, " ".join(wins) or "(none)"))
            if cat.endswith("CollisionTimelineObject") and nm == "mFlag":
                wins, cur, start = [], 0, None
                for k in keys:
                    if k["value"] and not cur:
                        start, cur = k["frame"], k["value"]
                    elif not k["value"] and cur:
                        wins.append("[%d,%d]" % (start, k["frame"] - 1)); cur = 0
                print("entry %d CTO: %s" % (eid, " ".join(wins) or "(none)"))
        if eid in PLAN and ign:
            print("entry %d: IGNITION RESIDUE %d" % (eid, ign))


if __name__ == "__main__":
    main()
