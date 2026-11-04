"""Execute the compiled Runtime permanent-grant switch against original x86.

Win32 (including failed worker creation) and local presentation are explicit harness seams. Native map action34,
Grant and timer registration execute. No game process, Launch or network peers.
The negative signature case deliberately corrupts/restores emulator code only.
"""
import argparse,hashlib,importlib.util,json,struct
from pathlib import Path
from unicorn import UC_HOOK_CODE

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('game',type=Path);p.add_argument('dll',type=Path);p.add_argument('--output',type=Path,required=True)
a=p.parse_args();spec=importlib.util.spec_from_file_location('sw_grant_fixture',Path(__file__).with_name('entry_runtime_regression.py'))
m=importlib.util.module_from_spec(spec);m.fixture_paths=[str(a.game),str(a.dll)];spec.loader.exec_module(m)
u=m.u;w=m.w;r=m.r;b=m.b;rows=[];actions=[]
state=lambda:m.Shared.from_buffer_copy(u.mem_read(m.SH,m.C.sizeof(m.Shared)))
save=lambda s:u.mem_write(m.SH,bytes(s))
for at,name in m.imports.items():
 if name=='CreateThread':
  u.hook_del(m.native_stub_hooks[at]);u.hook_add(UC_HOOK_CODE,lambda cpu,pc,size,user:m.ret(0,6),begin=at,end=at)
def check(name,condition):
 assert condition,name
 rows.append(dict(case=name,status='PASS'))

w(0xA8B230,m.alloc());w(0xB73550,1);w(0x8809A0,-1);w(m.house+0x10,101);b(0xA8B263,0)
supers=[];types=[]
for i in range(2):
 sw=m.alloc();typ=m.alloc();supers.append(sw);types.append(typ)
 w(sw+0x10,201+i);w(sw+0x24,-1);w(sw+0x28,typ);w(sw+0x2C,m.house);w(sw+0x30,-1);b(sw+0x60,1)
 w(typ+0x10,301+i);w(typ+0x98,i);w(typ+0xB0,900);b(typ+0xE7,1);b(typ+0xF4,1)
 u.mem_write(typ+0x24,('SW'+str(i)).encode()+b'\0')
m.vector(m.house+0x254,supers)
timers=m.alloc(256*4);u.mem_write(0xA83D50,struct.pack('<6I',0,timers,256,0x101,0,10))
u.hook_add(UC_HOOK_CODE,lambda cpu,pc,size,user:actions.append(r(cpu.reg_read(m.UC_X86_REG_ECX)+0x90)),begin=0x6E1C40,end=0x6E1C40)
# Original AddCameo/GetTab already have explicit sinks in the shared harness.
u.hook_add(UC_HOOK_CODE,lambda cpu,pc,size,user:m.ret(0,1),begin=0x6A60A0,end=0x6A60A0)
s=state();s.flags=0;save(s);s=m.hook()
check('default_off_no_native_grants',not actions and s.superweaponGrantStatus==0 and not u.mem_read(supers[0]+0x6D,1)[0])
s.superweaponGrant=1;save(s);s=m.hook()
check('disabled_sw_session_grants_all_registered_supers',actions==[0,1] and s.superweaponGrantStatus==1 and s.superweaponGrantCount==2 and not u.mem_read(0xA8B263,1)[0])
check('native_permanent_flags_and_normal_charge',all(u.mem_read(sw+0x6D,2)==b'\x01\x00' and not u.mem_read(sw+0x60,1)[0] and r(sw+0x30)==100 and r(sw+0x38)==900 and not u.mem_read(sw+0x6F,1)[0]for sw in supers))
before=actions[:];s=m.hook();check('same_game_frame_does_not_repeat',actions==before)
original=[bytes(u.mem_read(sw,0x80))for sw in supers]
for frame in range(101,281):
 w(0xA8ED84,frame);s=m.hook()
 assert all(bytes(u.mem_read(sw,0x80))==old for sw,old in zip(supers,original))
check('one_grant_each_observed_frame_for_180_frames',len(actions)==362 and s.superweaponGrantFrame==280)
check('timer_registration_is_not_duplicated',r(0xA83D60)==2)
# An actual physical message reaches the existing dispatcher without ready=1.
message=m.CONTROL+0x200;w(0xA8ED84,281);w(m.CONTROL+16,1);w(m.CONTROL+20,0)
u.mem_write(message,struct.pack('<7I',1,0x200,0,0,0,0,0));m.run(m.entry,(0,1,message));s=state()
check('physical_message_samples_new_frame_without_host_ready',len(actions)==364 and s.superweaponGrantFrame==281 and r(m.CONTROL+20)==0)
# Lose is modeled by resetting this one instance, without touching another SW.
b(supers[1]+0x6D,0);b(supers[1]+0x60,1);w(0xA8ED84,282);s=m.hook()
check('missing_super_is_regranted_next_frame',len(actions)==366 and u.mem_read(supers[1]+0x6D,1)[0] and not u.mem_read(supers[1]+0x60,1)[0] and r(supers[1]+0x30)==282)
s.superweaponGrant=0;save(s);before=len(actions);w(0xA8ED84,283);s=m.hook()
check('off_stops_calls_and_preserves_native_grants',len(actions)==before and s.superweaponGrantStatus==0 and all(u.mem_read(sw+0x6D,1)[0]for sw in supers))
s.superweaponGrant=1;save(s);s=m.hook();check('reenable_in_same_frame_is_fresh_request',len(actions)==before+2 and s.superweaponGrantStatus==1)
before=len(actions);b(m.house+0x1F5,1);w(0xA8ED84,284);s=m.hook();check('defeated_house_is_not_granted',len(actions)==before and s.superweaponGrantStatus==2);b(m.house+0x1F5,0)
clean=bytes(u.mem_read(0x6E1C40,1));u.mem_write(0x6E1C40,b'\xCC');w(0xA8ED84,285);s=m.hook()
check('incompatible_signature_refuses_native_call',len(actions)==before and s.superweaponGrantStatus==-1);u.mem_write(0x6E1C40,clean)
w(supers[0]+0x2C,m.enemy);w(0xA8ED84,286);s=m.hook();check('foreign_owner_refused',len(actions)==before and s.superweaponGrantStatus==-2);w(supers[0]+0x2C,m.house)
w(0xA8ED84,287);s=m.hook();check('later_valid_frame_recovers',len(actions)==before+2 and s.superweaponGrantStatus==1)
check('no_fire_or_production_events_queued',r(0x8B41F8)==0 and r(0xA802C8)==0)
report=dict(scope=__doc__,passed=True,checks=len(rows),cases=rows,grant_calls=len(actions),per_frame_repeated_frames=180,
 game_sha256=hashlib.sha256(a.game.read_bytes()).hexdigest(),dll_sha256=hashlib.sha256(a.dll.read_bytes()).hexdigest(),
 game_process_accessed=False,launch_executed=False,multiplayer_tested=False)
a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(report,indent=2)+'\n')
print('PASS:',len(rows),'compiled Runtime switch/original grant cases;',len(actions),'native map actions; no Launch/netplay')
