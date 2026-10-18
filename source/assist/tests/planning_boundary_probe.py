"""Bounded native instruction probes, not full gameplay or netplay tests.

1. Given an already resolved action (no MouseOverObject emulation), run actual
   FootClass::ClickedAction and record arguments at ClickedMission entry.
2. Run ClickedMission up to the reference file's missing source-pack trampoline.
   This is an observed blocker, not an emulated/reconstructed success.
3. Execute only the final Sabotage/C4 gate, with positive and negative controls.
   This does not emulate movement, other branches, or completed explosions.
"""
import argparse, hashlib, importlib.util, json, struct
from pathlib import Path
from unicorn import UC_HOOK_CODE, UC_HOOK_MEM_WRITE
from unicorn.x86_const import *

p=argparse.ArgumentParser()
p.add_argument('game');p.add_argument('dll');p.add_argument('--output',required=True)
p.add_argument('--fixture',default=str(Path(__file__).with_name('entry_runtime_regression.py')))
args=p.parse_args()

def fixture():
 spec=importlib.util.spec_from_file_location('f',args.fixture)
 m=importlib.util.module_from_spec(spec);m.fixture_paths=[args.game,args.dll]
 spec.loader.exec_module(m)
 return m

def callback(m,at,fn):
 return m.u.hook_add(UC_HOOK_CODE,lambda uc,a,s,_:fn(),begin=at,end=at)

def argv(m,n):
 return [m.r(m.u.reg_read(UC_X86_REG_ESP)+4+4*i) for i in range(n)]

def call_until(m,address,args=(),self=0):
 sp=m.STACK+0x1e000
 m.u.mem_write(sp,struct.pack('<'+'I'*(len(args)+1),m.STOP,*args))
 m.u.reg_write(UC_X86_REG_ESP,sp);m.u.reg_write(UC_X86_REG_ECX,self)
 m.u.emu_start(address,m.STOP,count=100000)
 return m.u.reg_read(UC_X86_REG_EIP)

rows=[]
for action,mission in [(16,17),(9,8),(3,7)]:
 m=fixture();u=m.u;captured=[]
 # Simulate local control; no action, capability or mission decisions mocked.
 callback(m,0x50b6f0,lambda:m.ret(int(u.reg_read(UC_X86_REG_ECX)==m.house),0))
 def boundary():
  captured.append(argv(m,4));m.ret(0,4)
 callback(m,0x6ffbe0,boundary)
 m.run(0x4d74e0,(action,m.building,1),m.eng)
 assert captured==[[mission,0,m.building,0]],captured
 rows.append({'action':action,'mission':mission,'attack_target':None,
              'destination_id':12,'follow_target':None,'status':'PASS'})

m=fixture();u=m.u
# No keyboard modifiers held; planning mode explicitly enabled in fixture.
callback(m,0x54f5c0,lambda:m.ret(0,1))
m.b(0xac4cf4,1)
stopped=[]
def missing():
 stopped.append({'address':'0xb7e614','bytes':bytes(u.mem_read(0xb7e614,16)).hex(),
                 'source_argument':m.r(u.reg_read(UC_X86_REG_ESP)),
                 'mission_argument':m.r(u.reg_read(UC_X86_REG_ESP)+4)})
 u.emu_stop()
callback(m,0xb7e614,missing)
ip=call_until(m,0x6ffbe0,(17,0,m.building,0),m.eng)
assert ip==0xb7e614 and stopped[0]['bytes']=='00'*16,stopped
assert stopped[0]['source_argument']==m.eng and stopped[0]['mission_argument']==17

gates=[]
for capability in [0,1]:
 for planning in ['absent','uncommitted','committed']:
  m=fixture();u=m.u;follower=m.eng
  m.b(m.engT+0xec2,capability);m.b(m.engT+0xec3,0)
  m.w(follower+0xac,17)
  if planning!='absent':
   token=m.alloc(0x9c);m.w(token,follower)
   m.w(token+0x8c,0xffffffff if planning=='uncommitted' else 1)
   m.w(follower+0x514,token)
  hits=[];writes=[]
  for address in [0x51a4e6,0x51a7f4]:
   def hit(address=address):hits.append(address);u.emu_stop()
   callback(m,address,hit)
  def protect(uc,access,at,size,value,_):
   if 0x401000<=at<0x7e0000 or m.engT<=at<m.engT+0x2000:writes.append(hex(at))
  u.hook_add(UC_HOOK_MEM_WRITE,protect)
  u.reg_write(UC_X86_REG_ESI,follower)
  ip=call_until(m,0x51a4bf)
  assert hits==[0x51a4e6 if capability else 0x51a7f4],hits
  assert not writes,writes
  gates.append({'C4':bool(capability),'planning_token':planning,
                'passed_gate':bool(capability),'status':'PASS','protected_writes':writes})

result={'scope':__doc__,'reference_sha256':hashlib.sha256(Path(args.game).read_bytes()).hexdigest(),
        'resolved_action_to_clicked_mission':rows,
        'full_clicked_mission':{'status':'BLOCKED_MISSING_REFERENCE_TRAMPOLINE',
          'observed':stopped,'note':'No replacement bytes or successful full UI event claimed.'},
        'final_sabotage_gate':gates}
Path(args.output).write_text(json.dumps(result,indent=2)+'\n')
print('PASS 3 resolved-action boundaries; full click path BLOCKED by zero trampoline; PASS 6 bounded C4 gate cases')
