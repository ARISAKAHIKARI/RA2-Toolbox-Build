"""Native receive dispatcher 64A0C0, transport-ID mapper 541040 and decoders.
Synthetic transport connection records and buffers only; no socket receive,
event execution, complete lockstep scheduler, CRC comparison or live peers.
"""
from pathlib import Path
SCOPE=__doc__
fixture=Path(__file__).with_name('destruct_receiver_probe.py')
exec(compile(fixture.read_text().split('for i,h in enumerate(houses):',1)[0],str(fixture),'exec'))
clock=stop+16;u.mem_write(clock,b'\xb8'+struct.pack('<I',5678)+b'\xc3');w(0x7e1530,clock)
iface=0x20042000;vtable=0x20043000;buf=0x20044000;stats=0x20046000
nodes=[0x20047000,0x20048000];ids=[1001,2002]
w(iface,vtable);w(vtable+0x18,0x541040);w(iface+0x44,2)
for i,node in enumerate(nodes):w(iface+0x28+4*i,node);w(node+0x64,ids[i])
def packet(protocol,house,extra=False):
 header=bytearray(111 if protocol==0 else 14);header[0]=0x1c;header[2]=house
 struct.pack_into('<I',header,3,1239);struct.pack_into('<I',header,7,0x12345678)
 struct.pack_into('<H',header,11,9);header[13]=5
 event=bytearray(111 if protocol==0 else 1);event[0]=8
 if protocol==0:event[2]=house;struct.pack_into('<I',event,3,1239)
 return bytes(header)+(bytes(header) if extra else b'')+bytes(event)
rows=[]
for protocol in (0,1,2):
 for sender in (0,1):
  for house in (0,1):
   for extra in (False,True):
    w(0xa8b24c,protocol);w(0xa8b238,3);b8(0xa8dba0,0);w(0xa8ed84,1234)
    w(0x8b41f8,0);w(0x8b41fc,0);w(0x8b4200,0)
    for i in (0,1):u.mem_write(stats+24*i,struct.pack('<6I',100,7,5,11,12,13))
    untouched=bytes(u.mem_read(stats+24*(1-sender),24))
    data=packet(protocol,house,extra);u.mem_write(buf,data)
    u.reg_write(UC_X86_REG_EDX,buf);run(0x64a0c0,iface,ids[sender],len(data),stats,0)
    assert bytes(u.mem_read(stats+24*(1-sender),24))==untouched
    assert [r(stats+24*sender+off) for off in (0,4,8)]==[1234,9,6+int(extra)]
    assert r(0x8b41f8)==2+int(extra)
    final=0x8b4204+(1+int(extra))*111
    assert bytes(u.mem_read(final,3))==bytes([8,0,house]) and r(final+3)==1239
    rows.append({'protocol':protocol,'transport_sender_index':sender,'frameinfo_house':house,'extra_frameinfo':extra,'sender_progress_frame':1234,'sender_expected':9,'sender_received':6+int(extra),'other_sender_record_unchanged':True,'decoded_destruct_house':house,'status':'PASS'})
Path(a.output).write_text(json.dumps({'scope':SCOPE,'game_sha256':hashlib.sha256(b).hexdigest(),'supported_source_exe_sha256':'7b8a068535d6af06845edf95ae829b113d00c02909330e16f197426cd7db94b6','results':rows},indent=2)+'\n')
print('PASS',len(rows),'native sender mapping/accounting and extra-FrameInfo count cases; no network')
