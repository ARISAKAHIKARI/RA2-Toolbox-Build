"""Actual DLL no-input startup matrix, supplied disk EXE, synthetic idle world.
Monitors game image and existing object writes, native-code visits, queues and FP state.
Not a platform launcher/extension/anti-cheat or real netplay test.
"""
import argparse,ctypes as C,hashlib,importlib.util,json
from pathlib import Path
from unicorn import UC_HOOK_CODE,UC_HOOK_MEM_WRITE
from unicorn.x86_const import *
p=argparse.ArgumentParser();p.add_argument('game');p.add_argument('dll');p.add_argument('--output',required=True);p.add_argument('--baseline-native-air',action='store_true');a=p.parse_args();rows=[]
cases=[('no_features',0)]+[(f'flag_{n}',1<<n) for n in range(32)]+[('uploaded_config_A',0xD5FCECD7),('uploaded_config_B',0xD1FCECD7),('all_flags',0xFFFFFFFF)]
if a.baseline_native_air:cases=[c for c in cases if c[0] in ('no_features','uploaded_config_A','uploaded_config_B')]
for mode in (3,4):
 for label,flags in cases:
  spec=importlib.util.spec_from_file_location('startup_fixture',Path(__file__).with_name('entry_runtime_regression.py'));m=importlib.util.module_from_spec(spec);m.fixture_paths=[a.game,a.dll];spec.loader.exec_module(m)
  for obj in (m.unit,m.eng,m.building,m.enemyUnit):m.w(obj+0x6c,100)
  m.w(0xa8b238,mode);s=m.Shared.from_buffer_copy(m.u.mem_read(m.SH,C.sizeof(m.Shared)));s.flags=flags;s.externalDisplay=1;m.u.mem_write(m.SH,bytes(s))
  image_end=m.game.OPTIONAL_HEADER.ImageBase+m.game.OPTIONAL_HEADER.SizeOfImage;heap_end=m.cursor
  writes=[];visits=set();armed=[False]
  def watch(u,access,at,n,value,_):
   if armed[0] and (0x400000<=at<image_end or m.HEAP<=at<heap_end):writes.append((at,n,u.reg_read(UC_X86_REG_EIP)))
  def visit(u,at,n,_):
   if armed[0]:visits.add(at)
  m.u.hook_add(UC_HOOK_MEM_WRITE,watch);m.u.hook_add(UC_HOOK_CODE,visit,begin=0x400000,end=image_end-1)
  fpregs=[UC_X86_REG_FPCW,UC_X86_REG_FPSW,UC_X86_REG_MXCSR]+[UC_X86_REG_XMM0+n for n in range(8)]
  for frame in (0,1,2,60):
   m.w(0xa8ed84,frame);m.u.reg_write(UC_X86_REG_FPCW,0x27f);m.u.reg_write(UC_X86_REG_MXCSR,0x3f80)
   for n in range(8):m.u.reg_write(UC_X86_REG_XMM0+n,0x123456789ABCDEF0+n)
   before=[m.u.reg_read(r) for r in fpregs];armed[0]=True;s=m.hook();armed[0]=False
   assert before==[m.u.reg_read(r) for r in fpregs],('FP state',mode,label,frame)
   assert m.r(0xa802c8)==m.r(0x8b41f8)==0,(mode,label,'unrequested event')
   assert s.data.status==1 and s.data.lastSubmitError==0
  allowed=[(0x4a8eb0,5)] if flags&65536 else []
  if a.baseline_native_air and flags&0x80000000:allowed += [(0x416ab6,10),(0x7e2908,4)]
  bad=[w for w in writes if not any(at<=w[0] and w[0]+w[1]<=at+n for at,n in allowed)]
  assert not bad,(mode,label,'unexpected game/object write',[(hex(at),n,hex(ip)) for at,n,ip in bad[:12]])
  assert not visits,(mode,label,'unexpected startup game function',list(map(hex,sorted(visits)))[:12])
  assert s.data.hookInstalled==len(allowed),(mode,label,s.data.hookInstalled)
  row={'mode':mode,'case':label,'flags':hex(flags),'frames':[0,1,2,60],'game_write_sites':sorted(set(hex(w[0]) for w in writes)),'native_instructions_executed':len(visits),'events':0,'fp_preserved':True,'status':'PASS'};rows.append(row);print('PASS',mode,label,flush=True)
Path(a.output).write_text(json.dumps({'dll_sha256':hashlib.sha256(Path(a.dll).read_bytes()).hexdigest(),'game_sha256':hashlib.sha256(Path(a.game).read_bytes()).hexdigest(),'scope':__doc__,'baseline_native_air':a.baseline_native_air,'results':rows},indent=2)+'\n')
