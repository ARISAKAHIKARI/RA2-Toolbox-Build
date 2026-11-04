"""Current released DLL sender thunks against a second native sender and third decoder.

Synthetic Win32/world. Executes real released NetworkHousePreserve machine code,
original constructors, sender, compressor and decoder. Does not map the Spawner
module, run sockets, dispatch missions or prove actual multiplayer OOS.
"""
import argparse,hashlib,json,sys
from pathlib import Path

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--game',type=Path,required=True,help='Authorized supported PE32 engine fixture')
parser.add_argument('--runtime',type=Path,required=True,help='Exact released GameRuntime.dll')
parser.add_argument('--repo',type=Path,required=True,help='RA2-Toolbox-Build source checkout')
parser.add_argument('--output',type=Path,required=True)
options=parser.parse_args()
repo=options.repo.resolve()
options.output.parent.mkdir(parents=True,exist_ok=True)

# These cases require the original native sender continuation. Do not repair a
# modified/private engine fixture merely to make a network test complete.
import pefile
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
  'preflight':preflight,'planned_cases':4,'results':[]}
 options.output.write_text(json.dumps(out,indent=2)+'\n')
 print('BLOCKED: engine fixture native continuation differs; see',options.output)
 sys.exit(2)

fixture=repo/'source/assist/tests/network_house_runtime.py'
ns={'__name__':'network_cache_fixture','__file__':str(fixture)}
original_argv=sys.argv
sys.argv=[str(fixture),str(options.game),str(options.runtime),'--output',str(options.output)]
try:
 exec(compile(fixture.read_text().split('\nMIXED=',1)[0],str(fixture),'exec'),ns)
finally:
 sys.argv=original_argv
setup,toggle,fill,send,decode=(ns[x] for x in ('setup','toggle','fill','send','decode'))

def ids(records):return [int.from_bytes(x[7:11],'little') for x in records if x[0]==4]
def receive(m,packets):
 m.sent=list(packets)
 return ids(decode(m))

rows=[]
for protocol in (1,2):
 for intervening in (False,True):
  modified=setup(3,protocol);assert toggle(modified,True).networkHouseStatus==1
  native=setup(3,protocol);native.w(0xa83d4c,native.enemy)
  receiver=setup(3,protocol)
  fill(native,[(4,1,201),(4,1,202)]);native.w(0xa8ed84,1234)
  count,first_local=send(native);first=list(native.sent)
  assert ids(first_local)==[201,202] and receive(receiver,first)==[201,202]
  foreign=[]
  if intervening:
   fill(modified,[(4,1,101),(4,1,102)]);modified.w(0xa8ed84,1234)
   count,foreign_local=send(modified);foreign=list(modified.sent)
   assert ids(foreign_local)==[101,102] and receive(receiver,foreign)==[101,102]
  fill(native,[(4,1,201),(4,1,202)]);native.w(0xa8ed84,1237)
  count,last_local=send(native);last=list(native.sent);decoded=receive(receiver,last)
  assert ids(last_local)==[201,202]
  assert decoded==([101,102] if intervening else [201,202]),decoded
  assert len(last[0])<len(first[0]),'native sender must reuse its cached source group'
  rows.append({'protocol':protocol,'foreign_sender_intervenes':intervening,
   'network_house_preserve':True,'actual_modified_sender_sites':len(ns['SITES']),
   'first_native_packet_lengths':[len(p) for p in first],
   'foreign_packet_lengths':[len(p) for p in foreign],
   'cached_native_packet_lengths':[len(p) for p in last],
   'native_sender_local_source_ids':ids(last_local),'third_receiver_source_ids':decoded,
   'source_substitution':ids(last_local)!=decoded,'status':'PASS'})
out={'scope':__doc__,'status':'PASS','preflight':preflight,'game_sha256':hashlib.sha256(Path(ns['a'].game).read_bytes()).hexdigest(),
 'GameRuntime.dll_sha256':hashlib.sha256(Path(ns['a'].dll).read_bytes()).hexdigest(),
 'spawner_module_mapped':False,'real_network':False,'mission_execution':False,
 'results':rows}
Path(ns['a'].output).write_text(json.dumps(out,indent=2)+'\n')
print('PASS',len(rows),'actual released DLL cross-sender cache cases')
