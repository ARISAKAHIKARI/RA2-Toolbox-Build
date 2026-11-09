"""Execute compiled Runtime selected-Owner Destruct through its exported hook.

Maps a user-supplied PE and DLL inside Unicorn; never starts/attaches a game.
The original EventClass constructor executes. Win32 and clock are fixture IO.
"""
import argparse,ctypes as C,hashlib,json,struct,sys
from pathlib import Path
from unicorn import UC_HOOK_CODE
parser=argparse.ArgumentParser();parser.add_argument('game',type=Path);parser.add_argument('dll',type=Path);parser.add_argument('--out',required=True,type=Path);parser.add_argument('--allow-old-selection-result',action='store_true');args=parser.parse_args()
here=Path(__file__).resolve().parent;shared_fixture=here/'entry_runtime_regression.py'
game_sha=hashlib.sha256(args.game.read_bytes()).hexdigest();assert game_sha=='268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0'
dll_sha=hashlib.sha256(args.dll.read_bytes()).hexdigest();pdb_sha=hashlib.sha256(args.dll.with_suffix('.pdb').read_bytes()).hexdigest();shared_source=shared_fixture.read_text(encoding='utf-8-sig');shared_sha=hashlib.sha256(shared_fixture.read_bytes()).hexdigest()
ns={'__name__':'selected_destruct_fixture','__file__':str(shared_fixture),'fixture_paths':[str(args.game.resolve()),str(args.dll.resolve())]}
exec(compile(shared_source,str(shared_fixture),'exec'),ns)
u=ns['u'];w=ns['w'];r=ns['r'];b=ns['b'];alloc=ns['alloc'];vector=ns['vector'];Shared=ns['Shared'];SH=ns['SH'];hook=ns['hook'];house=ns['house'];enemy=ns['enemy'];unit=ns['unit'];building=ns['building'];enemyUnit=ns['enemyUnit']
def snapshot():return Shared.from_buffer_copy(u.mem_read(SH,C.sizeof(Shared)))
def publish(s):u.mem_write(SH,bytes(s))
def queue_reset():
 for at in (0x8b41f8,0x8b41fc,0x8b4200,0xa802c8,0xa802cc,0xa802d0):w(at,0)
def queues():return {'single':r(0x8b41f8),'multi':r(0xa802c8)}
house_type=alloc(0x1000);enemy_type=alloc(0x1000)
for owner,typ,index,name in ((house,house_type,0,b'Americans'),(enemy,enemy_type,1,b'Russians')):
 w(owner+0x10,100+index);w(owner+0x30,index);w(owner+0x34,typ);u.mem_write(typ+0x24,name+b'\0');b(typ+0x1a6,0)
 for offset in (0x1f5,0x1f6,0x1f7,0x1f8):b(owner+offset,0)
 b(owner+0x1ed,1);b(owner+0x1ec,index==0)
vector(0xa8da74,[]);w(0xa8b230,alloc());w(0xa8b24c,0)
for at in (unit,building,enemyUnit):b(at+0x83,1);b(at+0x81,0)
def ready(session=5,selected=None):
 ns['timer_side_effect']=None;queue_reset();w(0xa8b238,session);w(0xa8ed84,r(0xa8ed84)+10)
 vector(0xa8ecb8,[unit] if selected is None else selected)
 s=snapshot();s.flags=0;s.networkHousePreserve=0;s.controlAll=0;s.computeRequested=0;s.computeApplied=0;s.request=s.ack;s.arg=s.arg2=s.requestToken=0;publish(s)
 s=hook();assert s.data.status==1 and s.data.commandStatus==1,('not ready',s.data.status,s.data.commandStatus)
 queue_reset();return s
def issue(arg=0,arg2=0,token=0):
 s=snapshot();s.command=32;s.arg=arg;s.arg2=arg2;s.requestToken=token;s.requestTick=ns['clock'];s.requestMatch=s.data.match;s.request+=1;publish(s)
 s=hook();assert s.request==s.ack,('missing ack',s.request,s.ack)
 return s
rows=[]
def record(name,s,expected,expected_house=None):
 result=C.c_int32(s.result).value;q=queues();assert result==expected,(name,result,expected,s.data.lastSubmitError,q);assert s.count==(1 if expected>0 else 0),(name,s.count)
 assert q==({'single':1,'multi':0} if expected>0 and r(0xa8b238) in (0,5) else {'single':0,'multi':1} if expected>0 else {'single':0,'multi':0}),(name,q)
 row={'case':name,'result':result,'count':s.count,'last_submit_error':s.data.lastSubmitError,'queue':q}
 if expected>0:
  at=0x8b4204 if q['single'] else 0xa802d4;packet=bytes(u.mem_read(at,111));assert packet[0]==8 and packet[1]==0 and packet[2]==expected_house,(name,packet[:12].hex(),expected_house);row['packet_header_hex']=packet[:7].hex();row['packet_house']=packet[2];row['packet_frame']=struct.unpack_from('<I',packet,3)[0];assert row['packet_frame']==r(0xa8ed84)+(1 if q['single'] else 0),(name,row['packet_frame'])
 rows.append(row)
ready();s=issue();record('same-owner SP native constructor and DoList',s,1,0)
ready(3);s=issue();record('same-owner MP native constructor and OutList',s,1,0)
ready(selected=[unit,building]);s=issue();record('multiple selected units with identical Owner',s,1,0)
ready(selected=[enemyUnit]);s=issue();record('SP selected foreign Owner retains House index',s,1,1)
ready(selected=[]);s=issue();record('empty selection',s,-7)
ready(selected=[unit,enemyUnit]);s=issue();record('mixed Owners',s,-7)
ready(3,selected=[enemyUnit]);s=issue();record('MP foreign Owner with preservation off',s,-42)
def foreign_ready():
 ready(3,selected=[enemyUnit]);s=snapshot();s.networkHousePreserve=1;publish(s);s=hook();assert s.networkHouseStatus==1,('preservation unavailable',s.networkHouseStatus);queue_reset()
foreign_ready();s=issue();record('MP selected foreign Owner with actual managed preservation group',s,1,1)
foreign_ready();ns['timer_side_effect']=lambda:w(SH+Shared.networkHousePreserve.offset,0);s=issue();ns['timer_side_effect']=None;record('gameSendTiming callback: preservation disabled',s,-42)
for label,change,restore in [
 ('unit UID change',lambda:w(unit+0x10,77),lambda:w(unit+0x10,10)),
 ('Owner change',lambda:w(unit+0x21c,enemy),lambda:w(unit+0x21c,house)),
 ('House UID change',lambda:w(house+0x10,10101),lambda:w(house+0x10,100)),
 ('House index change',lambda:w(house+0x30,1),lambda:w(house+0x30,0)),
 ('selection vector change',lambda:vector(0xa8ecb8,[building]),lambda:vector(0xa8ecb8,[unit])),
 ('selected flag cleared',lambda:b(unit+0x83,0),lambda:b(unit+0x83,1)),
]:
 ready();ns['timer_side_effect']=change;s=issue();ns['timer_side_effect']=None
 expected=C.c_int32(s.result).value if args.allow_old_selection_result else -7
 assert expected<0;record('gameSendTiming callback: '+label,s,expected);restore()
ready();original_scenario=r(0xa8b230);ns['timer_side_effect']=lambda:w(0xa8b230,original_scenario+0x1000);s=issue();ns['timer_side_effect']=None;w(0xa8b230,original_scenario)
expected=C.c_int32(s.result).value if args.allow_old_selection_result else -7;assert expected<0;record('gameSendTiming callback: Scenario identity change',s,expected)
ready();old_frame=r(0xa8ed84);ns['timer_side_effect']=lambda:w(0xa8ed84,old_frame+1);s=issue();ns['timer_side_effect']=None;expected=C.c_int32(s.result).value;assert expected<0;record('gameSendTiming callback: simulation frame advanced',s,expected)
ready();ns['timer_side_effect']=lambda:w(0xa83d4c,enemy);s=issue();ns['timer_side_effect']=None;w(0xa83d4c,house);expected=C.c_int32(s.result).value;assert expected<0;record('gameSendTiming callback: current player House changed',s,expected)
# A real checked ReadProcessMemory failure after the external clock boundary
# rejects. Wrap only the Win32 IO sink; compiled capture code is untouched.
read_target=next(a for a,n in ns['imports'].items() if n=='ReadProcessMemory');u.hook_del(ns['native_stub_hooks'][read_target]);fail={'address':None}
def checked_read(uc,a,size,data):
 sp=uc.reg_read(ns['UC_X86_REG_ESP']);at=r(sp+8);count=r(sp+16)
 if fail['address'] is not None and at<=fail['address']<at+count:ns['ret'](0,5)
 else:ns['on_code'](uc,a,size,data)
u.hook_add(UC_HOOK_CODE,checked_read,begin=read_target,end=read_target)
ready();ns['timer_side_effect']=lambda:fail.update(address=unit+0x21c);s=issue();ns['timer_side_effect']=None;fail['address']=None
expected=C.c_int32(s.result).value if args.allow_old_selection_result else -7;assert expected<0;record('gameSendTiming callback: checked Owner read failure',s,expected)
ready();s=issue(house,100,0);record('legacy explicit House pointer mode remains available',s,1,0)
ready();s=issue(enemy,101,1);record('legacy explicit foreign House pointer retains SP House index',s,1,1)
ready();s=issue(0,1,0);record('arg0 rejects unexpected arg2',s,-7)
ready();s=issue(0,0,1);record('arg0 rejects unexpected House token',s,-7)
# Native DoList skips AI Houses. The SP backend must reach the real receiver
# exactly once, without changing CurrentPlayer, Owner or human-player flags.
responded=[]
u.hook_add(UC_HOOK_CODE,lambda uc,at,size,_:responded.append(at),begin=0x4c6cb0,end=0x4c6cb0)
for mode in (0,5):
 for explicit in (False,True):
  b(enemy+0x1ed,0);b(enemy+0x1f6,0)
  s=ready(mode,selected=[enemyUnit]);queued=s.data.queuedEvents;responded.clear()
  original_player=r(0xa83d4c);original_owner=r(enemyUnit+0x21c)
  s=issue(enemy,101,1) if explicit else issue()
  assert s.result==2 and s.count==1 and responded==[0x4c6cb0],(mode,explicit,s.result,responded)
  assert queues()=={'single':0,'multi':0} and s.data.queuedEvents==queued
  assert u.mem_read(enemy+0x1f6,1)==b'\x01' and r(enemy+0x298)==r(0xa8ed84) and r(enemy+0x2a0)==0
  assert not u.mem_read(house+0x1f6,1)[0] and not u.mem_read(enemy+0x1ed,1)[0]
  assert r(0xa83d4c)==original_player and r(enemyUnit+0x21c)==original_owner
  hook();assert responded==[0x4c6cb0] # Ack does not repeat immediate response.
  rows.append({'case':f'SP AI original response once session {mode} explicit {explicit}',
               'result':2,'native_response_calls':1,'queue':queues()})
  b(enemy+0x1f6,0)
for fault in ('receiver signature','selection changed','changed to MP','read only'):
 b(enemy+0x1ed,0);ready(5,selected=[enemyUnit]);responded.clear()
 original=bytes(u.mem_read(0x4c6cb0,13))
 if fault=='receiver signature':u.mem_write(0x4c6cb0,b'\x90')
 elif fault=='selection changed':ns['timer_side_effect']=lambda:vector(0xa8ecb8,[unit])
 elif fault=='changed to MP':ns['timer_side_effect']=lambda:w(0xa8b238,3)
 else:ns['timer_side_effect']=lambda:w(SH+Shared.readOnly.offset,1)
 s=issue();ns['timer_side_effect']=None;u.mem_write(0x4c6cb0,original)
 assert C.c_int32(s.result).value<0 and not responded and queues()=={'single':0,'multi':0}
 assert not u.mem_read(enemy+0x1f6,1)[0]
 if fault=='receiver signature':assert C.c_int32(s.result).value==-12
 rows.append({'case':'SP AI rejects '+fault,'result':C.c_int32(s.result).value,'native_response_calls':0})
 s.readOnly=0;publish(s)
b(enemy+0x1ed,1)
foreign_ready();b(enemy+0x1ed,0);responded.clear();s=issue()
assert C.c_int32(s.result).value==-4 and not responded and queues()=={'single':0,'multi':0}
rows.append({'case':'MP AI never uses SP immediate response','result':-4,'native_response_calls':0})
b(enemy+0x1ed,1)
assert hashlib.sha256(args.game.read_bytes()).hexdigest()==game_sha
assert hashlib.sha256(args.dll.read_bytes()).hexdigest()==dll_sha and hashlib.sha256(args.dll.with_suffix('.pdb').read_bytes()).hexdigest()==pdb_sha,'DLL/PDB changed during the fixture'
assert hashlib.sha256(shared_fixture.read_bytes()).hexdigest()==shared_sha,'Shared ABI fixture changed during execution'
report={'scope':'Compiled GameRuntimeHook -> command -> original 0x4C66C0 ctor -> SP DoList/MP OutList or SP AI original 0x4C6CB0 response. Synthetic world; Win32 IO and clock callback are fixture sinks. No game process, complete House AI destruction, platform or multiplayer acceptance.',
 'game_sha256':game_sha,'dll_sha256':dll_sha,'pdb_sha256':pdb_sha,'fixture_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),'shared_fixture_sha256':shared_sha,'strict_selection_result':not args.allow_old_selection_result,'passed':True,'cases':rows}
args.out.parent.mkdir(parents=True,exist_ok=True);args.out.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
