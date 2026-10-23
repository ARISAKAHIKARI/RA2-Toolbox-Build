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
