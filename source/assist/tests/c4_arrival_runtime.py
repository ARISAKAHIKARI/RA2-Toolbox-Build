"""Retired capability overrides: original movement/arrival/timer branches on private EXEs.
Synthetic world/Win32, cell lookup and animation/damage sinks. Native code plants
the charge, sets its timer/credit and invokes damage at expiry. This does not run
complete locomotion, Windows graphics, destruction or peer simulation.
"""
import argparse,hashlib,importlib.util,json,struct,sys,traceback
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
p=argparse.ArgumentParser();p.add_argument('game');p.add_argument('dll');p.add_argument('--output',required=True);p.add_argument('--filter',default='');a=p.parse_args();rows=[]
spec=importlib.util.spec_from_file_location('helper',Path(__file__).with_name('no_interception_runtime.py'));n=importlib.util.module_from_spec(spec)
args=sys.argv;sys.argv=[str(spec.origin),a.game,a.dll,'--filter','__fixture__','--output',str(Path(a.output).with_suffix('.helper.json'))];spec.loader.exec_module(n);sys.argv=args
sites=(0x51a4d8,0x51c32b);original=bytes.fromhex('8a81c20e0000')
def hook(m,at,fn):return m.u.hook_add(UC_HOOK_CODE,lambda uc,a,size,_:fn(),begin=at,end=at)
def set_on(m,on):s=n.state(m);s.c4Test=on;n.save(m,s);return n.tick(m)
def run_segment(m,start,stops,regs,stack_values=None):
 sp=m.STACK+0x1c000;m.u.mem_write(sp,bytes(256));m.u.reg_write(UC_X86_REG_ESP,sp)
 for off,val in (stack_values or {}).items():m.w(sp+off,val)
 for reg,val in regs.items():m.u.reg_write(reg,val)
 reached=[];hs=[hook(m,at,lambda at=at:(reached.append(at),m.u.emu_stop())) for at in stops]
 try:m.u.emu_start(start,0,count=200000)
 finally:
  for h in hs:m.u.hook_del(h)
 assert len(reached)==1,('segment did not finish',hex(m.u.reg_read(UC_X86_REG_EIP)))
 return reached[0]
for mode in (0,3,4,5):
 for case in ('ordinary','native','native_off','off','foreign','wrong_mission','wrong_building','no_building','selling','immune','already_armed','disable','stop','conflict_arrival','conflict_movement','context_changed'):
  if a.filter and a.filter!=f'{mode}_{case}':continue
  try:
   m=n.fixture(mode);m.b(m.engT+0xec3,0);m.b(m.engT+0xec2,case in ('native','native_off'));m.w(m.building+0x21c,m.enemy)
   conflict=sites[0] if case=='conflict_arrival' else sites[1] if case=='conflict_movement' else 0
   if conflict:m.u.mem_write(conflict,b'\x90'*6)
   if case=='context_changed':m.u.mem_write(0x51c331,b'\x90')
   s=set_on(m,case not in ('off','native_off'))
   failed=bool(conflict or case=='context_changed')
   assert s.data.c4TestStatus==int(case not in ('off','native_off'))
   if case in ('disable','stop'):
    s=n.state(m)
    if case=='stop':s.stop=1
    else:s.c4Test=0
    n.save(m,s);n.tick(m)
   enabled=False  # v1.0.36 never installs either C4 capability override
   assert not m.codeWrites
   for at in sites:
    assert bytes(m.u.mem_read(at,6))==(b'\x90'*6 if at==conflict else original) if not enabled else m.u.mem_read(at,1)==b'\xe9'
   if failed:
    rows.append({'case':f'{mode}_{case}','status':'PASS'});continue
   if case=='foreign':m.w(m.eng+0x21c,m.enemy)
   mission=7 if case=='wrong_mission' else 17
   m.w(m.eng+0xac,mission);m.w(m.eng+0x5a4,m.building);m.w(m.eng+0x2b4,m.building)
   allowed=case in ('native','native_off') or enabled and case!='foreign'
   # Execute original CanEnterCell discrimination and both compiled adapters.
   reached=run_segment(m,0x51c2e1,(0x51c37d,0x51c41c),{UC_X86_REG_EBP:m.eng,UC_X86_REG_ESI:m.building})
   assert reached==(0x51c37d if mission==7 or allowed else 0x51c41c)
   rules=m.alloc(0x2000);m.w(0x8871e0,rules);m.u.mem_write(rules+0x1750,struct.pack('<d',0.01));m.w(rules+0xfa8,0x12345678)
   # The fixture substitutes graphics/animation and locomotor side effects.
   # Capability loads, target comparison, armed guard and timer writes execute.
   hook(m,0x47c520,lambda:m.ret(0 if case=='no_building' else m.building,0))
   if case=='wrong_building':m.w(m.eng+0x5a4,m.enemyUnit)
   m.w(m.building+0xac,19 if case=='selling' else 5)
   hook(m,m.r(m.r(m.building)+0x160),lambda:m.ret(int(case=='immune'),0))
   animation=[]
   def anim():animation.append(m.r(m.u.reg_read(UC_X86_REG_ESP)+4));m.ret(0,1)
   hook(m,m.r(m.r(m.building)+0x148),anim)
   if case=='already_armed':m.b(m.building+0x6df,1);m.w(m.building+0x528,42);m.w(m.building+0x530,99);m.w(m.building+0x540,m.unit)
   type_before=bytes(m.u.mem_read(m.engT,0x2000))
   reached=run_segment(m,0x51a4bf,(0x51a60b,0x51a550,0x51a667,0x51a7f4),{UC_X86_REG_ESI:m.eng,UC_X86_REG_EDI:m.building},{0x14:m.crateCell,0x48:0xffffffff})
   planted=allowed and mission==17 and case not in ('wrong_building','no_building','selling','immune','already_armed')
   if planted:
    assert reached==0x51a60b and m.u.mem_read(m.building+0x6df,1)==b'\x01'
    assert m.r(m.building+0x528)==m.r(0xa8ed84) and m.r(m.building+0x540)==m.eng
    delay=m.r(m.building+0x530);assert 0<delay<100 and len(animation)==1
    # Run the original timer branch before expiry, then through ReceiveDamage.
    damage=[]
    def hurt():
     sp=m.u.reg_read(UC_X86_REG_ESP);av=[m.r(sp+4+4*i) for i in range(7)]
     damage.append((m.r(av[0]),av[2],av[3],av[4]));m.ret(0,7)
    hook(m,m.r(m.r(m.building)+0x16c),hurt)
    start=m.r(m.building+0x528);regs={UC_X86_REG_ESI:m.building,UC_X86_REG_EBP:0xffffffff}
    m.w(0xa8ed84,start+delay-1);run_segment(m,0x4401d2,(0x440378,0x44035e),regs);assert not damage
    m.w(0xa8ed84,start+delay);run_segment(m,0x4401d2,(0x440378,0x44035e),regs)
    assert damage==[(m.r(m.building+0x6c),0x12345678,m.eng,1)],damage
   elif case=='already_armed':
    assert m.r(m.building+0x528)==42 and m.r(m.building+0x530)==99 and m.r(m.building+0x540)==m.unit and not animation
   else:assert m.u.mem_read(m.building+0x6df,1)==b'\0' and not animation
   assert bytes(m.u.mem_read(m.engT,0x2000))==type_before
   rows.append({'case':f'{mode}_{case}','status':'PASS','planted':planted})
  except Exception as e:rows.append({'case':f'{mode}_{case}','status':'FAIL','error':repr(e)});traceback.print_exc()
  print(rows[-1],flush=True)
Path(a.output).write_text(json.dumps({'scope':__doc__,'game_sha256':hashlib.sha256(Path(a.game).read_bytes()).hexdigest(),'dll_sha256':hashlib.sha256(Path(a.dll).read_bytes()).hexdigest(),'results':rows},indent=2)+'\n');raise SystemExit(any(x['status']!='PASS' for x in rows))
