import struct

VAN = r"E:\MHW-arts\chunk-extracted\chunk\hm\wp\wp03\collision\wp03.col"
MODS = {
 "VALOR": r"E:\MHW-arts\MHW-valor\nativePC\hm\wp\wp03\collision\wp03.col",
 "SR":    r"E:\MHW-arts\MHW-MHRS-migrate\spirit-reckoning\nativePC\hm\wp\wp03\collision\wp03.col",
}

def runs(diffs):
    """collapse byte indices into ranges"""
    out=[]
    for i in diffs:
        if out and i == out[-1][1]: out[-1][1]=i+1
        else: out.append([i,i+1])
    return out

van = open(VAN,'rb').read()
for tag, path in MODS.items():
    mod = open(path,'rb').read()
    n = min(len(van), len(mod))
    d = [i for i in range(n) if van[i]!=mod[i]]
    print(f"\n=== {tag}: van {len(van)}B vs mod {len(mod)}B, {len(d)} differing bytes ===")
    rr = runs(d)
    print(f"{len(rr)} diff regions:")
    for a,b in rr[:40]:
        va, ma = van[a:min(b,a+24)], mod[a:min(b,a+24)]
        print(f"  0x{a:04x}-0x{b:04x} ({b-a:3d}B): van={va.hex(' ')}")
        print(f"  {'':16s} mod={ma.hex(' ')}")
    if len(rr)>40: print(f"  ... +{len(rr)-40} more regions")
