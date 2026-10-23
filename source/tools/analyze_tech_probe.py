#!/usr/bin/env python3
"""Summarize a local RA2Master off/on/off capture; never exports memory dumps."""
import argparse,json,hashlib
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('capture',type=Path);p.add_argument('--output',type=Path,required=True);p.add_argument('--archive',type=Path);a=p.parse_args()
root=a.capture
stages={n:(root/(n+'-game.mem')).read_bytes() for n in ['00-clean','01-off','02-on','03-off']}
valid={n:(root/(n+'-game.valid')).read_bytes() for n in stages};stable={n:(root/(n+'-game.stable')).read_bytes() for n in stages}
for n,b in stages.items():assert len(b)==len(valid[n])==len(stable[n])
reversible=json.loads((root/'04-reversible.json').read_text());code=json.loads((root/'02-on-code.json').read_text());startup=json.loads((root/'01-trainer-start-code.json').read_text());session=json.loads((root/'session.json').read_text())
rows=[];categories={};verified=0
for change in reversible['changes']:
 at=int(change['rva'],16);size=change['length'];off,on,end=(stages[n][at:at+size] for n in ['01-off','02-on','03-off'])
 assert off.hex().upper()==change['before'] and on.hex().upper()==change['enabled'] and end.hex().upper()==change['disabled_again']
 assert off==end and all(x!=y for x,y in zip(off,on))
 for n in ['01-off','02-on','03-off']:assert all(valid[n][at:at+size]) and all(stable[n][at:at+size])
 verified+=size;va=at+0x400000
 category='executable' if change['executable'] else 'sidebar_and_display' if 0x87F7E8<=va<0x883000 else 'other_static_data'
 categories[category]=categories.get(category,0)+size
 if change['executable']:rows.append({k:change[k] for k in ['va','length','before','enabled','disabled_again']})
assert verified==315 and categories['executable']==83
patches=[]
for va,n,label in [(0x44574A,21,'building enumeration: CanBuild -> TechLevel != -1'),(0x445795,15,'vehicle enumeration: CanBuild -> TechLevel != -1'),(0x4457D0,21,'infantry enumeration: CanBuild -> TechLevel != -1'),(0x445810,21,'aircraft enumeration: CanBuild -> TechLevel != -1'),(0x6AA788,6,'sidebar prune conditional JNE -> unconditional JMP')]:
 at=va-0x400000;off=stages['01-off'][at:at+n];on=stages['02-on'][at:at+n];end=stages['03-off'][at:at+n];assert off==end
 patches.append({'va':hex(va),'length':n,'meaning':label,'before':off.hex(),'enabled':on.hex(),'restored':True})
result={'archive_sha256':hashlib.sha256(a.archive.read_bytes()).hexdigest() if a.archive else None,'sample_sha256':session['file_sha256'].lower(),'stage_image_sha256':{n:hashlib.sha256(b).hexdigest() for n,b in stages.items()},'verified_reversible_bytes':verified,'categories':categories,'logical_code_patches':patches,'startup_changes':[{'va':c['va'],'length':c['length']} for c in startup['changes']],'private_regions':{n:len(json.loads((root/(n+'-private-regions.json')).read_text())) for n in stages},'scope':'Observed reversible module bytes, with validity/stability masks checked. Not writer attribution, heap/type-rule inspection, full production execution or multiplayer evidence.'}
a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n');print(json.dumps({'verified_bytes':verified,'categories':categories,'logical_patches':len(patches)}))
