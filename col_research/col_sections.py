import struct, re
def u32(d,o): return struct.unpack_from('<I', d, o)[0]
def f32(d,o): return struct.unpack_from('<f', d, o)[0]

van = open(r"E:\MHW-arts\chunk-extracted\chunk\hm\wp\wp03\collision\wp03.col",'rb').read()

print("=== all magic-prefix occurrences (01 10 09 18 + 4 bytes) ===")
for m in re.finditer(b'\x01\x10\x09\x18', van):
    tag = van[m.start()+4:m.start()+8]
    a,b = u32(van,m.start()+8), u32(van,m.start()+12)
    print(f"  @0x{m.start():04x}: tag={tag}  u32s=({a},{b}, {u32(van,m.start()+16) if m.start()+20<=len(van) else ''})")

# dump one full 72-byte record from bank (111,6) @0x1b44
print("\n=== 72-byte record at 0x1bc6 (raw + annotated) ===")
rec = van[0x1bc6:0x1bc6+72]
for i in range(0,72,4):
    raw = rec[i:i+4]
    fu = f32(rec,i); iu = u32(rec,i)
    note = ""
    print(f"  +{i:02d}: {raw.hex(' ')}  f32={fu:12.3f}  u32={iu:10d}  u16s=({struct.unpack_from('<H',rec,i)[0]},{struct.unpack_from('<H',rec,i+2)[0]})")
