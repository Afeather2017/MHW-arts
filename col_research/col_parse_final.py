import struct, sys

def u8(d,o): return d[o]
def u16(d,o): return struct.unpack_from('<H', d, o)[0]
def s16(d,o): return struct.unpack_from('<h', d, o)[0]
def u32(d,o): return struct.unpack_from('<I', d, o)[0]
def i32(d,o): return struct.unpack_from('<i', d, o)[0]
def f32(d,o): return struct.unpack_from('<f', d, o)[0]

def parse(path):
    d = open(path,'rb').read()
    info = {}
    assert d[4:8]==b'COL\x00'
    name_count = u32(d,0x14)
    names = []; o = 0x18
    for i in range(name_count):
        e = d.index(b'\x00', o)
        nm = d[o:e].decode('utf-8', errors='replace')
        o = e+1
        names.append(dict(idx=i, name=nm, cgp=i32(d,o), move=i32(d,o+4), unk1=i32(d,o+8),
                          unk2=u16(d,o+12), unk3=u8(d,o+14), unk4=u32(d,o+15)))
        o += 19
    info['names']=names; p=o
    assert d[p+4:p+8]==b'CLND'
    cgp_count=u32(d,p+12); cgp_type=u32(d,p+16)
    q=p+21; banks=[]
    for b in range(cgp_count):
        assert d[q+4:q+8]==b'CLGM', (hex(q), d[q+4:q+8])
        cnt=u32(d,q+12); r=q+16; inners=[]
        for k in range(cnt):
            inners.append(dict(bank=b,idx=k,off=r,
                unk1=u8(d,r),unk2=u8(d,r+1),attack_range=f32(d,r+2),
                f6=f32(d,r+6),f10=f32(d,r+10),f14=f32(d,r+14),
                s18=s16(d,r+18),s20=s16(d,r+20),
                f22=f32(d,r+22),f26=f32(d,r+26),f30=f32(d,r+30),f34=f32(d,r+34),f38=f32(d,r+38),f42=f32(d,r+42),
                u46=u16(d,r+46),u48=u32(d,r+48),s52=s16(d,r+52),
                f54=f32(d,r+54),f58=f32(d,r+58),f62=f32(d,r+62),f66=f32(d,r+66),u70=u16(d,r+70)))
            r+=72
        r+=27  # fixed tail (uint, uint, 4xB, 2xu32, short, 2xushort, ubyte)  -- type2 adds 32
        banks.append(dict(off=q,count=cnt,end=r,inners=inners))
        q=r
    info['clnd']=dict(off=p,type=cgp_type,banks=banks)
    # --- ATK ---
    atk=q
    assert d[atk+4:atk+8]==b'ATK\x00', d[atk+4:atk+8]
    move_count=u32(d,atk+12); move_type=u32(d,atk+16)
    info['atk']=dict(off=atk,count=move_count,type=move_type)
    info['atks_end_expected']=None
    # stride autodetect: entries start at atk+20; first field Index (0..count-1)
    body=atk+20
    stride=None
    for s in range(150,260):
        ok=True
        for k in range(min(move_count,4)):
            if u32(d,body+k*s)!=k: ok=False; break
        if ok:
            end=body+move_count*s
            nxt=end
            if d[nxt+4:nxt+8]==b'OAP\x00':
                stride=s; break
    info['atk_body']=body; info['atk_stride']=stride
    info['oap_off']=None
    if stride:
        e=body+move_count*stride
        if d[e+4:e+8]==b'OAP\x00': info['oap_off']=e
    info['filesize']=len(d)
    return d,info

def show_atk(d,info,idxs=None):
    body,stride=info['atk_body'],info['atk_stride']
    F=['Index','Attack_Type','Motion_Value','Fixed_Damage','Part_Modifier','Elem_Modifier','Status_Modifier',
       'Stun','Exhaust','Mount','fUnk1','fUnk2','bUnk3','Soften','fUnk4','fUnk5','fUnk6','fUnk7','fUnk8',
       'fUnk9','fUnk10','fUnk11','Flinch','uUnk12','MindsEye','fUnk13','fUnk14','bUnk15','fUnk16',
       'HitLagSpd_a','HitLag_a','fUnk17','fUnk18','fUnk19','HitLagSpd_b','HitLag_b','fUnk20','fUnk21',
       'uUnk22','bUnk23','bUnk24','uUnk25','fUnk26','fUnk27','fUnk28','Lingering','Frame_Delay',
       'No_Sharpness','uUnk30','Energy_Gen','Cond_MV','Cond_Elem','uUnk31','uUnk32','uUnk33','uUnk34','uUnk35','uUnk36','uUnk37','uUnk38']
    n_bytes={'Index':4,'Attack_Type':4}
    print(f"ATK: count={info['atk']['count']} type={info['atk']['type']} stride={stride}")
    # dump first entry fully as (offset, u32, f32) to map fields
    for idx in (idxs or [0,1]):
        o=body+idx*stride
        print(f"--- entry {idx} @0x{o:04x} ---")
        for i in range(0,stride,4):
            tag=F[i//4] if i//4 < len(F) else f'field{i//4}'
            print(f"  +{i:03d} [{tag:14s}] u32={u32(d,o+i):10u} f32={f32(d,o+i):12.4f}" if False else
                  f"  +{i:03d} [{tag:14s}] u32={u32(d,o+i):>10} f32={f32(d,o+i):>12.4f}")

if __name__=='__main__':
    d,info=parse(r"E:\MHW-arts\chunk-extracted\chunk\hm\wp\wp03\collision\wp03.col")
    ns=info['names']
    print(f"names={len(ns)} end=0x{info['clnd']['off']:04x}")
    print(f"CLND type={info['clnd']['type']} banks={len(info['clnd']['banks'])}")
    for b in info['clnd']['banks']:
        print(f"  bank@0x{b['off']:04x} n={b['count']} end=0x{b['end']:04x}")
    print(f"ATK@0x{info['atk']['off']:04x} count={info['atk']['count']} type={info['atk']['type']} body=0x{info['atk_body']:04x} stride={info['atk_stride']} oap=0x{info['oap_off']:x} file=0x{info['filesize']:x}")
    if info['atk_stride']: show_atk(d,info,[0,25])
