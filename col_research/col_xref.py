import sys
sys.path.insert(0, r"E:\MHW-arts\col_research")
from col_final2 import parse, u32

d,i = parse(r"E:\MHW-arts\chunk-extracted\chunk\hm\wp\wp03\collision\wp03.col")
print("=== full names table: idx | cgp(bank?) | move(->ATK) | unk4(node?) | name ===")
for n in i['names']:
    print(f"  [{n['idx']:2d}] cgp={n['cgp']:2d} move={n['move']:2d} unk4={n['unk4']:3d}  {n['name']}")

# TIML node ids observed for LMT entries: {101:44,102:43,108:[53,59,62],109:[52,72],127:57,140:63,307:[84,101]}
obs = {101:[44],102:[43],108:[53,59,62],109:[52,72],127:[57],140:[63],307:[84,101]}
node2name = {n['unk4']:n for n in i['names']}
print("\n=== TIML node id -> name entry ===")
for lmt,ids in obs.items():
    for nid in ids:
        n = node2name.get(nid)
        print(f"  LMT {lmt}: node {nid:3d} -> {'name[%d] cgp=%d move=%d %s' % (n['idx'],n['cgp'],n['move'],n['name']) if n else '?? no name'}")

print("\n=== all CLGM records: per-bank field signatures ===")
for bi,b in enumerate(i['clnd']['banks']):
    for r in b['inners']:
        print(f"  bank{bi:2d}[{r['idx']}] u1={r['u1']} u2={r['u2']} rng={r['rng']:6.1f} f6/10/14={r['f6']:5.1f},{r['f10']:5.1f},{r['f14']:5.1f} "
              f"s18={r['s18']:3d} s20={r['s20']:3d} f22={r['f22']:6.1f} f30={r['f30']:6.1f} f42={r['f42']:6.1f} "
              f"u46={r['u46']:2d} u48={r['u48']:08x} s52={r['s52']:2d} ang=({r['f54']:.0f},{r['f58']:.0f},{r['f62']:.0f},{r['f66']:.0f}) u70={r['u70']}")
