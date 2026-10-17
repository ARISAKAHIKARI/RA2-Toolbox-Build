#!/usr/bin/env python3
"""Optional local static verification. Supply original two PE paths as argv."""
from pathlib import Path
import hashlib,struct,sys
if len(sys.argv)!=3:
 print('usage: python tests/check_call_sites_against_user_exes.py PATH_TO_EXE_1 PATH_TO_EXE_2')
 raise SystemExit(2)
locations=[0x48ce8a,0x608206,0x60d407,0x62314e,0x623d72,0x624271]
allcontexts=[]
for file in sys.argv[1:]:
 data=Path(file).read_bytes()
 assert data[:2]==b'MZ'
 head=struct.unpack_from('<I',data,0x3c)[0]
 assert data[head:head+4]==b'PE\0\0'
 assert struct.unpack_from('<H',data,head+4)[0]==0x14c
 contexts=[]
 for addr in locations:
  off=addr-0x400000
  contexts.append(data[off-8:off+13])
  assert data[off]==0xe8
  destination=addr+5+struct.unpack_from('<i',data,off+1)[0]
  assert destination==0x55d360,(file,hex(addr),hex(destination))
 assert data[0x55d360-0x400000:0x55d360-0x400000+12]==bytes.fromhex('a0 a0 e9 a8 00 81 ec b4 01 00 00 84')
 allcontexts.append(contexts)
 print(Path(file).name,'PASS',hashlib.sha256(data).hexdigest())
assert allcontexts[0]==allcontexts[1], 'candidate contexts differ between game executables'
print('all six 21-byte candidate contexts match both EXEs')
