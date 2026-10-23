"""Execute native facing interpolation and infantry frame selection on private EXEs.
Constructed objects, native facing lookup, visibility virtual stub; no real video.
"""
from pathlib import Path
from unicorn import UC_HOOK_CODE
exec(compile(Path(__file__).with_name('no_interception_runtime.py').read_text().split('for mode in (0,5,3,4):',1)[0],str(Path(__file__).with_name('no_interception_runtime.py')),'exec'))
m=fixture(0);f=m.alloc(24);out=m.alloc(4);rows=[]
for desired,start,rate,duration in [(16384,0,1024,16),(1024,64512,512,4),(64512,1024,512,4),(12500,2000,700,15)]:
 for elapsed in [0,1,4,8,16,20]:
  m.w(f,desired);m.w(f+4,start);m.w(f+8,100);m.w(f+16,duration);m.w(f+20,rate);m.w(0xa8ed84,100+elapsed)
  m.run(0x4c93d0,(out,),f)
  diff=(desired-start+32768)%65536-32768;steps=abs(diff)//rate;left=max(0,duration-elapsed)
  expected=(desired-int(diff/steps)*left)&65535 if steps and left else desired
  assert m.r(out)&65535==expected,(desired,start,elapsed,m.r(out),expected)
  rows.append({'case':'native_current_facing','desired':desired,'start':start,'elapsed':elapsed,'actual':expected,'status':'PASS'})
# Infantry::ShapeNumber native branch, after local visibility predicate.
p=m.eng;t=m.engT;sequence=m.alloc(42*36);m.w(t+0xe3c,sequence);m.w(p+0x6c4,2);m.w(sequence+72,80);m.w(sequence+76,6);m.w(sequence+80,6);m.w(p+0xf8,8);m.b(t+0xd94,0);m.w(p+0x388+20,0)
visible=m.r(m.r(p)+0x440);m.u.hook_add(UC_HOOK_CODE,lambda uc,at,size,_:m.ret(1,1),begin=visible,end=visible)
lookup=[m.r(0x7eaefc+i*4) for i in range(32)]
for direction in range(32):
 m.w(p+0x388,direction*2048);m.run(0x518d80,(),p);actual=m.u.reg_read(UC_X86_REG_EAX);expected=82+lookup[direction]*6
 assert actual==expected,(direction,actual,expected)
 rows.append({'case':'native_infantry_shape','direction':direction,'sprite_frame':actual,'status':'PASS'})
Path(a.output).write_text(json.dumps({'scope':__doc__,'game_sha256':hashlib.sha256(Path(a.game).read_bytes()).hexdigest(),'native_infantry_facing_lookup':lookup,'results':rows},indent=2)+'\n')
print('PASS native Current() interpolation/wrap and 32 infantry facing frames')
