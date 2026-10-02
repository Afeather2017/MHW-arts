# -*- coding: utf-8 -*-
"""Repoint one TIML transform's key array at another entry's array (zero-grow alias).

  python sharekeys.py <lmt> <dstEntry> <category-substr> <transform> <srcEntry>

Patches the dst transform header (xoff/xcount) to reference the src key array.
Auto-.bak on first write. Verify with patch_timl_defense.py dump.
"""
import os
import shutil
import struct
import sys

sys.path.insert(0, r"E:\MHW-arts")
import dump_timl_frames as D


def find_headers(buf, entry, cat_sub, name):
    hits = []
    n, actions = D.parse_lmt(buf)
    act = actions.get(entry)
    if not act or not act["timlOffset"]:
        return hits
    timl = act["timlOffset"]
    typesOffset, typeCount = struct.unpack_from("<QQ", buf, timl)
    for ti in range(typeCount):
        toff, tcount, tlHash, _ = struct.unpack_from("<QQIi", buf, typesOffset + 24 * ti)
        cat = D.tname(tlHash)
        if not toff or cat_sub.lower() not in cat.lower():
            continue
        for xi in range(tcount):
            hdr = toff + 24 * xi
            xoff, xcount, dtHash, dataType = struct.unpack_from("<QQIi", buf, hdr)
            if D.dname(dtHash)[0] == name and xoff and xcount:
                hits.append((hdr, xoff, xcount))
    return hits


def dump_keys(buf, entry, cat_sub, name):
    out = []
    for hdr, xoff, xcount in find_headers(buf, entry, cat_sub, name):
        ks = [(struct.unpack_from("<I", buf, xoff + 20 * k)[0],
               struct.unpack_from("<f", buf, xoff + 20 * k + 12)[0]) for k in range(xcount)]
        out.append("entry %d hdr@0x%x keys=%s" % (entry, hdr, ks))
    return out


def main():
    path, dst, cat_sub, name, src = sys.argv[1], int(sys.argv[2]), sys.argv[3], sys.argv[4], int(sys.argv[5])
    with open(path, "rb") as f:
        buf = bytearray(f.read())

    src_hits = find_headers(buf, src, cat_sub, name)
    dst_hits = find_headers(buf, dst, cat_sub, name)
    print("src:", "; ".join(dump_keys(buf, src, cat_sub, name)) or "NOT FOUND")
    print("dst:", "; ".join(dump_keys(buf, dst, cat_sub, name)) or "NOT FOUND")
    if len(src_hits) != 1 or len(dst_hits) != 1:
        print("FAIL: need exactly one match on each side (src=%d dst=%d)" % (len(src_hits), len(dst_hits)))
        return 1
    (src_hdr, src_off, src_cnt), (dst_hdr, dst_off, dst_cnt) = src_hits[0], dst_hits[0]

    backup = path + ".bak"
    if not os.path.exists(backup):
        shutil.copy2(path, backup)
        print("backup written: %s" % backup)

    struct.pack_into("<QQ", buf, dst_hdr, src_off, src_cnt)
    with open(path, "r+b") as f:
        f.write(buf)
    print("patched: entry %d [%s] %s hdr@0x%x -> xoff=0x%x count=%d (was 0x%x/%d)"
          % (dst, cat_sub, name, dst_hdr, src_off, src_cnt, dst_off, dst_cnt))

    with open(path, "rb") as f:
        check = f.read()
    print("verify dst:", "; ".join(dump_keys(check, dst, cat_sub, name)))
    print("verify src unchanged:", "; ".join(dump_keys(check, src, cat_sub, name)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
