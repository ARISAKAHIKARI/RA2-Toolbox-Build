"""Bounded release-DLL control-order comparison; no world tick or real transport."""
import json,hashlib
from pathlib import Path
fixture=Path(__file__).with_name('control_command_owner_runtime.py')
ns={'__name__':'control_off_fixture','__file__':str(fixture)}
exec(compile(fixture.read_text().split('\nfor mode in (3,4):',1)[0],str(fixture),'exec'),ns)
net,n=ns['net'],ns['n']; UC_HOOK_CODE=ns['UC_HOOK_CODE']; ECX=ns['UC_X86_REG_ECX']; ESP=ns['UC_X86_REG_ESP']
rows=[]
for protocol in (0,1,2):
 for route in ('native','tool_alt'):
  outputs=[]
  for gateway in (False,True):
   m=ns['setup'](protocol=protocol,enabled=False,network=False,gateway=gateway)
   assert n.state(m).controlCommandOwnerStatus==0 and n.state(m).networkHouseStatus==0
   net.restored(m)
   pre=net.protect(m);ctor=[]
   def record(uc,at,size,_):
    sp=uc.reg_read(ESP)
    ctor.append([m.r(sp+4+i*4) for i in range(10)])
   m.u.hook_add(UC_HOOK_CODE,record,begin=0x4c6860,end=0x4c6860)
   if route=='native':
    event=ns['native'](m);assert event
   else:
    def enemy(s,i):i.sources[0]=m.PointerSource(13,m.enemyUnit)
    s=n.pointer(m,9,0,enemy);assert s.pointer.accepted==1,(protocol,s.pointer.result)
    event=net.logical_out(m)[0][0]
   assert m.r(0x8b41f8)==0,'order path directly appended DoList'
   net.unchanged(m,pre)
   assert n.state(m).nativeMegaGatewayAccepted==0
   out=[(p.hex(),time) for p,time in net.logical_out(m)]
   sent,local=net.send(m)
   remote=[p for p in net.decode(m) if p[0]!=0x1c]
   assert [ns['meaningful'](p)+(p[3:7],) for p in local]==[ns['meaningful'](p)+(p[3:7],) for p in remote],(protocol,route,[(i,a,b) for i,(a,b) in enumerate(zip(local[0],remote[0])) if a!=b])
   assert len(local)==1 and local[0][2]==0
   outputs.append({'ctor_args':ctor,'outlist':out,'local':[net.canonical(p).hex() for p in local],
                   'remote':[net.canonical(p).hex() for p in remote],'wire':[p.hex() for p in m.sent],
                   'return':sent,'network_sites_original':True,'no_dolist_before_sender':True})
  assert outputs[0]==outputs[1],(protocol,route)
  rows.append({'protocol':protocol,'route':route,'network_option':False,'owner_option':False,
               'gateway_off_vs_on_exact_equal':True,'outlist_house':0 if route=='native' else 1,
               'sent_local_and_remote_house':0,'constructor_args_equal':True,'wire_bytes_equal':True,
               'no_dolist_before_sender':True,'status':'PASS'})
Path(ns['a'].output).write_text(json.dumps({'scope':__doc__,'dll_sha256':hashlib.sha256(Path(ns['a'].dll).read_bytes()).hexdigest(),
'game_tested':False,'multiplayer_tested':False,
'limitations':['Synthetic Win32/world; original constructor, sender and compressed decoder run.','No baseline 1.0.38 DLL executed; gateway-disabled behavior compared with release default-enabled behavior.','No Windows platform, full simulation or real netplay.'],
'results':rows},indent=2)+'\n')
print('PASS',len(rows),'N0 O0 exact comparison cases')
