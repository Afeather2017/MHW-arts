import struct

def u32(d,o): return struct.unpack_from('<I', d, o)[0]

van = open(r"E:\MHW-arts\chunk-extracted\chunk\hm\wp\wp03\collision\wp03.col",'rb').read()
region = van[0x14:0x0a43]  # name table region
o = 0
entries = []
while o < len(region):
    # read u32s until we hit a non-zero byte that starts a string
    metas = []
    while o < len(region) and region[o] == 0:
        # collect leading zero u32s as meta
        if o+4 <= len(region):
            metas.append(u32(region,o)); o += 4
        else:
            o += 1
    # string: bytes until \x00
    s = o
    while o < len(region) and region[o] != 0:
        o += 1
    raw = region[s:o]
    o += 1  # skip null
    try: nm = raw.decode('utf-8')
    except: nm = repr(raw)
    entries.append((metas, nm))
print(f"{len(entries)} entries in name table")
for i,(metas,nm) in enumerate(entries):
    print(f"[{i:3d}] metas={metas}  '{nm}'")
