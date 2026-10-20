"""Compare supplied PE32 executables without executing or modifying either file.
Usage: PYTHONPATH=<pefile location> python compare_game_builds.py sample3 sample4 --output report.json
"""
import argparse,hashlib,json
from pathlib import Path
import pefile
p=argparse.ArgumentParser();p.add_argument('sample3');p.add_argument('sample4');p.add_argument('--reference');p.add_argument('--output',required=True);a=p.parse_args()
paths=[Path(a.sample3),Path(a.sample4)];images=[pefile.PE(str(f)) for f in paths]
def digest(f):return hashlib.sha256(f.read_bytes()).hexdigest()
rows=[]
for name,start,size in [('Object_constructor_window',0x4C65E0,0x70),('MegaMission_constructor_window',0x4C6860,0x110),('Produce_constructor_window',0x4C6970,0x90),('SuperWeapon_constructor_window',0x4C6B60,0x80),('CanBuild_window',0x4F7870,0x540),('Aircraft_move_window',0x416800,0x400),('TryNextPlanningTokenNode',0x6385C0,0x114),('ProcessClickCoords_window',0x692300,0x900),('Frame_increment_window',0x55DE73,12),('Event_execution_branch',0x4C794B,9),('Placement_patch',0x4A8EB0,5),('Aircraft_type_patch',0x7E2908,4),('Aircraft_arrival_patch',0x416AB6,10),('Shroud_draw_patch',0x47F038,5),('Fog_draw_patch',0x47F274,5),('CRC_mismatch_branch',0x64C6E9,15),('Shroud_check_mode',0x5C0E30,3),('ProtocolZero_queue_site',0x647BEB,9),('ProtocolZero_send_rate_site',0x647EB4,10),('ProtocolZero_late_event_site',0x64C598,6),('C4_patch',0x51A4D8,6),('Tech_draw_patch',0x6A97D2,5),('Tech_prune_patch',0x6AA781,5)]:
 data=[im.get_data(start-im.OPTIONAL_HEADER.ImageBase,size) for im in images]
 rows.append({'name':name,'address':hex(start),'length':size,'identical':data[0]==data[1],'sha256':[hashlib.sha256(d).hexdigest() for d in data],**({'bytes':[d.hex() for d in data]} if size<=16 else {})})
text=[im.get_data(0x1000,0x3DF000) for im in images];different=sum(x!=y for x,y in zip(*text))
report={'scope':'Static disk image comparison; matching windows do not prove calling-chain/runtime-DLL compatibility or real netplay. Platform labels supplied by the user.','samples':[{'label':label,'file':str(f.name),'sha256':digest(f),'bytes':f.stat().st_size,'timestamp':im.FILE_HEADER.TimeDateStamp,'image_base':hex(im.OPTIONAL_HEADER.ImageBase),'image_size':hex(im.OPTIONAL_HEADER.SizeOfImage),'imports':[i.dll.decode() for i in im.DIRECTORY_ENTRY_IMPORT]} for f,im,label in zip(paths,images,['ZhanWang (3)','LBW (4)'])],'text_window':{'rva':'0x1000','length':0x3DF000,'different_bytes':different},'windows':rows}
if a.reference:report['previous_reference']={'sha256':digest(Path(a.reference)),'matches_sample3':digest(Path(a.reference))==digest(paths[0]),'matches_sample4':digest(Path(a.reference))==digest(paths[1])}
Path(a.output).write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n');print('PASS static comparison:',different,'different bytes; identical windows:',sum(r['identical'] for r in rows),'/',len(rows))
