import struct, sys, re

MAGIC_HDR = b'\x01\x10\x09\x18'

def find_blobs(data):
    """find all embedded MAGIC+4char blobs"""
    blobs = []
    for m in re.finditer(re.escape(MAGIC_HDR), data):
        tag = data[m.start()+4:m.start()+8]
        if tag in (b'COL\x00', b'CLND', b'CLGM', b'CLLN'):
            blobs.append((m.start(), tag))
    return blobs

def u32(d,o): return struct.unpack_from('<I', d, o)[0]
def i32(d,o): return struct.unpack_from('<i', d, o)[0]
def f32(d,o): return struct.unpack_from('<f', d, o)[0]
def u16(d,o): return struct.unpack_from('<H', d, o)[0]

def parse_names(data, name_section_off, count):
    """walk name table: each entry = ? + name"""
    off = name_section_off
    names = []
    for i in range(count):
        # entry: u32 A, u32 B, then length-prefixed utf8 name?
        a = u32(data, off); b = u32(data, off+4)
        slen = u32(data, off+8)
        if slen < 200:
            raw = data[off+12:off+12+slen]
            try:
                nm = raw.decode('utf-8')
            except UnicodeDecodeError:
                nm = repr(raw)
        else:
            nm = f"<bad len {slen}>"
        names.append((off, a, b, nm))
        # advance: 12 + slen, round up to 4, then skip trailing pad to next entry
        nxt = off + 12 + slen
        nxt = (nxt + 3) & ~3
        # heuristic: entries seem to have extra padding; find next plausible entry by scanning
        # look for next name start: try nxt, nxt+4 ... check pattern: u32 len small then utf8
        probe = nxt
        found = False
        for pad in range(0, 48, 2):
            p = nxt + pad
            if p+12 > len(data): break
            l = u32(data, p)
            if 2 <= l <= 120:
                try:
                    data[p+4:p+4+l].decode('utf-8')
                    # also require previous bytes zero-ish
                    found = True; probe = p; break
                except UnicodeDecodeError:
                    pass
        off = probe if found else nxt
    return names

def analyze(path):
    data = open(path,'rb').read()
    print(f"\n=== {path} ({len(data)} bytes) ===")
    blobs = find_blobs(data)
    for off, tag in blobs:
        a, b = u32(data, off+8), u32(data, off+12)
        print(f"  blob {tag.decode()} @0x{off:04x}: u32s=({a},{b})")
    return data, blobs

if __name__ == '__main__':
    for p in [r"E:\MHW-arts\chunk-extracted\chunk\hm\wp\wp03\collision\wp03.col",
              r"E:\MHW-arts\MHW-valor\nativePC\hm\wp\wp03\collision\wp03.col",
              r"E:\MHW-arts\MHW-MHRS-migrate\spirit-reckoning\nativePC\hm\wp\wp03\collision\wp03.col"]:
        analyze(p)
