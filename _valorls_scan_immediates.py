# -*- coding: utf-8 -*-
"""Scan ValorLS.dll (delivered 4.2) for game-VA immediates: which game
functions/data it references, and where hook targets live."""
import struct

data = open(r'MHW-valor/nativePC/plugins/ValorLS.dll', 'rb').read()
pe = struct.unpack_from('<I', data, 0x3C)[0]
nsec = struct.unpack_from('<H', data, pe + 6)[0]
opt = struct.unpack_from('<H', data, pe + 20)[0]
secs = []
for i in range(nsec):
    off = pe + 24 + opt + i * 40
    name = data[off:off + 8].rstrip(b'\x00').decode()
    vsize, va, rsize, raw = struct.unpack_from('<IIII', data, off + 8)
    secs.append((name, va, vsize, raw))
print('sections:', [(n, hex(a), hex(s)) for n, a, s, r in secs])

targets = [0x140310030, 0x140AE9EA0, 0x142134B70]
for t in targets:
    tb8 = struct.pack('<Q', t)
    p8 = []
    i = data.find(tb8)
    while i >= 0:
        p8.append(hex(i))
        i = data.find(tb8, i + 1)
    print('0x%X  u64@%s' % (t, p8[:8]))

from collections import Counter
hits = Counter()
for i in range(len(data) - 8):
    v = struct.unpack_from('<Q', data, i)[0]
    if 0x140000000 <= v < 0x150000000:
        hits[v] += 1
print('total distinct game-VA u64s:', len(hits))
for v, c in sorted(hits.items()):
    print('0x%X x%d' % (v, c))
