"""Read-only PE32 registration-table reader; never loads a DLL into Windows."""
from pathlib import Path,PurePosixPath
import hashlib,json,struct

ORDINARY_SHA256='58f198973bd0fd3b0e1b2c42fc6c545efe1fd399680abc2e8d6e9e189751614d'
PRODUCT_CODE_SHA='b372ace50101e0099a5e9533c50470b4cabb1536'
PRODUCT_RELEASE_SHA='5508c80f7b81abad4fc51847eb7f3530acddc9af'
UPSTREAM_SHA='fbe8d7d2620584d04ba14fa3580943bfa1448d40'
UPSTREAM_TREE='0a1f3074b95213b5a36b7938e2b711950f179513'
RELEASE_RUNTIME_SHA256='eba0ce9f13376aa193942a4765b07fc28e2f63d7a577bed9450325d8e951f4f7'

# A declared zero is not an empty installed hook. Without claiming a loader
# overwrite length, conservatively cover the complete source-skipped block.
# Both callback return locations are in the fixed, blob-verified public source.
ZERO_SIZE_HOOK_SOURCE='src/Misc/Blowfish/Hooks.Blowfish.cpp'
ZERO_SIZE_HOOKS={
    0x4381DA:('BlowPipe_Key_BlowfishEngine_Allocation',0x4381F3),
    0x43830A:('BlowStraw_Key_BlowfishEngine_Allocation',0x438323),
}

def coverage_size(entry):
    size=entry.get('coverage_size',entry['size'])
    if size<=0:raise ValueError('Unresolved nonpositive interval coverage: '+str(entry))
    return size

def intersects(a,b):
    a_size=coverage_size(a);b_size=coverage_size(b)
    return a['address']<b['address']+b_size and b['address']<a['address']+a_size

def coverage_method(ordinary):
    return {
        'interval_rule':'Half-open address ranges use coverage_size when present, otherwise size; nonpositive unresolved coverage is rejected.',
        'zero_declared_size_policy':'Conservative source-skipped-block envelope from hook address to callback return location. This is not an assertion that the loader overwrites 25 bytes.',
        'zero_declared_size_hooks':[h for h in ordinary['hooks'] if h['declared_size']==0],
    }

def validate_upstream(root):
    root=Path(root).resolve();pin=Path(__file__).with_name('upstream-source-pin.json');data=json.loads(pin.read_text())
    if data.get('repository')!='CnCNet/yrpp-spawner' or data.get('commit')!=UPSTREAM_SHA or data.get('tree')!=UPSTREAM_TREE:
        raise ValueError('Unexpected public upstream source pin')
    records=data.get('files',[])
    if len(records)!=69 or len({r['path'] for r in records})!=69:raise ValueError('Expected 69 pinned public source blobs')
    for record in records:
        rel=PurePosixPath(record['path']);source=root/str(rel)
        if rel.is_absolute() or '..' in rel.parts or source.is_symlink() or not source.is_file() or root not in source.resolve().parents:
            raise ValueError('Unsafe or missing public source blob: '+str(rel))
        raw=source.read_bytes();blob=hashlib.sha1(b'blob '+str(len(raw)).encode('ascii')+b'\0'+raw).hexdigest()
        if blob!=record['git_blob_sha']:raise ValueError('Public source blob mismatch: '+str(rel))
    return {'repository':data['repository'],'commit':data['commit'],'tree':data['tree'],'verified_blobs':len(records),'source_pin_sha256':hashlib.sha256(pin.read_bytes()).hexdigest()}

class PE:
    def __init__(self,path):
        self.path=Path(path);self.data=self.path.read_bytes()
        if hashlib.sha256(self.data).hexdigest()!=ORDINARY_SHA256:
            raise ValueError('Expected the fixed official ordinary CnCNetYR v17 DLL')
        off=struct.unpack_from('<I',self.data,0x3c)[0]
        if self.data[:2]!=b'MZ' or self.data[off:off+4]!=b'PE\0\0':raise ValueError('Invalid PE')
        machine,count,_,_,_,optional_size,_=struct.unpack_from('<HHIIIHH',self.data,off+4)
        optional=off+24
        if machine!=0x14c or struct.unpack_from('<H',self.data,optional)[0]!=0x10b:raise ValueError('Expected PE32 x86')
        self.base=struct.unpack_from('<I',self.data,optional+28)[0];self.sections=[]
        for i in range(count):
            at=optional+optional_size+40*i
            name=self.data[at:at+8].split(b'\0',1)[0].decode('ascii')
            virtual_size,rva,raw_size,raw_offset=struct.unpack_from('<IIII',self.data,at+8)
            if raw_offset+raw_size>len(self.data):raise ValueError('Section exceeds input file')
            self.sections.append(dict(name=name,virtual_size=virtual_size,rva=rva,raw_size=raw_size,raw_offset=raw_offset))
    def section(self,name):return next(s for s in self.sections if s['name']==name)
    def raw(self,va,size):
        for s in self.sections:
            rel=va-self.base-s['rva']
            if 0<=rel and rel+size<=s['raw_size']:return self.data[s['raw_offset']+rel:s['raw_offset']+rel+size]
        return None
    def name(self,va):
        for s in self.sections:
            rel=va-self.base-s['rva']
            if 0<=rel<s['raw_size']:
                data=self.data[s['raw_offset']+rel:s['raw_offset']+min(s['raw_size'],rel+512)]
                if b'\0' in data:return data.split(b'\0',1)[0].decode('ascii')
        raise ValueError('No file-backed hook name')
    def table(self,name,stride,fmt):
        s=self.section(name)
        if s['virtual_size']%stride or s['virtual_size']>s['raw_size']:raise ValueError('Unexpected registration table size')
        data=self.data[s['raw_offset']:s['raw_offset']+s['virtual_size']]
        return [struct.unpack_from(fmt,data,at) for at in range(0,len(data),stride)]
    def report(self):
        hooks=[];patches=[]
        for address,size,name_va,padding in self.table('.syhks00',16,'<IIII'):
            if not address:break
            if padding:raise ValueError('Unexpected hook padding')
            name=self.name(name_va)
            entry=dict(address=address,address_hex=hex(address),size=size,declared_size=size,coverage_size=size,name=name)
            if not size:
                expected=ZERO_SIZE_HOOKS.get(address)
                if expected is None or expected[0]!=name:raise ValueError('Unresolved zero-size hook: '+str(entry))
                end=expected[1]
                entry.update(coverage_size=end-address,coverage_end_hex=hex(end),coverage_reason='Conservative complete callback-skipped source block; actual loader overwrite length is not established.',coverage_source=ZERO_SIZE_HOOK_SOURCE,coverage_source_commit=UPSTREAM_SHA,coverage_callback_return_hex=hex(end))
            coverage_size(entry)
            hooks.append(entry)
        for address,size,data_va in self.table('.patch',12,'<III'):
            if not address:break
            data=self.raw(data_va,size)
            entry=dict(address=address,address_hex=hex(address),size=size,data_va_hex=hex(data_va),file_backed_data=data is not None,data_hex=data.hex() if data is not None else None)
            coverage_size(entry)
            patches.append(entry)
        return dict(file_name=self.path.name,size=len(self.data),sha256=ORDINARY_SHA256,image_base=hex(self.base),sections=self.sections,hooks=hooks,patches=patches)
