# -*- coding: utf-8 -*-
"""Patch the CollisionTimelineObject node-id key(s) of one LMT entry in place.

Usage: python _patch_cto_node.py <lmt> <entry> <newNode> [oldNode]
Patches every CTO ノードID keyframe of <entry> whose value == oldNode (all keys
if oldNode omitted). Zero length change; prints each key before/after and
asserts at least one key patched. Structure: LMT entry header +88 = timlOffset;
TIML_Data(typesOffset@+0, typeCount@+8); types x24B {toff, tcount, tlHash,
unkn0}; transforms x24B {xoff, xcount, dtHash, dataType}; keys x20B
{value u32@+0, frame f32@+12}. CTO tlHash=956269290, ノードID[0] dtHash=2996226662.
"""
import struct
import sys

CTO_HASH = 956269290   # nTimelineParam::CollisionTimelineObject
NODE_HASH = 2996226662  # transform ノードID[0]


def main():
    path, entry, new_node = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
    old_node = int(sys.argv[4]) if len(sys.argv) > 4 else None
    buf = bytearray(open(path, "rb").read())
    size0 = len(buf)
    sig, ver, n, _ = struct.unpack_from("<4shh8s", buf, 0)
    assert sig == b"LMT\x00"
    off = struct.unpack_from("<%dQ" % n, buf, 16)[entry]
    timl = struct.unpack_from("<Q", buf, off + 88)[0]
    assert timl, "entry %d has no TIML" % entry
    typesOffset, typeCount = struct.unpack_from("<QQ", buf, timl)
    patched = 0
    for ti in range(typeCount):
        toff, tcount, tlHash, _ = struct.unpack_from("<QQIi", buf, typesOffset + 24 * ti)
        if tlHash != CTO_HASH or not toff:
            continue
        for xi in range(tcount):
            xoff, xcount, dtHash, dtyp = struct.unpack_from("<QQIi", buf, toff + 24 * xi)
            if dtHash != NODE_HASH or not xoff:
                continue
            for k in range(xcount):
                pos = xoff + 20 * k
                frame = struct.unpack_from("<f", buf, pos + 12)[0]
                val = struct.unpack_from("<I", buf, pos)[0]
                if old_node is not None and val != old_node:
                    continue
                print("  key f%g: node %d -> %d @0x%x" % (frame, val, new_node, pos))
                struct.pack_into("<I", buf, pos, new_node)
                patched += 1
    assert patched, "no matching node-id key in entry %d" % entry
    assert len(buf) == size0
    open(path, "wb").write(buf)
    print("patched %d key(s), size unchanged (%d)" % (patched, size0))


if __name__ == "__main__":
    main()
