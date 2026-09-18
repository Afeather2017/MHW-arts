"""col_tool.py - MHW weapon .col (COL/CLND/CLGM/ATK/OAP) dumper & value editor.

Format reverse-engineered 2026-09-19 from wp03.col + Synthlight/MHW-Editor 010 template.
Value edits only (byte lengths must not change); auto .bak like patch_timl_defense.py.

Usage:
  python col_tool.py dump <col>                  # names + banks + ATK summary
  python col_tool.py names <col>                 # names table (cgp=bank, move=ATK id, unk4=node id)
  python col_tool.py bank <col> <bank>           # one CLGM bank's records
  python col_tool.py atk <col> [move_id ...]     # ATK rows (default: all)
  python col_tool.py setval <col> atk <move> <field> <value>          # edit ATK field
  python col_tool.py setval <col> rng <bank> <rec> <value>            # edit record Attack_Range
  python col_tool.py setval <col> rec <bank> <rec> <field> <value>    # edit record field (f6/f10/.../u70)
"""
import struct, shutil, sys, os

def u8(d,o): return d[o]
def u16(d,o): return struct.unpack_from('<H', d, o)[0]
def s16(d,o): return struct.unpack_from('<h', d, o)[0]
def u32(d,o): return struct.unpack_from('<I', d, o)[0]
def i32(d,o): return struct.unpack_from('<i', d, o)[0]
def f32(d,o): return struct.unpack_from('<f', d, o)[0]

REC_FIELDS = {  # offset -> (name, type) inside one 72-byte CLGM record
    0:('u1','B'), 1:('u2','B'), 2:('rng','f'), 6:('f6','f'), 10:('f10','f'), 14:('f14','f'),
    18:('s18','h'), 20:('s20','h'), 22:('f22','f'), 26:('f26','f'), 30:('f30','f'),
    34:('f34','f'), 38:('f38','f'), 42:('f42','f'), 46:('u46','H'), 48:('u48','I'),
    52:('s52','h'), 54:('f54','f'), 58:('f58','f'), 62:('f62','f'), 66:('f66','f'), 70:('u70','H')}

ATK_FIELDS = [(0,'Index','u'),(4,'Attack_Type','u'),(8,'Motion_Value','f'),(12,'Fixed_Damage','f'),
 (16,'Part_Modifier','f'),(20,'Elem_Modifier','f'),(24,'Status_Modifier','f'),(28,'Stun','f'),(32,'Exhaust','f'),
 (36,'Mount','f'),(40,'fUnk1','f'),(44,'fUnk2','f'),(48,'bUnk3','B'),(49,'Soften','f'),(53,'fUnk4','f'),
 (57,'fUnk5','f'),(61,'fUnk6','f'),(65,'fUnk7','f'),(69,'fUnk8','f'),(73,'fUnk9','f'),(77,'fUnk10','f'),
 (81,'fUnk11','f'),(85,'Flinch','u'),(89,'uUnk12','u'),(93,"MindsEye",'B'),(94,'fUnk13','f'),(98,'fUnk14','f'),
 (102,'bUnk15','B'),(103,'fUnk16','f'),(107,'HitLagSpd_a','f'),(111,'HitLag_a','f'),(115,'fUnk17','f'),
 (119,'fUnk18','f'),(123,'fUnk19','f'),(127,'HitLagSpd_b','f'),(131,'HitLag_b','f'),(135,'fUnk20','f'),
 (139,'fUnk21','f'),(143,'uUnk22','u'),(147,'bUnk23','B'),(148,'bUnk24','B'),(149,'uUnk25','u'),
 (153,'fUnk26','f'),(157,'fUnk27','f'),(161,'fUnk28','f'),(165,'Lingering','B'),(166,'Frame_Delay','f'),
 (170,'No_Sharpness','u'),(174,'uUnk30','S'),(176,'Energy_Gen','f'),(180,'Cond_MV','f'),(184,'Cond_Elem','f'),
 (188,'uUnk31','u'),(192,'uUnk32','u'),(196,'uUnk33','u'),(200,'uUnk34','u'),(204,'uUnk35','u'),
 (208,'uUnk36','u'),(212,'uUnk37','u'),(216,'uUnk38','S')]

def parse(path):
    d = bytearray(open(path,'rb').read()); info={}
    assert d[4:8]==b'COL\x00', "not a COL file"
    nc = u32(d,0x14); names=[]; o=0x18
    for i in range(nc):
        e = d.index(b'\x00',o); nm = bytes(d[o:e]).decode('utf-8','replace'); o=e+1
        names.append(dict(idx=i,name=nm,cgp=i32(d,o),move=i32(d,o+4),unk1=i32(d,o+8),
                          unk2=u16(d,o+12),unk3=u8(d,o+14),unk4=u32(d,o+15)))
        o += 19
    info['names']=names
    p=o; assert d[p+4:p+8]==b'CLND'
    nb=u32(d,p+12); q=p+21; banks=[]
    for b in range(nb):
        assert d[q+4:q+8]==b'CLGM', f"bad CLGM at 0x{q:x}"
        cnt=u32(d,q+12); r=q+16; inners=[]
        for k in range(cnt):
            inners.append(dict(bank=b,idx=k,off=r,**{nm:({'B':u8,'H':u16,'I':u32,'h':s16,'f':f32}[t](d,r+off))
                                                     for off,(nm,t) in REC_FIELDS.items()}))
            r+=72
        r+=27
        banks.append(dict(off=q,n=cnt,inners=inners)); q=r
    while d[q+4:q+8]!=b'ATK\x00': q+=1
    atk=q; mc=u32(d,atk+12); mt=u32(d,atk+16); body=atk+20
    atks=[]
    for k in range(mc):
        o=body+k*218
        rec={'idx':k,'off':o}
        for fo,fn,ft in ATK_FIELDS:
            rec[fn]= f32(d,o+fo) if ft=='f' else (u32(d,o+fo) if ft=='u' else (u16(d,o+fo) if ft=='S' else u8(d,o+fo)))
        atks.append(rec)
    info.update(clnd=dict(off=p,banks=banks), atk=dict(off=atk,n=mc,type=mt,body=body), atks=atks)
    return d,info

def pack_val(t,v):
    return {'f':struct.pack('<f',float(v)),'B':struct.pack('<B',int(v)),'H':struct.pack('<H',int(v)),
            'I':struct.pack('<I',int(v)),'h':struct.pack('<h',int(v)),'S':struct.pack('<H',int(v)),'u':struct.pack('<I',int(v))}[t]

def show_field(v,t):
    return f"{v:.4f}" if t=='f' else str(v)

def cmd_dump(path):
    d,i = parse(path)
    print(f"{path}: {len(d)} bytes, names={len(i['names'])} banks={len(i['clnd']['banks'])} ATK entries={i['atk']['n']} (type {i['atk']['type']})")
    print("\nnames (cgp = CLGM bank, move = ATK row, unk4 = node id used by TIML CollisionTimelineObject):")
    for n in i['names']:
        print(f"  [{n['idx']:2d}] bank={n['cgp']:2d} move={n['move']:2d} node={n['unk4']:3d}  {n['name']}")
    print("\nCLGM banks (rng = Attack_Range):")
    for b in i['clnd']['banks']:
        rngs = ', '.join(f"{r['rng']:.0f}" for r in b['inners'])
        print(f"  bank{b['off']//1:2d} @0x{b['off']:04x} n={b['n']}: rng=[{rngs}]")

def cmd_names(path):
    d,i = parse(path)
    for n in i['names']:
        print(f"[{n['idx']:2d}] bank={n['cgp']:2d} move={n['move']:2d} node={n['unk4']:3d}  {n['name']}")

def cmd_bank(path,bk):
    d,i = parse(path)
    b = i['clnd']['banks'][bk]
    print(f"bank{bk} @0x{b['off']:04x}, {b['n']} records:")
    for r in b['inners']:
        s = ' '.join(f"{nm}={show_field(r[nm],t)}" for off,(nm,t) in sorted(REC_FIELDS.items()))
        print(f"  rec[{r['idx']}] @0x{r['off']:04x} {s}")

def cmd_atk(path,ids):
    d,i = parse(path)
    sel = i['atks'] if not ids else [i['atks'][k] for k in map(int,ids)]
    for r in sel:
        print(f"ATK[{r['idx']}] @0x{r['off']:04x}:")
        for fo,fn,ft in ATK_FIELDS:
            print(f"    {fn:16s} = {show_field(r[fn],ft)}")

def cmd_setval(path,rest):
    d,i = parse(path)
    if rest[0]=='atk':
        _,mv,fn,val = rest
        mv,fn,val = int(mv),fn,float(val)
        for fo,f,t in ATK_FIELDS:
            if f==fn:
                o = i['atk']['body']+mv*218+fo
                old = bytes(d[o:o+4]); new = pack_val(t,val)
                print(f"ATK[{mv}].{fn} @0x{o:04x}: {f32(old,0) if t=='f' else u32(old,0)} -> {val}")
                shutil.copy2(path,path+'.bak')
                d[o:o+4] = new
                open(path,'wb').write(d)
                return
        raise SystemExit(f"no ATK field named {fn}")
    if rest[0] in ('rng','rec'):
        _,bk,rc,fn,val = (*rest[:4], rest[4]) if rest[0]=='rec' else (*rest[:3],'rng',rest[3])
        bk,rc,val = int(bk),int(rc),float(val)
        for off,(nm,t) in REC_FIELDS.items():
            if nm==fn:
                o = i['clnd']['banks'][bk]['inners'][rc]['off']+off
                print(f"bank{bk}[{rc}].{nm} @0x{o:04x}: {f32(d,o)} -> {val}")
                shutil.copy2(path,path+'.bak')
                d[o:o+4] = pack_val(t,val)
                open(path,'wb').write(d)
                return
        raise SystemExit(f"no record field named {fn} (have: {', '.join(n for _,(n,_) in REC_FIELDS.items())})")

def main():
    a = sys.argv[1:]
    if not a: raise SystemExit(__doc__)
    cmd = a[0]; path = a[1]
    if cmd=='dump': cmd_dump(path)
    elif cmd=='names': cmd_names(path)
    elif cmd=='bank': cmd_bank(path,int(a[2]))
    elif cmd=='atk': cmd_atk(path,a[2:])
    elif cmd=='setval': cmd_setval(path,a[2:])
    else: raise SystemExit(__doc__)

if __name__=='__main__':
    main()
