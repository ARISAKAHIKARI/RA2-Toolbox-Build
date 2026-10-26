"""Shipped DLL -> native EventClass -> SP DoList/MP OutList, plus native movement-gate branches.
Synthetic world/Win32; partial CanEnterCell and Capture tick, not full pathfinding,
arrival effects, Windows audio or peer simulation. Private game EXEs are not shipped.
"""
import argparse,ctypes as C,hashlib,importlib.util,json,struct,sys,traceback
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
p=argparse.ArgumentParser();p.add_argument('game');p.add_argument('dll');p.add_argument('--output',required=True);a=p.parse_args();rows=[]
spec=importlib.util.spec_from_file_location('entry_support',Path(__file__).with_name('no_interception_runtime.py'));n=importlib.util.module_from_spec(spec)
args=sys.argv;sys.argv=[str(spec.origin),a.game,a.dll,'--filter','__fixture__','--output',str(Path(a.output).with_suffix('.helper.json'))];spec.loader.exec_module(n);sys.argv=args
for mode in (0,3,4,5):
 for variant in (0,1):
  for case in ('plain','planning','can_c4','old_patch_conflict','engineer','spy','c4','off','own_building','foreign_infantry','dead_infantry','loaded','stale','chat','not_building','covered','edge'):
   try:
    m=n.fixture(mode);site=0x51a4d8;original=bytes(m.u.mem_read(site,6));m.b(m.engT+0xec3,case=='engineer');m.b(m.engT+0xebe,case=='spy');m.b(m.engT+0xec2,case=='c4');m.b(m.buildT+0x1577,case=='can_c4')
    m.w(m.building+0x21c,m.house if case=='own_building' else m.enemy);m.queryObject=m.enemyUnit if case=='not_building' else m.building
    if case=='old_patch_conflict':m.u.mem_write(site,b'\x90'*6);original=b'\x90'*6
    if case=='foreign_infantry':m.w(m.eng+0x21c,m.enemy)
    if case=='dead_infantry':m.w(m.eng+0x6c,0);m.b(m.eng+0x74,0)
    if case=='loaded':m.w(m.eng+0x11c,m.unit)
    m.w(m.crateCell+0xe4,m.unit if case=='covered' else 0);m.w(m.unit+0x30,m.building if case=='covered' else 0)
    if case=='covered':m.queryObject=m.unit
    if case=='edge':
     m.queryCell=(1,1);m.u.mem_write(m.crateCell+0x24,struct.pack('<hh',1,1));cells=m.r(0x87f7e8+316);m.w(cells+(1*512+1)*4,m.crateCell)
    s=n.state(m);s.c4Test=case!='off';n.save(m,s);s=n.tick(m)
    assert s.data.c4TestStatus==int(case!='off') and bytes(m.u.mem_read(site,6))==original and not m.codeWrites
    if case=='chat':m.b(0xa8bc60+25,1)
    before=bytes(m.u.mem_read(m.engT,0x2000))
    if case=='planning':m.b(0x87f7e8+0x11b3,1)
    def mutate(s,i):
     i.sources[0]=m.PointerSource(11,m.eng)
     if case=='stale':i.match+=1
    s=n.pointer(m,7,8192|(262144 if variant else 0),mutate);accepted=case in ('plain','planning','can_c4','old_patch_conflict','engineer','spy','c4','own_building','loaded','covered','edge')
    assert n.count(m)==int(accepted),(case,s.pointer.result,s.pointer.accepted,n.count(m))
    if accepted:
     b=n.packets(m);mission=17 if variant else 8
     assert b[0]==4 and b[7:12]==struct.pack('<IB',11,0x34) and b[12]==mission
     assert b[14:19]==struct.pack('<IB',0,0)
     assert b[19:24]==struct.pack('<IB',1001 if case=='edge' else 30020,11) and b[29]==int(case=='planning')
     for _ in range(3):n.tick(m)
     assert n.count(m)==1
    assert bytes(m.u.mem_read(m.engT,0x2000))==before and not m.codeWrites
    rows.append({'case':f'input_{mode}_{variant}_{case}','status':'PASS'});print('PASS',mode,variant,case,flush=True)
   except Exception as e:rows.append({'case':f'input_{mode}_{variant}_{case}','status':'FAIL','error':repr(e)});traceback.print_exc()
# Execute the actual native mission discrimination inside CanEnterCell. A
# mission-17 ordinary infantry fails before the old arrival-only patch can run.
for mission,c4 in ((8,False),(7,False),(17,False),(17,True)):
 try:
  m=n.fixture(0);m.w(m.eng+0xac,mission);m.b(m.engT+0xec2,c4);reached=[]
  def stop(uc,at,size,_):reached.append(at);uc.reg_write(UC_X86_REG_EIP,m.STOP)
  for at in (0x51c37d,0x51c41c):m.u.hook_add(UC_HOOK_CODE,stop,begin=at,end=at)
  m.u.reg_write(UC_X86_REG_EBP,m.eng);m.u.reg_write(UC_X86_REG_ESI,m.building);m.u.reg_write(UC_X86_REG_ESP,m.STACK+0x1e000)
  m.u.emu_start(0x51c2e1,m.STOP,count=10000)
  assert reached==[0x51c37d if mission!=17 or c4 else 0x51c41c],reached
  rows.append({'case':f'native_cell_gate_{mission}_{c4}','status':'PASS'});print('PASS native_cell_gate',mission,c4,flush=True)
 except Exception as e:rows.append({'case':f'native_cell_gate_{mission}_{c4}','status':'FAIL','error':repr(e)});traceback.print_exc()
try:
 m=n.fixture(0);m.b(m.engT+0xec3,0);m.b(m.engT+0xec2,0);m.b(m.engT+0xebe,0);m.w(m.eng+0xac,8);m.w(m.eng+0x2b4,m.building);m.w(m.eng+0x5a4,m.building)
 session=m.alloc(0x4000);m.w(0xa8b230,session)
 m.u.hook_add(UC_HOOK_CODE,lambda uc,at,size,_:m.ret(0,2),begin=0x65c7e0,end=0x65c7e0)
 m.run(0x4d4b20,(),m.eng)
 assert m.r(m.eng+0xac)==8 and m.r(m.eng+0x2b4)==m.r(m.eng+0x5a4)==m.building
 rows.append({'case':'native_capture_tick_preserves_explicit_destination_without_capabilities','status':'PASS'});print('PASS native_capture_tick',flush=True)
except Exception as e:rows.append({'case':'native_capture_tick_preserves_explicit_destination_without_capabilities','status':'FAIL','error':repr(e)});traceback.print_exc()
Path(a.output).write_text(json.dumps({'scope':__doc__,'game_sha256':hashlib.sha256(Path(a.game).read_bytes()).hexdigest(),'dll_sha256':hashlib.sha256(Path(a.dll).read_bytes()).hexdigest(),'results':rows},indent=2)+'\n');raise SystemExit(any(x['status']!='PASS' for x in rows))

