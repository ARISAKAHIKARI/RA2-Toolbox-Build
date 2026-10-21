"""Actual VoiceMove/VoiceAttack/QueueVoice and Runtime input acceptance; local owner and noncritical RNG mocked, audio device not emulated."""
import argparse,ctypes as C,hashlib,importlib.util,json,struct,sys,traceback
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ECX
p=argparse.ArgumentParser();p.add_argument('game');p.add_argument('dll');p.add_argument('--output',required=True);a=p.parse_args();rows=[]
spec=importlib.util.spec_from_file_location('advanced',Path(__file__).with_name('advanced_range_runtime.py'));v=importlib.util.module_from_spec(spec);args=sys.argv;sys.argv=[str(spec.origin),a.game,a.dll,'--filter','__fixture__','--output',str(Path(a.output).with_suffix('.helper.json'))];
try:spec.loader.exec_module(v)
except SystemExit as ex:assert not ex.code
sys.argv=args;n=v.n
for mode in (0,3):
 for case in ('move','attack','invalid','disabled_audio','follow_native'):
  try:
   m=v.setup(mode,True,'ifv_out_4');hits=[]
   m.vector(m.t+0x468,[71]);m.vector(m.t+0x484,[72]);m.w(m.t+0x560,0xffffffff);m.w(m.t+0x564,0xffffffff);m.w(m.unit+0x4f0,0xffffffff);m.b(0x822cf2,case!='disabled_audio')
   m.u.hook_add(UC_HOOK_CODE,lambda uc,at,size,_:m.ret(1,0),begin=0x50b6f0,end=0x50b6f0)
   # Platform RNG entry may detour into a separately loaded extension. Select
   # the first voice deterministically; execute Voice* and QueueVoice unchanged.
   m.u.hook_add(UC_HOOK_CODE,lambda uc,at,size,_:m.ret(0,0),begin=0x65c780,end=0x65c780)
   m.u.hook_add(UC_HOOK_CODE,lambda uc,at,size,_:hits.append(at),begin=0x708d90,end=0x708d90)
   # Use the shipped aircraft allocation input to exercise the attack acknowledgement.
   if case=='attack':
    s=n.state(m);s.flags|=1<<21;n.save(m,s)
    m.w(m.unit,0x7e22a4);m.b(m.unit+0x134,1)
    m.u.hook_add(UC_HOOK_CODE,lambda uc,at,size,_:m.ret(4,0),begin=m.r(0x7e22a4+0x2c),end=m.r(0x7e22a4+0x2c))
    m.queryObject=m.target
   def mutate(s,i):
    if case=='invalid':i.match+=1
   if case=='attack':
    # fixture ProcessClickCoords uses clickObject
    s=n.pointer(m,1,16,mutate)
   elif case=='follow_native':s=n.pointer(m,4,1,mutate)
   else:s=n.pointer(m,2,1,mutate)
   expected=case in ('move','attack');assert bool(hits)==(case in ('move','attack','disabled_audio')),(case,s.pointer.accepted,s.pointer.result,hits)
   assert (m.r(m.unit+0x4f0)!=0xffffffff)==expected,(case,m.r(m.unit+0x4f0))
   before=len(hits)
   for _ in range(3):n.tick(m)
   assert len(hits)==before,'automatic ticks must not repeat voice'
   rows.append({'case':f'{mode}_{case}','status':'PASS'});print('PASS',mode,case,flush=True)
  except Exception as e:rows.append({'case':f'{mode}_{case}','status':'FAIL','error':repr(e)});traceback.print_exc()
Path(a.output).write_text(json.dumps({'scope':__doc__,'dll_sha256':hashlib.sha256(Path(a.dll).read_bytes()).hexdigest(),'results':rows},indent=2)+'\n');raise SystemExit(any(x['status']!='PASS' for x in rows))
