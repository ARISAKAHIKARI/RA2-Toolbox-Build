"""Actual compiled DLL startup against captured signature/body bytes.
Win32 APIs, world state and platform callees are fixtures, not a live platform test.
Private capture bytes are supplied separately and are never included in releases.
"""
import argparse,ctypes as C,hashlib,importlib.util,json,struct
from pathlib import Path
from types import SimpleNamespace
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
p=argparse.ArgumentParser();p.add_argument('game');p.add_argument('dll');p.add_argument('--capture',type=Path,required=True);p.add_argument('--baseline-dll');p.add_argument('--output',required=True);a=p.parse_args()
report=json.loads((a.capture/'platform-signatures.json').read_text())
changed=[s for s in report['signatures'] if s['state']!='match']
assert len(changed)==1 and changed[0]['address']=='0x431410'
record=changed[0];original_base=int(record['target_region']['allocation_base'],16)
body=(a.capture/record['target_file']).read_bytes()[:0x127];relocations=(4,63,99,105,279)
rows=[]
def fixture(case='captured',base=original_base,dll=None):
 spec=importlib.util.spec_from_file_location('platform_startup_fixture',Path(__file__).with_name('entry_runtime_regression.py'));m=importlib.util.module_from_spec(spec);m.fixture_paths=[a.game,dll or a.dll];spec.loader.exec_module(m)
 m.u.mem_map(base,0xBBD000);module=SimpleNamespace(OPTIONAL_HEADER=SimpleNamespace(ImageBase=base,SizeOfImage=0xBBD000));m.images.append(module)
 m.u.mem_write(base,b'MZ');m.w(base+0x3c,0x80);m.w(base+0x80,0x4550);m.u.mem_write(base+0x84,struct.pack('<H',0x14c));m.u.mem_write(base+0x98,struct.pack('<H',0x10b));m.w(base+0xd0,0xBBD000)
 code=bytearray(body)
 for off in relocations:struct.pack_into('<I',code,off,struct.unpack_from('<I',code,off)[0]-original_base+base)
 target=base+0x12FB60;m.u.mem_write(target,bytes(code))
 for sig in report['signatures']:
  at=int(sig['address'],16);value=bytes.fromhex(sig['snapshot_actual'])
  if at==0x431410:value=b'\xe9'+struct.pack('<I',(target-at-5)&0xffffffff)+value[5:]
  m.u.mem_write(at,value)
 if case=='native':m.u.mem_write(0x431410,m.game.get_data(0x31410,8))
 if case=='bad_target':m.w(0x431411,target-0x431410-5+1)
 if case=='bad_body':m.b(target+0x126,1)
 if case=='bad_header':m.b(base,0)
 if case=='bad_size':m.w(base+0xd0,0xBBC000)
 if case=='other_signature':m.b(0x4c6970,0x90)
 # Replace only three explicitly modelled APIs; keep other production imports.
 handlers={v:k for k,v in m.imports.items() if v in ('GetModuleHandleW','ReadProcessMemory','VirtualQuery')}
 for name,at in handlers.items():
  del m.imports[at]
  def api(u,where,size,_,name=name):
   sp=u.reg_read(UC_X86_REG_ESP);arg=lambda i:m.r(sp+4+4*i)
   if name=='GetModuleHandleW':
    n=m.string_at(arg(0),True) if arg(0) else ''
    value=m.game.OPTIONAL_HEADER.ImageBase if not n else base if n.upper()=='MRGA1.DLL' and case!='missing_module' else m.dll.OPTIONAL_HEADER.ImageBase if n=='GameRuntime.dll' else 0
    m.ret(value,1)
   elif name=='ReadProcessMemory':
    fail=(case=='unreadable_body' and arg(1)==target) or (case=='unreadable_entry' and arg(1)==0x431410)
    if not fail:u.mem_write(arg(2),bytes(u.mem_read(arg(1),arg(3))))
    m.w(arg(4),0 if fail else arg(3));m.ret(not fail,5)
   else:
    image=m.module_at(arg(0));allocation=image.OPTIONAL_HEADER.ImageBase if image else arg(0)&~4095
    protection=0x20;kind=0x1000000;count=4096
    if arg(0)==target:
     if case=='wrong_owner':allocation=base+4096
     if case=='non_image':kind=0x20000
     if case=='non_exec':protection=4
     if case=='guarded':protection|=0x100
     if case=='short_region':count=0x100
    u.mem_write(arg(1),struct.pack('<7I',arg(0)&~4095,allocation,protection,count,0x1000,protection,kind));m.ret(28,3)
  m.u.hook_add(UC_HOOK_CODE,api,begin=at,end=at)
 m.w(0xa8b238,5);s=m.Shared.from_buffer_copy(m.u.mem_read(m.SH,C.sizeof(m.Shared)));s.flags=0;s.externalDisplay=1;m.u.mem_write(m.SH,bytes(s))
 return m,target,bytes(code)
def state(m):return m.Shared.from_buffer_copy(m.u.mem_read(m.SH,C.sizeof(m.Shared)))
def record_case(name):rows.append({'case':name,'status':'PASS'});print('PASS',name,flush=True)
if a.baseline_dll:
 m,_,_=fixture(dll=a.baseline_dll);assert m.hook().data.status==-1;record_case('old_dll_reproduces_captured_startup_rejection')
for case in ('native','captured','missing_module','bad_target','bad_body','bad_header','bad_size','other_signature','unreadable_body','unreadable_entry','wrong_owner','non_image','non_exec','guarded','short_region'):
 m,target,code=fixture(case);before=bytes(m.u.mem_read(0x431410,8)),bytes(m.u.mem_read(target,0x127));s=m.hook()
 assert s.data.status==(1 if case in ('native','captured') else -1),(case,s.data.status)
 assert before==(bytes(m.u.mem_read(0x431410,8)),bytes(m.u.mem_read(target,0x127)))
 assert m.r(0x8b41f8)==m.r(0xa802c8)==0
 record_case(case+'_startup_gate_no_patch_no_event')
for base in (0x40000000,0x68000000):
 m,target,code=fixture(base=base);assert m.hook().data.status==1;record_case('rebased_module_'+hex(base))
# A later foreign change must not be executed by ClearBeacons after cached startup.
m,target,code=fixture();s=m.hook();m.b(target+20,m.u.mem_read(target+20,1)[0]^1)
s.command=8;s.request=1;s.requestTick=m.clock;s.requestMatch=s.data.match;m.u.mem_write(m.SH,bytes(s));s=m.hook()
assert C.c_int32(s.result).value==-11 and not any(c[0]==0x431410 for c in m.calls)
record_case('beacon_command_rechecks_changed_body')
# Execute captured replacement machine code, stubbing only unavailable platform
# callees; check thiscall argument/stack and callee-saved registers on key branches.
for index in (-1,0,7,8):
 m,target,code=fixture();assert m.hook().data.status==1
 array=m.alloc(64);items=m.alloc(64);m.w(original_base+0x603120,array);m.w(array+0x10,8);m.w(array+4,items)
 for i in range(8):m.w(items+4*i,1)
 for rva,argc,result in ((0x17ec60,0,0),(0x130870,0,0),(0x131170,3,0)):
  at=original_base+rva
  m.u.hook_add(UC_HOOK_CODE,lambda u,a,n,_,argc=argc,result=result:m.ret(result,argc),begin=at,end=at)
 regs=(UC_X86_REG_EBX,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP)
 for i,reg in enumerate(regs):m.u.reg_write(reg,0x12345670+i)
 m.run(target,(index&0xffffffff,),0x89c3b0)
 assert m.u.reg_read(UC_X86_REG_ESP)==m.STACK+0x1e000+8
 assert [m.u.reg_read(r) for r in regs]==[0x12345670+i for i in range(4)]
 record_case('captured_beacon_thiscall_ret4_index_'+str(index))
Path(a.output).write_text(json.dumps({'scope':__doc__,'dll_sha256':hashlib.sha256(Path(a.dll).read_bytes()).hexdigest(),'capture_body_sha256':hashlib.sha256(body).hexdigest(),'real_game_execution':False,'results':rows},indent=2)+'\n')
