"""Actual x86 Runtime camera patch, native normal-map clamp, rollback/conflict checks. Synthetic geometry; not renderer/netplay validation."""
from pathlib import Path
exec(compile(Path(__file__).with_name('no_interception_runtime.py').read_text().split('for mode in (0,5,3,4):',1)[0],str(Path(__file__).with_name('no_interception_runtime.py')),'exec'))
rows=[];site=0x6d8640

def geometry(m,g):
 for at,value in zip((0x87f8dc,0x87f8e4,0x87f8e8,0x87f8ec,0x87f8f0,0x886fa8,0x886fac),g):m.w(at,value)

def clamp(m,p):
 at=m.alloc();m.u.mem_write(at,struct.pack('<ii',*p));m.run(site,(at,),0x887324)
 return struct.unpack('<ii',m.u.mem_read(at,8)),m.u.reg_read(UC_X86_REG_EAX)&255

def expected(g,p):
 mw,x,y,w,h,vw,vh=g;l=vw//2+30*(2*x-mw);t=vh//2+15*(2*y+mw-5);r=l+60*w-vw;b=t+30*h+135-vh
 def axis(v,a,b):return int((a+b)/2) if b<a else min(max(v,a),b)
 q=axis(p[0],l,r),axis(p[1],t,b);return q,int(q!=p)

def record(name):rows.append({'case':name,'status':'PASS'});print('PASS',name,flush=True)
for mode in (0,3):
 m=fixture(mode);original=bytes(m.u.mem_read(site,5))
 normal=(80,2,4,70,60,1280,720);geometry(m,normal)
 for p in ((0,0),(-99999,-99999),(99999,99999),(100,1500)):assert clamp(m,p)==expected(normal,p)
 tick(m);assert bytes(m.u.mem_read(site,5))==original and not m.codeWrites
 record(f'{mode}_normal_native_equivalence')
 for label,g in (('both',(20,2,4,8,8,1920,1080)),('horizontal',(20,2,4,8,60,1920,720)),('vertical',(80,2,4,70,8,1280,1080))):
  geometry(m,g);tick(m);assert m.u.mem_read(site,1)==b'\xe9',(label,state(m).data.hookInstalled)
  for p in ((0,0),(-99999,-99999),(99999,99999),(100,1500)):
   actual=clamp(m,p);assert actual==expected(g,p),(label,p,actual,expected(g,p))
   assert clamp(m,actual[0])==(actual[0],0),'must settle without alternating clamps'
  record(f'{mode}_{label}_stable_clamp')
 geometry(m,normal);tick(m);assert bytes(m.u.mem_read(site,5))==original
 record(f'{mode}_restore_large_map')
 geometry(m,(20,2,4,8,8,1920,1080));tick(m);s=state(m);s.stop=1;save(m,s);m.hook();assert bytes(m.u.mem_read(site,5))==original
 assert all(site<=at<site+5 for at,size in m.codeWrites)
 record(f'{mode}_restore_stop')
 for offset in (0,35):
  m=fixture(mode);original=bytes(m.u.mem_read(site,0xbe));changed=bytearray(original);changed[offset]^=1;m.u.mem_write(site,bytes(changed));geometry(m,(20,2,4,8,8,1920,1080));tick(m)
  assert bytes(m.u.mem_read(site,0xbe))==bytes(changed) and not m.codeWrites
  record(f'{mode}_foreign_{offset}_untouched')
Path(a.output).write_text(json.dumps({'scope':__doc__,'dll_sha256':hashlib.sha256(Path(a.dll).read_bytes()).hexdigest(),'results':rows},indent=2)+'\n')
