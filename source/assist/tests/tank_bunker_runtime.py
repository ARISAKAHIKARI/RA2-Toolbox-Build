"""Production x86 Runtime input -> cooperative mailbox -> native event.

Win32, world, click coordinates and native MouseOverObject legality are modeled.
Boarding is supplied as observed state, not simulated movement. No real game or
network claim. Native event constructor/queue and actual Runtime code execute.
"""
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
PROBE_SCOPE=__doc__
_base=Path(__file__).with_name('no_interception_runtime.py').read_text().split('for mode in (0,5,3,4):',1)[0]
exec(compile(_base,str(Path(__file__).with_name('no_interception_runtime.py')),'exec'))
rows=[]

def setup(mode,facility='bunker',docks=1):
 m=fixture(mode,1048576);m.b(m.t+0xd2e,1);m.u.mem_write(m.t+0x380,struct.pack('<d',3.))
 m.b(m.buildT+0x16ab,facility=='bunker');m.b(m.buildT+0x16a9,facility!='bunker');m.w(m.buildT+0x1780,docks)
 m.b(m.buildT+0xcce,facility=='yard');m.b(m.t+0xcce,facility=='yard')
 units=[m.unit,m.obj(0x7f5c70,m.t,m.house,20,selected=True)]
 buildings=[m.building,m.obj(0x7e3ebc,m.buildT,m.house,30)]
 m.vector(0xa8ec78,units+buildings);m.vector(0xa8ecb8,units)
 for building in buildings:
  m.w(building+0xe8,docks);m.w(building+0xe4,m.alloc(4*docks))
 for unit in units:
  for building in buildings:m.action_overrides[(unit,building)]=1 if facility=='bunker' else 11
 m.queryObject=buildings[0]
 def select(s,i):
  i.count=len(units)
  for n,u in enumerate(units):i.sources[n]=m.PointerSource(m.r(u+0x10),u)
 s=pointer(m,1,8,select)
 assert s.pointer.accepted==2 and s.cooperativeRequest.count==2,('capture',s.pointer.accepted,s.cooperativeRequest.count)
 assert count(m)==0
 return m,units,buildings

def submit(m,targets):
 s=state(m);r=s.cooperativeRequest;s.cooperativePlan+=1;s.cooperativeSerial=r.serial;s.cooperativeMode=1;s.cooperativeCount=len(targets)
 s.cooperativeTick=m.clock;s.cooperativeFrame=m.r(0xa8ed84);s.cooperativeMatch=s.data.match
 for n,t in enumerate(targets):s.cooperativeOrders[n]=m.CooperativeAssignment(r.sources[n].id,r.sources[n].token,m.r(t+0x10),m.buildT)
 save(m,s);return tick(m)

for mode in (0,3):
 for facility in ('bunker','depot','yard'):
  m,units,buildings=setup(mode,facility);s=submit(m,buildings)
  assert s.cooperativeAck==s.cooperativePlan and s.cooperativeAccepted==2 and count(m)==2,(facility,s.cooperativeAccepted,s.cooperativeRejected)
  for n,t in enumerate(buildings):
   event=packets(m)[111*n:111*(n+1)]
   assert event[0]==4 and event[12]==(7 if facility=='bunker' else 9)
   assert event[14:19]==bytes(5) and event[19:24]==struct.pack('<IB',m.r(t+0x10),0x34)
  if facility=='bunker':
   for u,t in zip(units,buildings):m.w(u+0x2e4,t);m.w(t+0x2e4,u)
   s=tick(m)
   for u in units:assert any(r.unit==m.r(u+0x10) and r.phase==4 for r in s.data.recentReceipts),('no completed bunker receipt',m.r(u+0x10))
  assert not m.codeWrites
  rows.append({'case':f'{facility}_dispatch_{mode}','status':'PASS'})
 for condition in ('duplicate_target','occupied_race','native_refusal','two_docks','repair_inbound','contact_not_double_counted','clicked_self_contact','contact_without_destination','contact_but_occupied','contact_but_passenger','filter_revoked'):
  facility='yard' if condition in ('two_docks','repair_inbound','contact_not_double_counted') else 'bunker'
  m,units,buildings=setup(mode,facility,2 if condition in ('two_docks','contact_not_double_counted') else 1)
  targets=buildings[:];expected=2
  if condition=='duplicate_target':targets=[buildings[0],buildings[0]];expected=1
  if condition=='occupied_race':m.w(buildings[0]+0x2e4,m.enemyUnit);expected=1
  if condition=='native_refusal':m.action_overrides[(units[0],buildings[0])]=31;expected=1
  if condition in ('clicked_self_contact','contact_without_destination','contact_but_occupied','contact_but_passenger'):
   m.w(m.r(buildings[0]+0xe4),units[0]);m.action_overrides[(units[0],buildings[0])]=31
   m.w(units[0]+0xac,2);m.w(units[0]+0x5a4,0 if condition=='contact_without_destination' else buildings[0])
   if condition=='contact_without_destination':expected=1
   if condition=='contact_but_occupied':m.w(buildings[0]+0x2e4,m.enemyUnit);expected=1
   if condition=='contact_but_passenger':m.w(buildings[0]+0x114,1);expected=1
  if condition=='filter_revoked':
   s=state(m);s.featureConfig.filters[7].custom=1;s.featureConfig.filters[7].count=0;s.featureConfigSeq+=2;save(m,s);expected=0
  if condition=='two_docks':targets=[buildings[0],buildings[0]]
  if condition in ('repair_inbound','contact_not_double_counted'):
   incoming=m.obj(0x7f5c70,m.t,m.house,40);m.w(incoming+0xac,9);m.w(incoming+0x5a4,buildings[0]);m.vector(0xa8ec78,units+buildings+[incoming]);expected=1 if condition=='repair_inbound' else 2
   if condition=='contact_not_double_counted':m.w(m.r(buildings[0]+0xe4),incoming)
  s=submit(m,targets)
  assert s.cooperativeAccepted==expected and count(m)==expected,(condition,mode,s.cooperativeAccepted,s.cooperativeRejected)
  assert s.cooperativeAck==s.cooperativePlan and not m.codeWrites
  rows.append({'case':f'{condition}_{mode}','status':'PASS'})
# Execute the relevant native fragments separately, with explicit radio/map
# boundaries. This proves mission and handshake dispatch, not full pathfinding.
m,units,buildings=setup(0);u=units[0];b=buildings[0];calls=[];hooks=[]
def hook(at,fn):hooks.append(m.u.hook_add(UC_HOOK_CODE,lambda uc,addr,size,_:fn(),begin=at,end=at))
def args(n):
 sp=m.u.reg_read(UC_X86_REG_ESP);return tuple(m.r(sp+4+i*4) for i in range(n))
def capture(label,n,result=1):
 calls.append((label,args(n)));m.ret(result,n)
def fragment(start,end,regs):
 m.u.reg_write(UC_X86_REG_ESP,m.STACK+0x1d000)
 for reg,value in regs.items():m.u.reg_write(reg,value)
 m.u.emu_start(start,end,count=10000);assert m.u.reg_read(UC_X86_REG_EIP)==end
hook(m.r(0x7f5c70+0x480),lambda:capture('destination',2))
hook(m.r(0x7f5c70+0x278),lambda:capture('radio',2))
hook(m.r(0x7f5c70+0x1e8),lambda:capture('mission',2))
fragment(0x4e0045,0x4e006d,{UC_X86_REG_EDI:u,UC_X86_REG_ESI:b})
assert calls==[('destination',(b,1)),('radio',(2,b)),('mission',(7,1))],calls
for h in hooks:m.u.hook_del(h)
hooks=[];calls=[]
hook(0x65ae30,lambda:m.ret(0,0))
hook(m.r(0x7f5c70+0x278),lambda:capture('radio',2))
hook(m.r(0x7f5c70+0x274),lambda:capture('contacts',1))
hook(0x4d94b0,lambda:capture('foot_destination',2))
m.w(m.STACK+0x1d000+0x98,1)
fragment(0x742caf,0x742d10,{UC_X86_REG_EBP:u,UC_X86_REG_EBX:b,UC_X86_REG_EDI:b})
assert calls==[('radio',(2,b)),('contacts',(14,)),('foot_destination',(b,1))],calls
for h in hooks:m.u.hook_del(h)
hooks=[]
hook(0x65ad30,lambda:m.ret(b,1))
hook(m.r(0x7e3ebc+0x2c),lambda:m.ret(6,0))
hook(m.r(0x7f5c70+0x1bc),lambda:m.ret(m.crateCell,0))
hook(0x47c520,lambda:m.ret(0,0))
hook(m.r(0x7f5c70+0x1e8),lambda:capture('mission',2))
for mission in (2,7):
 calls=[];m.w(u+0xac,mission)
 fragment(0x738c9d,0x738d21,{UC_X86_REG_ESI:u,UC_X86_REG_EBP:5})
 assert calls==([('mission',(5,0))] if mission==2 else []),(mission,calls)
rows.append({'case':'native_bunker_enter_helper_destination_radio_and_enroute_mission_guard','status':'PASS'})
print('PASS',len(rows),'Runtime cases and native bunker dispatch fragments; boarding state remains modeled')
Path(a.output).write_text(json.dumps({'scope':PROBE_SCOPE,'inputs':{str(Path(x).name):hashlib.sha256(Path(x).read_bytes()).hexdigest() for x in (a.game,a.dll)},'results':rows},indent=2)+'\n')
