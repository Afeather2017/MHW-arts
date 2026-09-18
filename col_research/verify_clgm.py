import struct
def u32(d,o): return struct.unpack_from('<I', d, o)[0]
def i32(d,o): return struct.unpack_from('<i', d, o)[0]
def f32(d,o): return struct.unpack_from('<f', d, o)[0]
def u16(d,o): return struct.unpack_from('<H', d, o)[0]
def u8(d,o): return d[o]

van = open(r"E:\MHW-arts\chunk-extracted\chunk\hm\wp\wp03\collision\wp03.col",'rb').read()

# hypothesis: CLGM records use the CCL 64-byte entry format
# entry: [0:4] skip | [4:6] StartID u16 | [6:8] EndID u16 | [8] shape | ... | [16:28] startPos | [28:32] skip | [32:44] endPos | [44:48] radius | [48:64] skip
# find record starts: scan every offset, score plausibility (bone ids < 600, shape in 0..3, floats sane)
def score(off):
    if off+64 > len(van): return -1
    s = 0
    sid, eid = u16(van,off+4), u16(van,off+6)
    shape = u8(van,off+8)
    if sid > 600 or eid > 600: return -1
    if shape > 3: return -1
    s += 1
    floats = [f32(van,off+16+i*4) for i in range(3)] + [f32(van,off+32+i*4) for i in range(3)] + [f32(van,off+44)]
    for f in floats:
        if not (-5000 < f < 5000): return -1
    s += sum(1 for f in floats if f != 0.0)
    return s

best = []
for off in range(0x0a58, 0x4fb8-64):
    sc = score(off)
    if sc >= 4: best.append((sc, off))
best.sort(reverse=True)
print("top candidate record starts (score, offset):")
seen = []
for sc, off in best:
    if any(abs(off-s) < 32 for s in seen): continue
    seen.append(off)
    sid, eid, shape = u16(van,off+4), u16(van,off+6), u8(van,off+8)
    sp = [round(f32(van,off+16+i*4),3) for i in range(3)]
    ep = [round(f32(van,off+32+i*4),3) for i in range(3)]
    r = round(f32(van,off+44),3)
    print(f"  @0x{off:04x} sc={sc} bones=({sid},{eid}) shape={shape} start={sp} end={ep} r={r}")
    if len(seen) >= 15: break
