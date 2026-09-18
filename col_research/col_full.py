import struct, re

def u32(d,o): return struct.unpack_from('<I', d, o)[0]
def i32(d,o): return struct.unpack_from('<i', d, o)[0]
def f32(d,o): return struct.unpack_from('<f', d, o)[0]
def u16(d,o): return struct.unpack_from('<H', d, o)[0]
def s16(d,o): return struct.unpack_from('<h', d, o)[0]

def walk_names(data, start, max_entries=200):
    """walk u32-len-prefixed utf8 strings, skipping gaps"""
    entries=[]; off=start
    while len(entries) < max_entries and off+4 < len(data)-4:
        L = u32(data, off)
        if 2 <= L <= 120:
            raw = data[off+4:off+4+L]
            try:
                nm = raw.decode('utf-8')
                # looks like a move name (contains kana/kanji) - accept
                entries.append((off, L, nm))
                off = off + 4 + L
                # skip trailing junk until next plausible entry (scan up to 40B)
                for skip in range(0, 44):
                    p = off + skip
                    if p+4 > len(data): break
                    L2 = u32(data, p)
                    if 0 <= L2 <= 120:
                        try:
                            data[p+4:p+4+L2].decode('utf-8')
                            off = p; break
                        except UnicodeDecodeError: pass
                continue
            except UnicodeDecodeError: pass
        # not a string; scan forward
        off += 2
    return entries

van = open(r"E:\MHW-arts\chunk-extracted\chunk\hm\wp\wp03\collision\wp03.col",'rb').read()

print("=== header ===")
print("countA(111?)", u32(van,8), "u32@C", u32(van,0xc), "u32@10", u32(van,0x10), "u32@14(first entry len?)", u32(van,0x14))

names = walk_names(van, 0x14)
print(f"\n=== {len(names)} name entries ===")
for i,(off,L,nm) in enumerate(names):
    gap = names[i+1][0] - (off+4+L) if i+1 < len(names) else 0
    print(f"[{i:3d}] @0x{off:04x} L={L:3d} gap={gap:2d}  {nm}")
