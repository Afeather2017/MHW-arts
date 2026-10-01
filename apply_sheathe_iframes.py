# -*- coding: utf-8 -*-
"""Apply the 特殊纳刀-family i-frame TIML recipe to w03_00.lmt (idempotent).

Recipe (empirically closed on 307/308, see LMT_TIML_Hitbox_IFrame_SuperArmor_*
  research doc): i-frames = PlMotionCommon mFlag b26 (0x04000000) window
  + one PlMotionCommon mTrigger key 2048 inside the window. Super armor is
  b24 (0x01000000); when both on, no-hit wins. Existing b25/b22/b1|b2 are kept.

Edits:
  entry 306 (特殊纳刀, action 98): b26 OR-ed into mFlag f0/f10/f15 (whole-motion
    window, no clear key), mTrigger f0 = 2048.
  entry 312 (action 100): b26 OR-ed into mFlag f0 (whole motion), and a
    PlMotionCommon mTrigger track {f0=2048} is STRUCTURALLY INSERTED
    (append-at-EOF + pointer rewrite; the entry's types/transforms/keys arrays
    are verified unshared first).

Re-run after every blend export that rewrites these entries:
  python apply_sheathe_iframes.py <lmt>
"""
import os
import shutil
import struct
import sys

import dump_timl_frames as D

B26 = 0x04000000
TRIG_DT_HASH = 0x99897C43          # mTrigger datatype hash (read from 307)
KEY_SIZE = 20

# canonical mTrigger key @f0 value 2048, copied from 307's key layout:
# value u32 | controlL f32 | controlR f32 | frame f32 | transition i16 | easing i16
TRIG_KEY_2048_F0 = bytes.fromhex("00080000" "00000000" "00000000" "00000000" "06000100")


def align16(pos):
    return pos + ((-pos) % 16)


def append_aligned(buf, data):
    """Pad buffer to 16-byte alignment, then append data. Returns data offset."""
    pad = (-len(buf)) % 16
    if pad:
        buf.extend(b"\x00" * pad)
    off = len(buf)
    buf.extend(data)
    return off


def load(path):
    return bytearray(open(path, "rb").read())


def save(path, buf):
    bak = path + ".pre-iframes.bak"
    if not os.path.exists(bak):
        shutil.copy2(path, bak)
        print("backup written: %s" % bak)
    with open(path, "r+b") as h:
        h.write(buf)
    print("saved %s (%d bytes)" % (path, len(buf)))


def types_of(buf, timl_off):
    types_offset, type_count = struct.unpack_from("<QQ", buf, timl_off)
    out = []
    for ti in range(type_count):
        toff, tcount, tl_hash, _unk = struct.unpack_from("<QQIi", buf, types_offset + 24 * ti)
        out.append(dict(index=ti, toff=toff, tcount=tcount, hash=tl_hash, name=D.tname(tl_hash)))
    return types_offset, out


def transforms_of(buf, toff, tcount):
    out = []
    for xi in range(tcount):
        xoff, xcount, dt_hash, dt = struct.unpack_from("<QQIi", buf, toff + 24 * xi)
        out.append(dict(xoff=xoff, xcount=xcount, hash=dt_hash, dt=dt, name=D.dname(dt_hash)[0]))
    return out


def find_pm(buf, entry, actions):
    _, types = types_of(buf, actions[entry]["timlOffset"])
    for t in types:
        if "PlMotionCommon" in t["name"]:
            return t
    raise SystemExit("entry %d has no PlMotionCommon type" % entry)


def or_flag(buf, entry, actions, frame, target_extra, expect_others):
    """OR a flag bit into mFlag key at `frame`; idempotent. Returns True if changed."""
    pm = find_pm(buf, entry, actions)
    tr = [t for t in transforms_of(buf, pm["toff"], pm["tcount"]) if t["name"] == "mFlag"]
    assert len(tr) == 1, "entry %d: %d mFlag tracks" % (entry, len(tr))
    tr = tr[0]
    for k in range(tr["xcount"]):
        base = tr["xoff"] + KEY_SIZE * k
        frame_v = struct.unpack_from("<f", buf, base + 12)[0]
        if abs(frame_v - frame) < 0.01:
            val = struct.unpack_from("<I", buf, base)[0]
            if (val & ~B26) != expect_others:
                raise SystemExit("entry %d mFlag f%.0f unexpected value 0x%08X "
                                 "(expected others 0x%08X)" % (entry, frame, val, expect_others))
            if val & B26:
                print("entry %d mFlag f%.0f already 0x%08X, skip" % (entry, frame, val))
                return False
            new = val | target_extra
            struct.pack_into("<I", buf, base, new)
            print("entry %d mFlag f%.0f: 0x%08X -> 0x%08X" % (entry, frame, val, new))
            return True
    raise SystemExit("entry %d: no mFlag key at f%.0f" % (entry, frame))


def set_trigger_f0(buf, entry, actions):
    """Ensure mTrigger has a f0 key = 2048.

    Returns None when the track is missing entirely (caller must insert it
    structurally), otherwise True/False for changed/skip.
    """
    pm = find_pm(buf, entry, actions)
    tr = [t for t in transforms_of(buf, pm["toff"], pm["tcount"]) if t["name"] == "mTrigger"]
    if not tr:
        return None  # caller must insert structurally
    tr = tr[0]
    for k in range(tr["xcount"]):
        base = tr["xoff"] + KEY_SIZE * k
        frame_v = struct.unpack_from("<f", buf, base + 12)[0]
        if abs(frame_v - 0.0) < 0.01:
            val = struct.unpack_from("<I", buf, base)[0]
            if val == 2048:
                print("entry %d mTrigger f0 already 2048, skip" % entry)
                return False
            if val != 0:
                raise SystemExit("entry %d mTrigger f0 unexpected value %d" % (entry, val))
            struct.pack_into("<I", buf, base, 2048)
            print("entry %d mTrigger f0: 0 -> 2048" % entry)
            return True
    raise SystemExit("entry %d: mTrigger has no f0 key" % entry)


def insert_trigger_track(buf, entry, actions):
    """Structurally add a PlMotionCommon mTrigger track {f0=2048}.

    Appends a rebuilt transforms array (+1 slot) and a 1-key keys array at EOF,
    then repoints the entry's own PlMotionCommon type slot. Refuses if the
    types array, transforms array, or any of its keys arrays are shared with
    another entry (would corrupt siblings).
    """
    timl = actions[entry]["timlOffset"]
    types_offset, types = types_of(buf, timl)
    pm = find_pm(buf, entry, actions)
    old_tr = transforms_of(buf, pm["toff"], pm["tcount"])
    assert not [t for t in old_tr if t["name"] == "mTrigger"], "mTrigger already exists"

    # sharing checks
    shared_types = []
    shared_tr = []
    shared_keys = set()
    for eid, a in actions.items():
        if not a["timlOffset"]:
            continue
        tO, tC = struct.unpack_from("<QQ", buf, a["timlOffset"])
        if tO == types_offset and eid != entry:
            shared_types.append(eid)
        for ti in range(tC):
            to2, tc2, _h, _u = struct.unpack_from("<QQIi", buf, tO + 24 * ti)
            if to2 == pm["toff"] and eid != entry:
                shared_tr.append(eid)
            for xi in range(tc2):
                xo, xc, _d, _t = struct.unpack_from("<QQIi", buf, to2 + 24 * xi)
                if to2 == pm["toff"] and xc and eid != entry:
                    shared_keys.add(xo)
    assert not shared_types, "types array shared with %s" % shared_types
    assert not shared_tr, "transforms array shared with %s" % shared_tr
    used_here = {t["xoff"] for t in old_tr if t["xcount"]}
    assert not (shared_keys & used_here), "keys arrays shared"

    keys_off = append_aligned(buf, TRIG_KEY_2048_F0)

    old_pack = b"".join(struct.pack("<QQIi", t["xoff"], t["xcount"], t["hash"], t["dt"])
                        for t in old_tr)
    old_pack += struct.pack("<QQIi", keys_off, 1, TRIG_DT_HASH, 1)
    tr_off = append_aligned(buf, old_pack)

    slot = types_offset + 24 * pm["index"]
    struct.pack_into("<QQ", buf, slot, tr_off, pm["tcount"] + 1)
    print("entry %d: inserted mTrigger track (transforms 0x%X n=%d, keys 0x%X), "
          "type slot @0x%X repointed" % (entry, tr_off, pm["tcount"] + 1, keys_off, slot))
    return True


def set_flag_exact(buf, entry, actions, frame, target, allowed_old):
    """Set an mFlag key to an exact value (for window-END reverts). Idempotent."""
    pm = find_pm(buf, entry, actions)
    tr = [t for t in transforms_of(buf, pm["toff"], pm["tcount"]) if t["name"] == "mFlag"]
    assert len(tr) == 1
    tr = tr[0]
    for k in range(tr["xcount"]):
        base = tr["xoff"] + KEY_SIZE * k
        frame_v = struct.unpack_from("<f", buf, base + 12)[0]
        if abs(frame_v - frame) < 0.01:
            val = struct.unpack_from("<I", buf, base)[0]
            if val == target:
                print("entry %d mFlag f%.0f already 0x%08X, skip" % (entry, frame, val))
                return False
            if val not in allowed_old:
                raise SystemExit("entry %d mFlag f%.0f unexpected 0x%08X" % (entry, frame, val))
            struct.pack_into("<I", buf, base, target)
            print("entry %d mFlag f%.0f: 0x%08X -> 0x%08X" % (entry, frame, val, target))
            return True
    raise SystemExit("entry %d: no mFlag key at f%.0f" % (entry, frame))


def ensure_guard_105(buf, actions):
    """Thrust guard control (79->LMT105): the user-verified working recipe.
    mFlag f0 = 0x12400006 (bit28 GUARD + b25 + b22 + b1|b2, NO i-frames -
    b26 overlap would mask the guard), clear key moved f80 -> f18."""
    changed = False
    changed |= set_flag_exact(buf, 105, actions, 0, 0x12400006,
                              allowed_old=(0x02400006, 0x87400006, 0x82400006))
    pm = find_pm(buf, 105, actions)
    tr = [t for t in transforms_of(buf, pm["toff"], pm["tcount"]) if t["name"] == "mFlag"][0]
    has18 = False
    f80off = None
    for k in range(tr["xcount"]):
        base = tr["xoff"] + KEY_SIZE * k
        fv = struct.unpack_from("<f", buf, base + 12)[0]
        if abs(fv - 18.0) < 0.01:
            has18 = True
        elif abs(fv - 80.0) < 0.01:
            f80off = base
    if not has18 and f80off is not None:
        struct.pack_into("<f", buf, f80off + 12, 18.0)
        print("entry 105 mFlag clear key f80 -> f18")
        changed = True
    return changed


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else \
        r"E:\SteamLibrary\steamapps\common\Monster Hunter World\nativePC\hm\wp\wp03\mot\w03_00\w03_00.lmt"
    buf = load(path)
    _n, actions = D.parse_lmt(buf)

    changed = False
    # --- 306 特殊纳刀: 整段 bit28 格挡(终态, 局内验证 2026-10-01)---
    # b26 多键布局(无敌头+防御尾)在该动作上会毒化整段(全程无敌, 机制未挖),
    # 终态 = 三键单值 0x12400006(bit28+b25+b22+基线) + mTrigger 2048@f0.
    # 引擎链: 格挡成功 → 音效 + bit250 上升 → mod rule 强制切换 + 吞 37.
    changed |= set_flag_exact(buf, 306, actions, 0, 0x12400006,
                              allowed_old=(0x06400000, 0x02400000))
    changed |= set_flag_exact(buf, 306, actions, 10, 0x12400006,
                              allowed_old=(0x07400000, 0x03400000))
    changed |= set_flag_exact(buf, 306, actions, 15, 0x12400006,
                              allowed_old=(0x07400006, 0x03400006,
                                           0x87400006, 0x82400006))
    changed |= set_trigger_f0(buf, 306, actions)

    # --- 105 突刺 guard control(对照组)---
    changed |= ensure_guard_105(buf, actions)

    # --- 312: full-motion i-frames (no reactive switch on the draw attack) ---
    changed |= or_flag(buf, 312, actions, 0, B26, 0x01000000)
    r = set_trigger_f0(buf, 312, actions)
    if r is None:
        changed |= insert_trigger_track(buf, 312, actions)
    else:
        changed |= r

    if changed:
        save(path, buf)
    else:
        print("nothing to do, file untouched")
    return 0


if __name__ == "__main__":
    sys.exit(main())
