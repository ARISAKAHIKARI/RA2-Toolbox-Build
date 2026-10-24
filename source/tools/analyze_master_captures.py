#!/usr/bin/env python3
"""Read-only analysis of the three user-supplied RA2Master captures."""
import argparse,bisect,collections,hashlib,json,struct,zipfile
from pathlib import Path
import numpy as np

STAGES=('00-clean','01-off','02-on','03-off')
BASE=0x400000
TYPES={'BuildingType':(0xA83C68,0x1798,0x7E4570),'InfantryType':(0xA8E348,0xED0,0x7EB610),'UnitType':(0xA83CE0,0xDF8,0x7F6218),'AircraftType':(0xA8B218,0xDF8,0x7E2868)}

def spans(indices):
    if not len(indices):return []
    groups=np.split(indices,np.where(np.diff(indices)!=1)[0]+1)
    return [(int(g[0]),int(g[-1])+1) for g in groups]

class Capture:
    def __init__(self,path):
        self.path=Path(path);self.z=zipfile.ZipFile(path)
        self.prefix=self.z.namelist()[0]
        self.session=self.js('session.json');self.cache={};self.regions={}
        for stage in STAGES:
            regs=self.js(stage+'-private-regions.json')
            self.regions[stage]=sorted([(int(r['base'],16),r['saved_bytes'],r['file']) for r in regs])
    def js(self,name):return json.loads(self.z.read(self.prefix+name))
    def raw(self,name):
        if name not in self.cache:self.cache[name]=self.z.read(self.prefix+name)
        return self.cache[name]
    def read(self,stage,addr,size):
        if BASE<=addr and addr+size<=BASE+self.session['image_size']:
            name=stage+'-game.mem';valid=stage+'-game.valid';off=addr-BASE
        else:
            regs=self.regions[stage];i=bisect.bisect_right(regs,(addr,1<<64,''))-1
            if i<0:raise ValueError(f'{stage}: missing {addr:#x}')
            base,n,name=regs[i];off=addr-base;valid=name+'.valid'
            if off<0 or off+size>n:raise ValueError(f'{stage}: range not captured {addr:#x}+{size:#x}')
        b=self.raw(name)[off:off+size];v=self.raw(valid)[off:off+size]
        if len(b)!=size or len(v)!=size or not all(v):raise ValueError(f'{stage}: invalid {addr:#x}')
        return b
    def u32(self,stage,addr):return struct.unpack('<I',self.read(stage,addr,4))[0]
    def objects(self,stage,va):
        ptr=self.u32(stage,va+4);count=self.u32(stage,va+16)
        if count>10000:raise ValueError(f'invalid count {count}')
        return struct.unpack('<'+'I'*count,self.read(stage,ptr,count*4)) if count else ()

def analyze(path,outdir):
    c=Capture(path);result={'archive':c.path.name,'archive_sha256':hashlib.sha256(c.path.read_bytes()).hexdigest(),'session':c.session,'observation':c.js('observation.json'),'stages':{},'code_changes':[]}
    images=[];valid=[];stable=[]
    for stage in STAGES:
        b=c.raw(stage+'-game.mem');v=c.raw(stage+'-game.valid');s=c.raw(stage+'-game.stable')
        assert len(b)==len(v)==len(s)==c.session['image_size']
        images.append(np.frombuffer(b,dtype=np.uint8));valid.append(np.frombuffer(v,dtype=np.uint8)!=0);stable.append(np.frombuffer(s,dtype=np.uint8)!=0)
        result['stages'][stage]={'image_sha256':hashlib.sha256(b).hexdigest(),'tick_ms':c.js(stage+'-capture.json')['tick_ms'],'private_regions':len(c.regions[stage]),'private_saved_bytes':sum(x[1] for x in c.regions[stage])}
    usable=np.logical_and.reduce(valid+stable)
    for sec in c.session['sections']:
        if not sec['characteristics']&0x20000000:continue
        a,n=sec['rva'],sec['size'];idx=np.flatnonzero(usable[a:a+n]&(images[1][a:a+n]!=images[2][a:a+n]))+a
        for lo,hi in spans(idx):
            values=[bytes(b[lo:hi]).hex() for b in images]
            result['code_changes'].append({'va':hex(BASE+lo),'rva':hex(lo),'length':hi-lo,'clean':values[0],'off':values[1],'on':values[2],'off_again':values[3],'restored':values[1]==values[3]})
    # Re-check every supplied reversible candidate, not only its JSON summary.
    reversible=c.js('04-reversible.json');count=0
    for r in reversible['changes']:
        a=int(r['rva'],16);n=r['length'];assert usable[a:a+n].all()
        vals=[bytes(b[a:a+n]).hex().upper() for b in images[1:]]
        assert vals==[r['before'],r['enabled'],r['disabled_again']]
        assert vals[0]==vals[2] and np.all(images[1][a:a+n]!=images[2][a:a+n]);count+=n
    assert count==reversible['changed_bytes'];result['reversible_bytes_verified']=count
    result['type_counts']={};result['type_changes']=[];result['unreadable_types']=[]
    for kind,(array,size,vt) in TYPES.items():
        try: objs=[c.objects(s,array) for s in STAGES[1:]]
        except ValueError as e:result['unreadable_types'].append({'kind':kind,'error':str(e)});continue
        result['type_counts'][kind]=[len(o) for o in objs]
        common=set(objs[0])&set(objs[1])&set(objs[2])
        for ptr in sorted(common):
            try:
                blobs=[c.read(s,ptr,size) for s in STAGES[1:]]
                ids=[b[0x24:0x24+24].split(b'\0')[0].decode('ascii','replace') for b in blobs]
                if len(set(ids))!=1 or any(struct.unpack_from('<I',b)[0]!=vt for b in blobs):raise ValueError('identity/vtable mismatch')
                idx=np.flatnonzero(np.frombuffer(blobs[0],np.uint8)!=np.frombuffer(blobs[1],np.uint8))
                for lo,hi in spans(idx):
                    context_lo=lo&~3;context_hi=(hi+3)&~3
                    result['type_changes'].append({'kind':kind,'id':ids[0],'object':hex(ptr),'offset':hex(lo),'length':hi-lo,'off':blobs[0][lo:hi].hex(),'on':blobs[1][lo:hi].hex(),'off_again':blobs[2][lo:hi].hex(),'restored':blobs[0][lo:hi]==blobs[2][lo:hi],'word_offset':hex(context_lo),'word_values':[b[context_lo:context_hi].hex() for b in blobs]})
            except ValueError as e:result['unreadable_types'].append({'kind':kind,'object':hex(ptr),'error':str(e)})
    # House objects are dynamic; changes here are candidates, not attributed writes.
    result['house_changes']=[]
    houses=[c.u32(s,0xA83D4C) for s in STAGES[1:]];result['local_house']=[hex(x) for x in houses]
    if len(set(houses))==1:
        try:
            blobs=[c.read(s,houses[0],0x5820) for s in STAGES[1:]]
            idx=np.flatnonzero(np.frombuffer(blobs[0],np.uint8)!=np.frombuffer(blobs[1],np.uint8))
            for lo,hi in spans(idx):
                result['house_changes'].append({'offset':hex(lo),'length':hi-lo,'off':blobs[0][lo:hi].hex(),'on':blobs[1][lo:hi].hex(),'off_again':blobs[2][lo:hi].hex(),'restored':blobs[0][lo:hi]==blobs[2][lo:hi]})
        except ValueError as e:result['house_error']=str(e)
    outdir.mkdir(exist_ok=True,parents=True);p=outdir/(c.session['feature_id']+'-analysis.json');p.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
    counts=collections.Counter((x['kind'],x['offset'],x['length'],x['restored']) for x in result['type_changes'])
    print(json.dumps({'file':c.path.name,'code_changes':result['code_changes'],'type_counts':result['type_counts'],'type_change_groups':[{'key':k,'count':v} for k,v in counts.most_common(70)],'unreadable_types':len(result['unreadable_types']),'house_changes':len(result['house_changes'])},ensure_ascii=False))

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('files',nargs='+',type=Path);p.add_argument('--out',type=Path,default=Path('capture-analysis/results'));a=p.parse_args()
    for path in a.files:analyze(path,a.out)
