"""Execute original Destruct-emission basic blocks, constructor and ring append.
Controlled House, stack and native clock callback; no UI selection/dialog click,
network transfer, receiver execution or House destruction is simulated.
"""
from pathlib import Path
SCOPE=__doc__
exec(compile(Path(__file__).with_name('destruct_receiver_probe.py').read_text().split('for i,h in enumerate(houses):',1)[0],str(Path(__file__).with_name('destruct_receiver_probe.py')),'exec'))
clock=stop+16
u.mem_write(clock,b'\xb8'+struct.pack('<I',5678)+b'\xc3');w(0x7e1530,clock);w(0xa8ed84,1234)
rows=[]
for start,end in ((0x48ca34,0x48caa1),(0x48cab2,0x48cb22),(0x48cbae,0x48cc1e)):
 for local in (0,1):
  for count,tail in ((0,0),(127,127),(128,0)):
   for i,h in enumerate(houses):u.mem_write(h,bytes(0x20000));w(h+0x30,i)
   w(0xa83d4c,houses[local]);w(0xa802c8,count);w(0xa802d0,tail)
   packet=0xa802d4+tail*111;u.mem_write(packet,b'\xa5'*111);w(0xa83a54+tail*4,0x11223344)
   u.mem_write(stack,bytes(0x1000));u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(start,end,count=2000);assert u.reg_read(UC_X86_REG_EIP)==end
   if count<128:
    assert r(0xa802c8)==count+1 and r(0xa802d0)==((tail+1)&127)
    assert bytes(u.mem_read(packet,3))==bytes([8,0,local]) and r(packet+3)==1234
    assert r(0xa83a54+tail*4)==5678
   else:
    assert r(0xa802c8)==count and r(0xa802d0)==tail and bytes(u.mem_read(packet,111))==b'\xa5'*111 and r(0xa83a54+tail*4)==0x11223344
   assert all(bytes(u.mem_read(h+0x1f6,1))==b'\x00' for h in houses)
   rows.append({'start':hex(start),'end':hex(end),'house':local,'count_before':count,'tail_before':tail,'appended':count<128,'status':'PASS'})
Path(a.output).write_text(json.dumps({'scope':SCOPE,'game_sha256':hashlib.sha256(b).hexdigest(),'supported_source_exe_sha256':'7b8a068535d6af06845edf95ae829b113d00c02909330e16f197426cd7db94b6','results':rows},indent=2)+'\n');print('PASS',len(rows),'native Destruct UI emission blocks, ring wrap/full and no local state effect')
