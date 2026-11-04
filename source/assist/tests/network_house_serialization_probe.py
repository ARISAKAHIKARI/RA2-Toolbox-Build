"""Native 649CA0 sender and compressed 64BDD0 decoder on synthetic buffers.
Transport callbacks are replaced by capture-only stubs. No packets are sent,
no live peers are used, and no event receiver or House destruction is run.
"""
from pathlib import Path
SCOPE=__doc__
fixture=Path(__file__).with_name('destruct_receiver_probe.py')
exec(compile(fixture.read_text().split('for i,h in enumerate(houses):',1)[0],str(fixture),'exec'))
clock=stop+16;send_stub=stop+32;flush_stub=stop+48
u.mem_write(clock,b'\xb8'+struct.pack('<I',5678)+b'\xc3')
u.mem_write(send_stub,b'\x31\xc0\xc2\x10\x00');u.mem_write(flush_stub,b'\x31\xc0\xc3')
w(0x7e1530,clock)
iface=0x20042000;vtable=0x20043000;buf=0x20044000
w(iface,vtable);w(vtable+4,flush_stub);w(vtable+8,send_stub)
sent=[]
def capture(uc,at,size,_):
 if at==send_stub:
  sp=uc.reg_read(UC_X86_REG_ESP);sent.append(bytes(uc.mem_read(r(sp+4),r(sp+8))))
u.hook_add(UC_HOOK_CODE,capture)
rows=[]
for protocol in (0,1,2):
 for local in (0,1):
  for target in (0,1):
   for i,h in enumerate(houses):u.mem_write(h,bytes(0x20000));w(h+0x30,i)
   w(0xa83d4c,houses[local]);w(0xa8ed84,1234);w(0xa8b24c,protocol);w(0xa8b554,3)
   w(0xa8b54c,2);w(0xa8b238,3);w(0xac51fc,0x12345678);b8(0xa8ed74,0)
   w(0xa802c8,1);w(0xa802cc,0);w(0xa802d0,1)
   w(0x8b41f8,0);w(0x8b41fc,0);w(0x8b4200,0)
   u.mem_write(0xa802d4,bytes(111));run(0x4c66c0,0xa802d4,target,8)
   assert bytes(u.mem_read(0xa802d4,3))==bytes([8,0,target])
   u.mem_write(buf,bytes(4096));sent.clear();u.reg_write(UC_X86_REG_EDX,buf)
   run(0x649ca0,iface,4096,5,1)
   assert len(sent)==1 and r(0xa802c8)==0 and r(0x8b41f8)==1
   assert bytes(u.mem_read(0xa802d4,3))==bytes([8,0,local])
   assert bytes(u.mem_read(0x8b4204,3))==bytes([8,0,local])
   packet=sent[0];assert packet[0]==0x1c and packet[2]==local
   result={'protocol':protocol,'current_house':local,'outlist_house_before':target,'outlist_house_after':u.mem_read(0xa802d6,1)[0],'local_dolist_house':u.mem_read(0x8b4206,1)[0],'packet_length':len(packet),'status':'PASS'}
   if protocol==0:
    assert len(packet)==222 and packet[111:114]==bytes([8,0,local])
    result['wire_event_house']=packet[113]
   else:
    assert len(packet)==15 and packet[14]==8
    w(0x8b41f8,0);w(0x8b41fc,0);w(0x8b4200,0)
    u.reg_write(UC_X86_REG_EDX,len(packet));run(0x64bdd0,buf)
    assert r(0x8b41f8)==2
    assert bytes(u.mem_read(0x8b4204,3))==bytes([0x1c,0,local])
    assert bytes(u.mem_read(0x8b4204+111,3))==bytes([8,0,local])
    result['decoded_destruct_house']=u.mem_read(0x8b4204+113,1)[0]
   rows.append(result)
Path(a.output).write_text(json.dumps({'scope':SCOPE,'game_sha256':hashlib.sha256(b).hexdigest(),'supported_source_exe_sha256':'7b8a068535d6af06845edf95ae829b113d00c02909330e16f197426cd7db94b6','results':rows},indent=2)+'\n')
print('PASS',len(rows),'native sender House rewrites and compressed decode checks; no network')
