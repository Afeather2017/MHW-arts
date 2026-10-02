# -*- coding: utf-8 -*-
"""Add a CollisionTimelineObject (damage-window config) to entries 339/340/343.

Data-side verdict chain: CTO mFlag = the damage window (见切 140 CTO[70,80) = its only
slash; 105 CTO[22,33) = the thrust), EC mFlag = the collision-request/sound state
machine, mTrigger 1024/1 = the slinger shot ignition. The shot family (332-349) ships
WITHOUT a CTO - vanilla blade part never had a data damage window. Requests now fire at
the user's windows (family TIML restored) but drop with no CTO to route them.

Method: clone entry 105's CTO structures verbatim (type entry + transform array + key
arrays, easing/transition bytes preserved), append to each target's TIML block at EOF,
patch the mFlag key frames to the user windows, grow typesOffset/typeCount.
"""
import os
import shutil
import struct
import sys
import time

sys.path.insert(0, r"E:\MHW-arts")
import dump_timl_frames as D

LMT = r"E:\SteamLibrary\steamapps\common\Monster Hunter World\nativePC\hm\wp\wp03\mot\w03_00\w03_00.lmt"

DONOR_ENTRY = 105          # vanilla thrust: CTO バンク[0]=0, ノードID[0]=42, mFlag 3 keys
TARGETS = {
    339: [(29, 35)],
    340: [(17, 23)],
    343: [(31, 40), (58, 71)],   # on-frame, off-frame pairs (user closed intervals +1)
}
NODE_ID = 63               # 見切り攻撃 col row (slash damage values) - tunable in wp03.col later


def align16(p):
    return p + ((-p) % 16)


def types_of(buf, timl):
    typesOffset, typeCount = struct.unpack_from("<QQ", buf, timl)
    out = []
    for ti in range(typeCount):
        toff, tcount, tlHash, unk = struct.unpack_from("<QQIi", buf, typesOffset + 24 * ti)
        out.append(dict(idx=ti, pos=typesOffset + 24 * ti, toff=toff, tcount=tcount,
                        hash=tlHash, unk=unk, name=D.tname(tlHash)))
    return typesOffset, typeCount, out


def transforms_of(buf, toff, tcount):
    out = []
    for xi in range(tcount):
        xoff, xcount, dtHash, dt = struct.unpack_from("<QQIi", buf, toff + 24 * xi)
        out.append(dict(pos=toff + 24 * xi, xoff=xoff, xcount=xcount,
                        hash=dtHash, dt=dt, name=D.dname(dtHash)[0]))
    return out


def key_bytes(buf, xoff, xcount):
    return bytes(buf[xoff:xoff + 20 * xcount])


def main():
    buf = bytearray(open(LMT, "rb").read())
    n, actions = D.parse_lmt(buf)

    # locate donor CTO structures
    d_timl = actions[DONOR_ENTRY]["timlOffset"]
    _, d_count, d_types = types_of(buf, d_timl)
    d_cto = next(t for t in d_types if t["name"].endswith("CollisionTimelineObject"))
    d_xf = transforms_of(buf, d_cto["toff"], d_cto["tcount"])
    donor = dict(
        type_entry=bytes(buf[d_cto["pos"]:d_cto["pos"] + 24]),
        xf_array=bytes(buf[d_cto["toff"]:d_cto["toff"] + 24 * d_cto["tcount"]]),
        keys={xf["name"]: key_bytes(buf, xf["xoff"], xf["xcount"]) for xf in d_xf},
        names=[xf["name"] for xf in d_xf],
    )
    print("donor CTO transforms:", donor["names"])
    assert set(donor["keys"]) >= {"バンク[0]", "ノードID[0]", "mFlag"}, donor["names"]

    for eid, windows in TARGETS.items():
        timl = actions[eid]["timlOffset"]
        typesOffset, typeCount, tl = types_of(buf, timl)
        assert not any(t["name"].endswith("CollisionTimelineObject") for t in tl), "entry %d already has CTO" % eid

        # build mFlag key array: f0=0 then on/off per window, cloning donor key records
        k0 = donor["keys"]["mFlag"]           # f0=0 v0 record
        k_on = donor["keys"]["mFlag"][20:40]  # f22=1 v1 record
        k_off = donor["keys"]["mFlag"][40:60] # f33=0 v0 record
        mf = bytearray(k0)
        for on_f, off_f in windows:
            rec = bytearray(k_on); struct.pack_into("<f", rec, 12, float(on_f)); mf += rec
            rec = bytearray(k_off); struct.pack_into("<f", rec, 12, float(off_f)); mf += rec

        node = bytearray(donor["keys"]["ノードID[0]"])
        struct.pack_into("<I", node, 0, NODE_ID)
        bank = bytearray(donor["keys"]["バンク[0]"])

        # append at EOF (aligned): types array copy +1, CTO xf array, key arrays
        new_start = align16(len(buf))
        buf.extend(b"\x00" * (new_start - len(buf)))
        p = new_start

        new_types_pos = p
        old_types = bytes(buf[typesOffset:typesOffset + 24 * typeCount])
        buf += old_types
        buf += b"\x00" * 24                      # slot for the CTO type
        p += 24 * (typeCount + 1)

        cto_xf_pos = p
        buf += donor["xf_array"]                 # 3 transforms, xoffs patched below
        p += len(donor["xf_array"])

        offs = {}
        for name, blob in (("バンク[0]", bytes(bank)), ("ノードID[0]", bytes(node)), ("mFlag", bytes(mf))):
            offs[name] = p
            buf += blob
            p += len(blob)

        # fill CTO type entry + transform xoffs
        tentry = bytearray(donor["type_entry"])
        struct.pack_into("<Q", tentry, 0, cto_xf_pos)          # toff
        struct.pack_into("<QQIi", tentry, 0, cto_xf_pos, 3, d_cto["hash"], d_cto["unk"])
        buf[new_types_pos + 24 * typeCount: new_types_pos + 24 * (typeCount + 1)] = tentry
        for i, name in enumerate(donor["names"]):
            struct.pack_into("<Q", buf, cto_xf_pos + 24 * i, offs[name])

        # retarget TIML_Data: typesOffset + typeCount (timing meta untouched)
        struct.pack_into("<QQ", buf, timl, new_types_pos, typeCount + 1)
        print("entry %d: CTO added, types 0x%X->0x%X count %d->%d, windows %s, node %d" %
              (eid, typesOffset, new_types_pos, typeCount, typeCount + 1, windows, NODE_ID))

    backup = LMT + ".bak-" + time.strftime("%Y%m%d-%H%M%S") + "-addcto"
    shutil.copy2(LMT, backup)
    with open(LMT, "r+b") as h:
        h.write(buf)
    print("backup: %s" % os.path.basename(backup))

    # verify from disk
    buf2 = open(LMT, "rb").read()
    n, actions = D.parse_lmt(buf2)
    for eid in TARGETS:
        timl = actions[eid]["timlOffset"]
        _, tc, tl = types_of(buf2, timl)
        print("== entry %d: %d types ==" % (eid, tc))
        for t in tl:
            if t["name"] != "CollisionTimelineObject":
                continue
            for xf in transforms_of(buf2, t["toff"], t["tcount"]):
                keys = [(round(struct.unpack_from("<f", buf2, xf["xoff"] + 20 * k + 12)[0]),
                         struct.unpack_from("<I", buf2, xf["xoff"] + 20 * k)[0])
                        for k in range(xf["xcount"])]
                print("   %-12s %s" % (xf["name"], keys))


if __name__ == "__main__":
    main()
