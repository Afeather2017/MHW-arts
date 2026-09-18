import struct

def u32(d,o): return struct.unpack_from('<I', d, o)[0]
def f32(d,o): return struct.unpack_from('<f', d, o)[0]

van = open(r"E:\MHW-arts\chunk-extracted\chunk\hm\wp\wp03\collision\wp03.col",'rb').read()

# CLND blob at 0x0a43, header 8+8=16 bytes -> body til next blob (CLGM @0x0a58)? too small.
# Actually CLND @0x0a43, CLGM @0x0a58: only 21 bytes apart -> CLND body is tiny? 0x0a43+8=0x0a4b +8 hdr = 0x0a53.
print("CLND raw @0x0a43..0x0a58:")
clnd = van[0x0a43:0x0a58]
print(' '.join(f'{b:02x}' for b in clnd))
print("  as u16:", [struct.unpack_from('<H',clnd,i)[0] for i in range(0,len(clnd)-1,2)])

# First CLGM bank @0x0a58 header: magic(8) + u32 111 + u32 6 -> body @0x0a6c .. next CLGM @0x0c33
body = van[0x0a6c:0x0c33]
print(f"\nCLGM bank0 body: {len(body)} bytes, 6 records -> {len(body)/6:.1f} bytes/record")
# try record size 45? not aligned. Try: maybe there's a u32 count or sizes first
for i in range(0, 64, 2):
    print(f"  +{i:3d} (0x{0x0a6c+i:04x}): u16={struct.unpack_from('<H',body,i)[0]:6d}  f32if_aligned={f32(body,i) if i%4==0 else ''}")
