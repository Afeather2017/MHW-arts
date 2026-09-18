import struct

def u8(d,o): return d[o]
def u16(d,o): return struct.unpack_from('<H', d, o)[0]
def s16(d,o): return struct.unpack_from('<h', d, o)[0]
def u32(d,o): return struct.unpack_from('<I', d, o)[0]
def i32(d,o): return struct.unpack_from('<i', d, o)[0]
def f32(d,o): return struct.unpack_from('<f', d, o)[0]

FIELDS = [(0,'Index','u'),(4,'Attack_Type','u'),(8,'Motion_Value','f'),(12,'Fixed_Damage','f'),
 (16,'Part_Modif','f'),(20,'Elem_Modif','f'),(24,'Status_Modif','f'),(28,'Stun','f'),(32,'Exhaust','f'),
 (36,'Mount','f'),(40,'fUnk1','f'),(44,'fUnk2','f'),(48,'bUnk3','B'),(49,'Soften','f'),(53,'fUnk4','f'),
 (57,'fUnk5','f'),(61,'fUnk6','f'),(65,'fUnk7','f'),(69,'fUnk8','f'),(73,'fUnk9','f'),(77,'fUnk10','f'),
 (81,'fUnk11','f'),(85,'Flinch','u'),(89,'uUnk12','u'),(93,"MindsEye",'B'),(94,'fUnk13','f'),(98,'fUnk14','f'),
 (102,'bUnk15','B'),(103,'fUnk16','f'),(107,'HitLagSpd_a','f'),(111,'HitLag_a','f'),(115,'fUnk17','f'),
 (119,'fUnk18','f'),(123,'fUnk19','f'),(127,'HitLagSpd_b','f'),(131,'HitLag_b','f'),(135,'fUnk20','f'),
 (139,'fUnk21','f'),(143,'uUnk22','u'),(147,'bUnk23','B'),(148,'bUnk24','B'),(149,'uUnk25','u'),
 (153,'fUnk26','f'),(157,'fUnk27','f'),(161,'fUnk28','f'),(165,'Lingering','B'),(166,'Frame_Delay','f'),
 (170,'No_Sharp','u'),(174,'uUnk30','S'),(176,'Energy_Gen','f'),(180,'Cond_MV','f'),(184,'Cond_Elem','f'),
 (188,'uUnk31','u'),(192,'uUnk32','u'),(196,'uUnk33','u'),(200,'uUnk34','u'),(204,'uUnk35','u'),
 (208,'uUnk36','u'),(212,'uUnk37','u'),(216,'uUnk38','S')]

def parse(path):
    d = open(path,'rb').read(); info={}
    nc = u32(d,0x14); names=[]; o=0x18
    for i in range(nc):
        e = d.index(b'\x00',o); nm = d[o:e].decode('utf-8','replace'); o=e+1
        names.append(dict(idx=i,name=nm,cgp=i32(d,o),move=i32(d,o+4),unk1=i32(d,o+8),unk4=u32(d,o+15))); o+=19
    info['names']=names
    p=o; assert d[p+4:p+8]==b'CLND'
    nb=u32(d,p+12); q=p+21; banks=[]
    for b in range(nb):
        assert d[q+4:q+8]==b'CLGM'
        cnt=u32(d,q+12); r=q+16; inners=[]
        for k in range(cnt):
            inners.append(dict(off=r,idx=k,u1=u8(d,r),u2=u8(d,r+1),rng=f32(d,r+2),
                f6=f32(d,r+6),f10=f32(d,r+10),f14=f32(d,r+14),s18=s16(d,r+18),s20=s16(d,r+20),
                f22=f32(d,r+22),f26=f32(d,r+26),f30=f32(d,r+30),f34=f32(d,r+34),f38=f32(d,r+38),f42=f32(d,r+42),
                u46=u16(d,r+46),u48=u32(d,r+48),s52=s16(d,r+52),
                f54=f32(d,r+54),f58=f32(d,r+58),f62=f32(d,r+62),f66=f32(d,r+66),u70=u16(d,r+70)))
            r+=72
        r+=27
        banks.append(dict(off=q,n=cnt,inners=inners)); q=r
    # align to ATK magic
    while d[q+4:q+8]!=b'ATK\x00': q+=1
    atk=q; mc=u32(d,atk+12); mt=u32(d,atk+16); body=atk+20; stride=218
    atks=[]
    for k in range(mc):
        o=body+k*stride
        rec={'idx':k,'off':o}
        for fo,fn,ft in FIELDS:
            rec[fn]= f32(d,o+fo) if ft=='f' else (u32(d,o+fo) if ft=='u' else (u16(d,o+fo) if ft=='S' else u8(d,o+fo)))
        atks.append(rec)
    info.update(clnd=dict(off=p,banks=banks),atk=dict(off=atk,n=mc,type=mt),atks=atks,oap=body+mc*stride)
    return d,info

def diff_fields(van_d,mod_d,body_v,body_m,stride,fields,atkn):
    print("\n=== SR field-level changes ===")
    for k in range(atkn):
        for fo,fn,ft in fields:
            w = {'f':4,'u':4,'S':2,'B':1}[ft]
            a,b = van_d[body_v+k*stride+fo:body_v+k*stride+fo+w], mod_d[body_m+k*stride+fo:body_m+k*stride+fo+w]
            if a!=b:
                va = f32(a,0) if ft=='f' else (struct.unpack('<I',a)[0] if ft=='u' else (struct.unpack('<H',a)[0] if ft=='S' else a[0]))
                vb = f32(b,0) if ft=='f' else (struct.unpack('<I',b)[0] if ft=='u' else (struct.unpack('<H',b)[0] if ft=='S' else b[0]))
                print(f"  ATK[{k:2d}] +{fo:3d} {fn:14s}: {va} -> {vb}")

if __name__=='__main__':
    VAN=r"E:\MHW-arts\chunk-extracted\chunk\hm\wp\wp03\collision\wp03.col"
    SR=r"E:\MHW-arts\MHW-MHRS-migrate\spirit-reckoning\nativePC\hm\wp\wp03\collision\wp03.col"
    d1,i1=parse(VAN); d2,i2=parse(SR)
    print(f"names={len(i1['names'])} banks={len(i1['clnd']['banks'])} atk n={i1['atk']['n']} type={i1['atk']['type']} oap=0x{i1['oap']:x} file=0x{len(d1):x}")
    # name->atk mapping guess: name.move == atk index?
    for n in i1['names'][:6]: print(f"  name[{n['idx']}] cgp={n['cgp']} move={n['move']} {n['name']}")
    print(" ...")
    for n in i1['names']:
        if n['idx'] in (22,25,26,27): print(f"  name[{n['idx']}] cgp={n['cgp']} move={n['move']} {n['name']}")
    diff_fields(d1,d2,i1['atk']['off']+20,i2['atk']['off']+20,218,FIELDS,i1['atk']['n'])
