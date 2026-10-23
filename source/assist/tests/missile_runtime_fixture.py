"""Actual DLL and original Attack/Guard packets with synthetic combat queries.
Does not simulate weapon projectiles, real Windows, or network delivery.
"""
from pathlib import Path
exec(compile(Path(__file__).with_name('release129_runtime.py').read_text().split('def unload(mode,inside):',1)[0],str(Path(__file__).with_name('release129_runtime.py')),'exec'))
def setup(mode,inside,allocation=0,scope=0,close2=True):
 m=n.fixture(mode);m.w(m.enemy+0x34,m.alloc());m.w(0xAC13C8,104);typ=m.alloc();m.b(typ+0xd68,1)
 m.targets=[m.obj(0x7e22a4,typ,m.enemy,40+i,hp=40 if i==0 else 120) for i in range(2)];m.typ=typ
 m.guns=[m.unit]+[m.obj(0x7f5c70,m.t,m.house,20+i) for i in range(5)]
 for j,q in enumerate(m.targets):m.w(q+0x9c,1000+j*300);m.w(q+0xa0,1000);m.w(q+0xa4,300)
 for p in m.guns:m.w(p+0x9c,1000);m.w(p+0xa0,1000);m.w(p+0xa4,0)
 projectile=m.alloc();wh=m.alloc();m.w(m.weapon+0xb4,1500);m.w(m.weapon+0xa4,40);m.w(m.weapon+0xa0,projectile);m.w(m.weapon+0xac,wh);m.b(projectile+0x2a4,1);m.u.mem_write(wh+0xa0,struct.pack('<11d',*([1.]*11)));info=m.alloc();m.w(info,m.weapon);m.close2=close2;m.closeFirst=True
 def select(u,at,size,_):m.ret(0,1)
 def weapon(u,at,size,_):m.ret(info,1)
 def close(u,at,size,_):q=m.r(u.reg_read(UC_X86_REG_ESP)+4);m.ret(int((q==m.targets[0] and m.closeFirst) or (q==m.targets[1] and m.close2)),2)
 for off,fn in ((0x2e4,select),(0x3f8,weapon),(0x3a8,close)):
  address=m.r(m.r(m.guns[0])+off);m.u.hook_add(UC_HOOK_CODE,fn,begin=address,end=address)
 m.vector(0xa8ec78,m.guns+m.targets);s=n.state(m);s.computeApplied=1984 if inside else 960;s.missileEnabled=1;s.missileScanHz=60;s.missileAllocation=allocation;s.missileScope=scope;n.save(m,s);return m

def run_scan(m,inside):
 tick(m,inside,'missile');return tick(m,inside,'missile') if not inside else n.state(m)
def apply(m,start=0):
 out=[];b=n.packets(m)
 for j in range(start,n.count(m)):
  packet=b[j*111:(j+1)*111];id=struct.unpack_from('<I',packet,7)[0];action=packet[12];target=struct.unpack_from('<I',packet,14)[0];p=next(p for p in m.guns if m.r(p+0x10)==id);m.w(p+0xac,action);m.w(p+0xb4,-1);m.w(p+0x2b4,next((q for q in m.targets if m.r(q+0x10)==target),0));out.append((id,action,target))
 return out
