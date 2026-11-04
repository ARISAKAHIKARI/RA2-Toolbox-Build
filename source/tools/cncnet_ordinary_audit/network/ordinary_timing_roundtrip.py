"""Bounded ordinary-v17 event-size hooks with the released Toolbox sender.

Actual ordinary DLL exports supply extended-event sizes; a modeled Syringe
REGISTERS bridge invokes those exports and applies their register/continuation
results. Real game constructors/sender/codec and actual Toolbox thunks run.
Synthetic Win32/world/capture-only transport; not full Spawner initialization,
ProtocolZero timing/sockets, event effects or real multiplayer validation.
"""
import argparse,hashlib,json,struct,sys
from pathlib import Path
import pefile
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--game',type=Path,required=True,help='Authorized supported PE32 engine fixture')
parser.add_argument('--runtime',type=Path,required=True,help='Exact released GameRuntime.dll')
parser.add_argument('--repo',type=Path,required=True,help='RA2-Toolbox-Build source checkout')
parser.add_argument('--output',type=Path,required=True)
parser.add_argument('--spawner',type=Path,required=True,help='Official non-Hardened CnCNetYR v17 DLL, SHA-256 checked')
options=parser.parse_args()
repo=options.repo.resolve()
options.output.parent.mkdir(parents=True,exist_ok=True)

# These cases require the original native sender continuation. Do not repair a
# modified/private engine fixture merely to make a network test complete.
game_pe=pefile.PE(str(options.game))
source=0x64b9f7;expected=bytes.fromhex('a1f8418b00')
image_base=game_pe.OPTIONAL_HEADER.ImageBase
mapped=game_pe.get_memory_mapped_image()
actual=mapped[source-image_base:source-image_base+len(expected)]
preflight={'source':hex(source),'expected_bytes':expected.hex(),'actual_bytes':actual.hex()}
if actual!=expected:
 if actual[:1]==b'\xe9':
  target=source+5+int.from_bytes(actual[1:5],'little',signed=True)
  target_rva=target-image_base
  section=game_pe.get_section_by_rva(target_rva)
  if section:
   raw_offset=section.PointerToRawData+target_rva-section.VirtualAddress
   raw=options.game.read_bytes()[raw_offset:raw_offset+32]
   preflight.update({'branch_target':hex(target),'target_section':section.Name.rstrip(b'\0').decode(),
    'target_section_virtual_start':hex(image_base+section.VirtualAddress),
    'target_section_virtual_end':hex(image_base+section.VirtualAddress+section.Misc_VirtualSize),
    'section_alignment':hex(game_pe.OPTIONAL_HEADER.SectionAlignment),
    'file_alignment':hex(game_pe.OPTIONAL_HEADER.FileAlignment),
    'target_section_raw_start':hex(section.PointerToRawData),
    'target_section_raw_end':hex(section.PointerToRawData+section.SizeOfRawData),
    'target_section_raw_size':hex(section.SizeOfRawData),'target_file_offset':hex(raw_offset),
    'pefile_target_file_offset':hex(game_pe.get_offset_from_rva(target_rva)),
    'target_raw_bytes':raw.hex(),'target_mapped_bytes':mapped[target_rva:target_rva+32].hex(),
    'target_in_raw_padding':target_rva-section.VirtualAddress>=section.Misc_VirtualSize})
 out={'scope':__doc__,'status':'BLOCKED','reason':'Engine fixture changes required native sender continuation; no instruction replacement performed.',
  'game_sha256':hashlib.sha256(options.game.read_bytes()).hexdigest(),
  'GameRuntime.dll_sha256':hashlib.sha256(options.runtime.read_bytes()).hexdigest(),
  'ordinary_dll_sha256':hashlib.sha256(options.spawner.read_bytes()).hexdigest(),
  'preflight':preflight,'planned_cases':8,'results':[]}
 options.output.write_text(json.dumps(out,indent=2)+'\n')
 print('BLOCKED: engine fixture native continuation differs; see',options.output)
 sys.exit(2)

fixture=repo/'source/assist/tests/network_house_runtime.py'
ns={'__name__':'ordinary_timing_fixture','__file__':str(fixture)}
original_argv=sys.argv
sys.argv=[str(fixture),str(options.game),str(options.runtime),'--output',str(options.output)]
try:
 exec(compile(fixture.read_text().split('\nMIXED=',1)[0],str(fixture),'exec'),ns)
finally:
 sys.argv=original_argv
ordinary=options.spawner
assert hashlib.sha256(ordinary.read_bytes()).hexdigest()=='58f198973bd0fd3b0e1b2c42fc6c545efe1fd399680abc2e8d6e9e189751614d'
exports={0x64b6fe:'sub_64B660_GetEventSize',0x64be7d:'sub_64BDD0_GetEventSize1',0x64c30e:'sub_64BDD0_GetEventSize2'}
regs=[UC_X86_REG_EDI,UC_X86_REG_ESI,UC_X86_REG_EBP,UC_X86_REG_ESP,UC_X86_REG_EBX,UC_X86_REG_EDX,UC_X86_REG_ECX,UC_X86_REG_EAX]

def install_size_exports(m):
 pe=pefile.PE(data=ordinary.read_bytes());base=0x60000000
 pe.relocate_image(base);mapped=pe.get_memory_mapped_image()
 m.u.mem_map(base,(len(mapped)+0xfff)&~0xfff);m.u.mem_write(base,mapped)
 functions={e.name.decode():base+e.address for e in pe.DIRECTORY_ENTRY_EXPORT.symbols if e.name}
 context=m.alloc(128);callstack=m.alloc(4096)+2048;sentinel=m.alloc(16)
 m.u.mem_write(sentinel,b'\x90')
 pending=[];skip=set();hits={name:0 for name in exports.values()}
 def callback(uc,at,size,_):
  if at==sentinel:
   state,origin=pending.pop();continuation=uc.reg_read(UC_X86_REG_EAX)
   values=struct.unpack('<10I',uc.mem_read(context,40))
   uc.context_restore(state)
   for reg,value in zip(regs,values[2:]):uc.reg_write(reg,value)
   uc.reg_write(UC_X86_REG_EFLAGS,values[1])
   if continuation:uc.reg_write(UC_X86_REG_EIP,continuation)
   else:skip.add(origin);uc.reg_write(UC_X86_REG_EIP,origin)
   return
  if at in skip:skip.remove(at);return
  assert not pending,'size export recursively entered a network site'
  values=[at,uc.reg_read(UC_X86_REG_EFLAGS),*(uc.reg_read(reg) for reg in regs)]
  uc.mem_write(context,struct.pack('<10I',*values));pending.append((uc.context_save(),at))
  uc.mem_write(callstack,struct.pack('<II',sentinel,context));uc.reg_write(UC_X86_REG_ESP,callstack)
  hits[exports[at]]+=1;uc.reg_write(UC_X86_REG_EIP,functions[exports[at]])
 for at in exports:m.u.hook_add(UC_HOOK_CODE,callback,begin=at,end=at)
 m.u.hook_add(UC_HOOK_CODE,callback,begin=sentinel,end=sentinel)
 return hits

def key(p):
 if p[0]==0x30:return (p[0],p[2],int.from_bytes(p[3:7],'little'),list(p[7:9]))
 if p[0]==4:return (p[0],p[2],int.from_bytes(p[3:7],'little'),list(p[7:13]),list(p[14:30]))
 return (p[0],p[2])

rows=[]
for mode in (3,4):
 for adapted in (False,True):
  for mixed in (False,True):
   m=ns['setup'](mode,2);hits=install_size_exports(m)
   assert ns['toggle'](m,adapted).networkHouseStatus==int(adapted)
   events=[(4,0,10),(0x30,0,0),(4,0,11)] if mixed else [(0x30,0,0)]
   ns['fill'](m,events,126)
   for n,(kind,house,ident) in enumerate(events):
    if kind==0x30:
     at=0xa802d4+((126+n)&127)*111
     record=bytearray(111);record[0]=0x30;record[2]=house;record[3:7]=struct.pack('<I',m.r(0xa8ed84));record[7:9]=bytes([9,2])
     m.u.mem_write(at,bytes(record))
   count,local=ns['send'](m);remote=[p for p in ns['decode'](m) if p[0]!=0x1c]
   assert count==len(events) and len(local)==len(remote)==len(events)
   assert [key(p) for p in local]==[key(p) for p in remote]
   assert [p[0] for p in remote]==[k for k,h,i in events]
   timing=[p for p in remote if p[0]==0x30];assert len(timing)==1 and timing[0][7:9]==bytes([9,2])
   assert all(p[2]==0 for p in remote) and ns['n'].state(m).networkHouseDropped==0
   assert hits['sub_64B660_GetEventSize'] and (hits['sub_64BDD0_GetEventSize1'] or hits['sub_64BDD0_GetEventSize2'])
   rows.append({'session':mode,'wire_protocol':2,'network_house_preserve':adapted,'mixed_native_mega_and_timing':mixed,
    'received_kinds':[p[0] for p in remote],'timing_payload':[9,2],'event_bytes':111,'ordinary_size_export_calls':hits,'status':'PASS'})
out={'scope':__doc__,'status':'PASS','preflight':preflight,'ordinary_dll_sha256':hashlib.sha256(ordinary.read_bytes()).hexdigest(),
 'GameRuntime.dll_sha256':hashlib.sha256(Path(ns['a'].dll).read_bytes()).hexdigest(),
 'game_sha256':hashlib.sha256(Path(ns['a'].game).read_bytes()).hexdigest(),
 'full_spawner_initialized':False,'real_transport':False,'results':rows}
Path(ns['a'].output).write_text(json.dumps(out,indent=2)+'\n')
print('PASS',len(rows),'ordinary actual event-size exports + current sender timing roundtrips')
