"""Native TMP accessors on each private sample, with constructed loaded TMP memory.
No real map assets, screen rendering, disk loader, or Windows window is exercised.
"""
from pathlib import Path
exec(compile(Path(__file__).with_name('no_interception_runtime.py').read_text().split('for mode in (0,5,3,4):',1)[0],str(Path(__file__).with_name('no_interception_runtime.py')),'exec'))
m=fixture(0);t=m.alloc(0x400);image=m.alloc(0x100);block=m.alloc(0x100);w=m.alloc(4);h=m.alloc(4)
m.w(t,0x7ecc48);m.w(t+0xa4,image)
for off,val in ((0,2),(4,2),(8,60),(12,30),(24,block)):m.w(image+off,val)
m.w(block+4,15);m.w(block+24,3);m.w(block+36,1);m.b(block+42,7)
rows=[]
m.run(0x544cb0,(),t);assert m.u.reg_read(UC_X86_REG_EAX)==image
rows.append({'case':'native_GetImage_returns_loaded_TMP_at_A4','status':'PASS'})
for index in (2,6,254):
 m.run(0x547150,(index,w,h),t);assert m.u.reg_read(UC_X86_REG_EAX)&255==1 and m.r(w)==60 and m.r(h)==42
 m.run(0x5471b0,(index,),t);assert m.u.reg_read(UC_X86_REG_EAX)==7
 rows.append({'case':f'native_subtile_{index}_modulo_directory_cliff_height_and_slope','status':'PASS'})
assert not m.codeWrites and count(m)==0
Path(a.output).write_text(json.dumps({'scope':__doc__,'dll_sha256':hashlib.sha256(Path(a.dll).read_bytes()).hexdigest(),'results':rows},indent=2)+'\n')
for row in rows:print('PASS',row['case'])
