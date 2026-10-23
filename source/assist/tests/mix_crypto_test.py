#!/usr/bin/env python3
"""Independent fixtures: Python integer pow + system OpenSSL Blowfish ECB.

No game assets or private RSA key are needed: any 80-byte source defines a key
via the public transform, and resource payloads below are synthetic strings.
"""
import argparse, base64, ctypes, ctypes.util, hashlib, json, pathlib, random, struct, subprocess, tempfile, zlib

ROOT=pathlib.Path(__file__).resolve().parents[2]
MODULUS=int.from_bytes(base64.b64decode('AihRvNoIbTn85FZRYNZRcT+i6KpU+maCsEqr3Q5q+LDB5tH7Tz2qQ38V')[2:],'big')
LIB=ctypes.CDLL(ctypes.util.find_library('crypto'))
class BFKey(ctypes.Structure):
    _fields_=[('p',ctypes.c_uint32*18),('s',ctypes.c_uint32*1024)]
LIB.BF_set_key.argtypes=[ctypes.POINTER(BFKey),ctypes.c_int,ctypes.c_void_p]
LIB.BF_ecb_encrypt.argtypes=[ctypes.c_void_p,ctypes.c_void_p,ctypes.POINTER(BFKey),ctypes.c_int]
def encrypt(key,data):
    state=BFKey();LIB.BF_set_key(ctypes.byref(state),len(key),key)
    out=[]
    for at in range(0,len(data),8):
        block=ctypes.create_string_buffer(8);LIB.BF_ecb_encrypt(data[at:at+8],block,ctypes.byref(state),1);out.append(block.raw)
    return b''.join(out)
def key(source):
    return b''.join(pow(int.from_bytes(source[at:at+40],'little'),65537,MODULUS).to_bytes(40,'little')[:39] for at in (0,40))[:56]
def hash_name(name):
    data=name.upper().encode();rem=len(data)%4
    if rem:data+=bytes([rem])+data[len(data)-rem:len(data)-rem+1]*(3-rem)
    return zlib.crc32(data)
def mix(entries,encrypted=True,bad=False):
    pairs=list(entries.items());payload=b''.join(v for _,v in pairs);index=b'';offset=0
    for name,data in pairs:
        index+=struct.pack('<III',hash_name(name),0xfffffff0 if bad else offset,len(data));offset+=len(data)
    head=struct.pack('<HI',len(pairs),len(payload))+index
    if not encrypted:return bytes(4)+head+payload
    source=bytes((i*37+len(pairs))%256 for i in range(80));head+=bytes((-len(head))%8)
    return struct.pack('<I',0x20000)+source+encrypt(key(source),head)+payload
def database(names,truncate=False):
    body=b''.join(n.encode()+b'\0' for n in names)
    if truncate:body=body[:-1]
    return b'XCC by Olaf van der Spek'+bytes.fromhex('1a04172710198000')+struct.pack('<IIIII',52+len(body),0,0,5,len(names))+body
def fixtures(root):
    rng=random.Random(13705);vectors=[]
    for i in range(64):
        source=bytes(80) if i==0 else bytes([255])*80 if i==1 else rng.randbytes(80)
        vectors.append('K '+source.hex()+' '+key(source).hex())
    # Published Blowfish known-answer vectors plus independent OpenSSL cases.
    for k,plain,cipher in [('0000000000000000','0000000000000000','4ef997456198dd78'),('ffffffffffffffff','ffffffffffffffff','51866fd5b85ecb8a')]:
        assert encrypt(bytes.fromhex(k),bytes.fromhex(plain)).hex()==cipher
        vectors.append('B '+k+' '+cipher+' '+plain)
    for i in range(64):
        k=rng.randbytes(4+i%53);plain=rng.randbytes(8*(1+i%16));vectors.append('B '+k.hex()+' '+encrypt(k,plain).hex()+' '+plain.hex())
    (root/'vectors.txt').write_text('\n'.join(vectors)+'\n')
    (root/'EXPANDMD99.MIX').write_bytes(mix({'ISOTEMMD.MIX':mix({'SLOPE01.TEM':b'expanded terrain'}),'LOCALMD.MIX':mix({'HTNK.VXL':b'expanded voxel'})}))
    (root/'RA2MD.MIX').write_bytes(mix({'ISOTEMMD.MIX':mix({'SLOPE01.TEM':b'base terrain','ONLYBASE.TEM':b'base fallback'}),'LOCALMD.MIX':mix({'HTNK.VXL':b'base voxel','UNITTEM.PAL':b'base palette'})}))
    custom={'LOCAL MIX DATABASE.DAT':database(['USERART.MIX']),'USERART.MIX':mix({'MODUNIT.SHP':b'custom named child'})}
    (root/'ECACHEMD02.MIX').write_bytes(mix(custom))
    (root/'BAD-EXTENT.MIX').write_bytes(mix({'OOB.SHP':b'x'},bad=True))
    custom['LOCAL MIX DATABASE.DAT']=database(['USERART.MIX'],True)
    (root/'BAD-DATABASE.MIX').write_bytes(mix(custom))
def main():
    parser=argparse.ArgumentParser();parser.add_argument('--sanitize',action='store_true');parser.add_argument('--report',type=pathlib.Path);args=parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='ra2-mix-') as td:
        root=pathlib.Path(td);fixtures(root);binary=root/'mix_crypto'
        cmd=['c++','-std=c++17','-O1','-g',str(ROOT/'assist/tests/mix_crypto.cpp'),'-o',str(binary)]
        if args.sanitize:cmd+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
        subprocess.run(cmd,check=True);r=subprocess.run([str(binary),str(root)],text=True,capture_output=True);print(r.stdout,end='');print(r.stderr,end='');r.check_returncode()
        if args.report:args.report.parent.mkdir(parents=True,exist_ok=True);args.report.write_text(json.dumps({'passed':True,'vectors':130,'sanitized':args.sanitize,'output':r.stdout.strip(),'fixtures':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in root.iterdir() if p!=binary}},indent=2)+'\n')
if __name__=='__main__':main()
