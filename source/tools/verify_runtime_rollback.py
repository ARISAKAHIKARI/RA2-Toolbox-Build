"""Verify 0.7.14 Runtime source and executable sections match supplied 0.7.11.
Version resources/build IDs are intentionally different; no gameplay claim.
"""
from pathlib import Path
import argparse,hashlib,json
from pe32_image import Image
p=argparse.ArgumentParser();p.add_argument('--baseline',type=Path,required=True)
p.add_argument('--output',type=Path,required=True);a=p.parse_args()
root=Path(__file__).resolve().parent.parent
sha=lambda b:hashlib.sha256(b).hexdigest()
audit=json.loads((root/'ARCHITECTURE_AUDIT.json').read_text())
sources={}
for name in audit['runtime_sources']:
 current=(root/name).read_bytes();old=(a.baseline/name).read_bytes()
 assert current==old, 'Runtime source differs from 0.7.11: '+name
 sources[name]=sha(current)
current=Image((root/'GameRuntime.dll').read_bytes())
old=Image((a.baseline/'GameRuntime.dll').read_bytes())
assert current.entry==old.entry
sections={}
for label,rva,size,offset,virtual_size in current.sections:
 if label in ('.rsrc','.buildid'):continue
 prior=next(s for s in old.sections if s[0]==label)
 assert (rva,size,virtual_size)==(prior[1],prior[2],prior[4]),label
 raw=current.take(offset,size);before=old.take(prior[3],prior[2])
 assert raw==before,'Runtime section differs from 0.7.11: '+label
 sections[label]={'bytes':size,'rva':rva,'sha256':sha(raw),'identical':True}
for name in audit['runtime_sources']:
 s=(root/name).read_text()
 assert 'NetEvidenceCapture' not in s and 'netEvidenceTick' not in s and 'recordNetPacket' not in s
result={'version':'0.7.14','baseline':'0.7.11','inputs':{
 'GameRuntime.dll':sha((root/'GameRuntime.dll').read_bytes()),
 'RA2Toolbox.exe':sha((root/'RA2Toolbox.exe').read_bytes()),
 'baseline_dll':sha((a.baseline/'GameRuntime.dll').read_bytes())},
 'runtime_source_files_identical':len(sources),'runtime_source_sha256':sources,
 'sections_identical':sections,'excluded_sections':['.rsrc (version)','.buildid (build identity)'],
 'removed_diagnostic_sites':['0x64CC61','0x64C5AF'],
 'abandoned_unreleased_site':'0x64C6E9',
 'runtime_protocol':3,'assist_protocol':34,'entry_protocol':3,
 'real_platform_disconnect_cause_confirmed':False,'real_netplay_tested':False}
a.output.write_text(json.dumps(result,indent=2)+'\n')
print('PASS',len(sources),'Runtime source files and',len(sections),'non-resource/build-id sections identical to 0.7.11')
