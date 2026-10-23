"""Actual manager-owned thunks and x86 enumeration/prune branches; synthetic types and Win32. No live production/netplay claim."""
from pathlib import Path
exec(compile(Path(__file__).with_name('tech_compat_runtime.py').read_text().split('for session in (0,3,4,5):',1)[0],str(Path(__file__).with_name('tech_compat_runtime.py')),'exec'))
sites={0x44574A:5,0x445795:5,0x4457D0:5,0x445810:5,0x6AA788:6}
def original(m,at,n):return m.game.get_data(at-0x400000,n)
def restored(m):
 for at,n in list(sites.items())+[(0x6A97D2,5),(0x6AA781,5)]:assert bytes(m.u.mem_read(at,n))==original(m,at,n),hex(at)
def run_slice(m,start,ends,registers):
 reached=[];handles=[]
 for address in ends:
  def stop(uc,at,size,_):reached.append(at);uc.emu_stop()
  handles.append(m.u.hook_add(UC_HOOK_CODE,stop,begin=address,end=address))
 sp=m.STACK+0x1e000;m.u.reg_write(UC_X86_REG_ESP,sp)
 for reg,val in registers.items():m.u.reg_write(reg,val)
 m.u.emu_start(start,m.STOP,count=1000)
 for h in handles:m.u.hook_del(h)
 assert m.u.reg_read(UC_X86_REG_ESP)==sp
 assert len(reached)==1
 return reached[0]
for session in (0,3,4,5):
 m=fixture(session);s=issue(m,10,1,2);assert s.data.techOpen==3 and s.data.hookInstalled==5 and C.c_int32(s.result).value>=0,(s.result,s.data.techError,s.data.hookInstalled)
 assert not m.sidebar_add_calls and not m.r(m.house+0x1fc)&255
 for site in (0x6A97D2,0x6AA781):assert bytes(m.u.mem_read(site,5))==original(m,site,5)
 arr=m.alloc();m.w(arr,m.t)
 for at,base,yes,no in [(0x44574A,UC_X86_REG_EAX,0x445761,0x44576E),(0x445795,UC_X86_REG_EDX,0x4457A6,0x4457B3),(0x4457D0,UC_X86_REG_ECX,0x4457E7,0x4457F4),(0x445810,UC_X86_REG_EAX,0x445827,0x445834)]:
  for level in (-1,0,10,-2):
   m.w(m.t+0x634,level);before=len(m.canbuild_calls)
   assert run_slice(m,at,(yes,no),{base:arr,UC_X86_REG_ESI:0})==(no if level==-1 else yes)
   assert len(m.canbuild_calls)==before
 for zf in (0,0x40):assert run_slice(m,0x6AA788,(0x6AA78E,0x6AAA68),{UC_X86_REG_EFLAGS:2|zf})==0x6AAA68
 for impl in (0,1,2,2,0,2):
  s=issue(m,10,1,impl);assert s.data.techOpen==impl+1 and s.data.hookInstalled==(5 if impl==2 else 2)
 s=issue(m,10,0);assert not s.data.techOpen and not s.data.hookInstalled;restored(m)
 record(f'{session}_four_enumerators_levels_stack_prune_switch_disable')
# Actual House recheck -> actual Building UpdateConstructionOptions -> patched
# enumerator. AddCameo itself is a sink; no factory or native factory gates are
# synthesized by Toolbox. Tick/Produce must never add an absent cameo directly.
for case in ('factory','none','limbo','inactive','not_constructed','selling','foreign','hidden_type'):
 m=fixture(0);m.b(m.building+0x41b,1);m.w(m.t+0x634,1)
 if case=='none':m.vector(m.house+0x68,[])
 if case=='limbo':m.b(m.building+0x81,1)
 if case=='inactive':m.b(m.building+0x660,0)
 if case=='not_constructed':m.b(m.building+0x41b,0)
 if case=='selling':m.w(m.building+0xac,19)
 if case=='foreign':m.w(m.building+0x21c,m.enemy)
 if case=='hidden_type':m.w(m.t+0x634,0xffffffff)
 issue(m,10,1,2);assert not m.sidebar_add_calls and not m.r(m.house+0x1fc)&255
 s=issue(m,12,1);assert C.c_int32(s.result).value==-9 and not m.sidebar_add_calls and count(m)==0
 for at in (0x6a7d20,0x50af10,0x50b1d0):m.u.hook_add(UC_HOOK_CODE,lambda uc,at,size,_:m.ret(0,0),begin=at,end=at)
 # Model a later native building-completion trigger; the tool never sets it.
 m.b(m.house+0x1fc,1)
 run_slice(m,0x4f9265,(0x4f9302,),{UC_X86_REG_ESI:m.house})
 assert m.sidebar_add_calls==([(40,0)] if case=='factory' else []),(case,m.sidebar_add_calls)
 assert not m.r(m.house+0x1fc)&255
 before=list(m.sidebar_add_calls);m.clock+=1100;tick(m)
 assert m.sidebar_add_calls==before and not m.r(m.house+0x1fc)&255
 record('master_native_recheck_'+case)
for corrupt in list(sites)+[0x445755,0x4457A2,0x4457DD,0x44581A]:
 m=fixture();m.u.mem_write(corrupt,b'\xcc');s=issue(m,10,1,2)
 assert C.c_int32(s.result).value==-11 and not s.data.techOpen and not s.data.hookInstalled
 assert bytes(m.u.mem_read(corrupt,1))==b'\xcc'
 for at,n in sites.items():
  if at!=corrupt:assert bytes(m.u.mem_read(at,n))==original(m,at,n)
 record(f'signature_{corrupt:X}_group_rejected_without_writes')
for end in ('flag_off','stop','round_end','foreign'):
 m=fixture();issue(m,10,1,2)
 if end=='foreign':m.u.mem_write(0x445795,b'\xcc'*5)
 s=m.Shared.from_buffer_copy(m.u.mem_read(m.SH,C.sizeof(m.Shared)))
 if end=='flag_off':s.flags=0
 if end in ('stop','foreign'):s.stop=1
 if end=='round_end':m.b(0xa8e9a0,0);m.w(0xa83d4c,0)
 m.u.mem_write(m.SH,bytes(s));tick(m)
 if end!='foreign':restored(m)
 else:
  assert bytes(m.u.mem_read(0x445795,5))==b'\xcc'*5
  for at,n in sites.items():
   if at!=0x445795:assert bytes(m.u.mem_read(at,n))==original(m,at,n)
 record(end+'_owned_cleanup')
Path(a.output).write_text(json.dumps({'scope':__doc__,'game_sha256':hashlib.sha256(Path(a.game).read_bytes()).hexdigest(),'dll_sha256':hashlib.sha256(Path(a.dll).read_bytes()).hexdigest(),'results':rows},indent=2)+'\n')
