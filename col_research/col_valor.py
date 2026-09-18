import sys
sys.path.insert(0, r"E:\MHW-arts\col_research")
from col_final2 import parse, FIELDS

VAN=r"E:\MHW-arts\chunk-extracted\chunk\hm\wp\wp03\collision\wp03.col"
VAL=r"E:\MHW-arts\MHW-valor\nativePC\hm\wp\wp03\collision\wp03.col"
d1,i1=parse(VAN); d2,i2=parse(VAL)
print(f"VAN: names={len(i1['names'])} banks={len(i1['clnd']['banks'])} atk={i1['atk']['n']}")
print(f"VAL: names={len(i2['names'])} banks={len(i2['clnd']['banks'])} atk={i2['atk']['n']}")

print("\n=== name-table diffs (valor) ===")
for a,b in zip(i1['names'], i2['names']):
    if a['name']!=b['name'] or a['cgp']!=b['cgp'] or a['move']!=b['move'] or a['unk4']!=b['unk4']:
        print(f"  [{a['idx']}] VAN cgp={a['cgp']} move={a['move']} unk4={a['unk4']} '{a['name']}'")
        print(f"  [{' '*(len(str(a['idx'])))}] VAL cgp={b['cgp']} move={b['move']} unk4={b['unk4']} '{b['name']}'")
if len(i2['names'])>len(i1['names']):
    for n in i2['names'][len(i1['names']):]:
        print(f"  [{n['idx']}] VAL NEW cgp={n['cgp']} move={n['move']} unk4={n['unk4']} '{n['name']}'")

print("\n=== new bank records (valor) ===")
for b in i2['clnd']['banks'][len(i1['clnd']['banks']):]:
    print(f"  bank @0x{b['off']:04x} n={b['n']}")
    for r in b['inners']:
        print(f"    rec[{r['idx']}] u1={r['u1']} u2={r['u2']} rng={r['rng']} f6={r['f6']} f10={r['f10']} f14={r['f14']} s18={r['s18']} s20={r['s20']} f22={r['f22']} f26={r['f26']} f30={r['f30']} f34={r['f34']} f38={r['f38']} f42={r['f42']} u46={r['u46']} u48={r['u48']:08x} s52={r['s52']} f54={r['f54']} f58={r['f58']} f62={r['f62']} f66={r['f66']} u70={r['u70']}")

print("\n=== bank4 inners (vanilla, where SR changed range) ===")
for r in i1['clnd']['banks'][4]['inners']:
    print(f"    rec[{r['idx']}] rng={r['rng']} f6={r['f6']} f10={r['f10']} f14={r['f14']} s18={r['s18']} s20={r['s20']} f42={r['f42']} f54={r['f54']} u70={r['u70']}")

print("\n=== bank0 inners (vanilla) ===")
for r in i1['clnd']['banks'][0]['inners']:
    print(f"    rec[{r['idx']}] u1={r['u1']} u2={r['u2']} rng={r['rng']} f6={r['f6']} f10={r['f10']} f14={r['f14']} s18={r['s18']} s20={r['s20']} f30={r['f30']} f42={r['f42']} u46={r['u46']} f54={r['f54']} f58={r['f58']} u70={r['u70']}")

print("\n=== ATK diffs (valor) ===")
for k in range(min(i1['atk']['n'], i2['atk']['n'])):
    for fo,fn,ft in FIELDS:
        w = {'f':4,'u':4,'S':2,'B':1}[ft]
        o1 = i1['atk']['off']+20+k*218+fo; o2 = i2['atk']['off']+20+k*218+fo
        if d1[o1:o1+w]!=d2[o2:o2+w]:
            va = f32(d1,o1) if ft=='f' else (u32(d1,o1) if ft=='u' else (u16(d1,o1) if ft=='S' else u8(d1,o1)))
            vb = f32(d2,o2) if ft=='f' else (u32(d2,o2) if ft=='u' else (u16(d2,o2) if ft=='S' else u8(d2,o2)))
            print(f"  ATK[{k:2d}] {fn:14s}: {va} -> {vb}")
