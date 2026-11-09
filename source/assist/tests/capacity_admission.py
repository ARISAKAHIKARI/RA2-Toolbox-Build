"""Execute current unload admission and variant-selection functions on controlled game IO.
No native Windows or game code executes. Queue/roster/selection functions are extracted verbatim.
"""
import argparse,hashlib,json,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--out',type=Path,required=True);p.add_argument('--baseline',type=Path);p.add_argument('--case',default='all');a=p.parse_args()
root=Path(__file__).resolve().parents[3];src=root/'source/assist/source';tests=Path(__file__).resolve().parent;a.out.mkdir(parents=True,exist_ok=True)
def block(text,signature):
 start=text.index(signature);at=text.index('{',start);depth=1;end=at+1
 while depth:depth+=(text[end]=='{')-(text[end]=='}');end+=1
 return text[start:end]
paths=[src/n for n in ('native_orders_bridge.h','native_orders.h','unload_policy.h','unload_admission.h','object_index.h','type_select_bridge.h')]+[tests/'capacity_admission.cpp',Path(__file__).resolve()]
hashes={str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}
text=(src/'native_orders_bridge.h').read_text();unload=text[text.index('// Build one carrier-keyed roster'):text.index('static void watchEjections(')]
variant=(src/'type_select_bridge.h').read_text();variant='#include <unordered_set>\n'+block(variant,'static uint32_t normalVariantWeapon(')+'\n'+block(variant,'static int selectVariant(')
if a.baseline:
 old=(a.baseline/'native_orders_bridge.h').read_text();unload=unload.replace(block(unload,'static int smartUnload('),block(old,'static int smartUnload('))
 old=(a.baseline/'type_select_bridge.h').read_text();variant=block(old,'static uint32_t normalVariantWeapon(')+'\n'+block(old,'static int selectVariant(')
(a.out/'unload.inc').write_text(unload);(a.out/'variant.inc').write_text(variant)
commands=[['g++','-std=c++17','-O2','-UNDEBUG','-DCAPACITY_UNLOAD_FUNCTIONS="'+str(a.out/'unload.inc')+'"','-DCAPACITY_VARIANT_FUNCTIONS="'+str(a.out/'variant.inc')+'"',str(tests/'capacity_admission.cpp'),'-o',str(a.out/'fixture')],[str(a.out/'fixture'),a.case]];runs=[]
for i,cmd in enumerate(commands):
 run=subprocess.run(cmd,capture_output=True,text=True,timeout=90);(a.out/('build.log' if i==0 else 'run.log')).write_text(run.stdout+run.stderr);runs.append(dict(command=cmd,exit_code=run.returncode));print(run.stdout+run.stderr,end='')
 if run.returncode:break
stable=all(hashlib.sha256((root/p).read_bytes()).hexdigest()==h for p,h in hashes.items());passed=len(runs)==2 and all(r['exit_code']==0 for r in runs) and stable
(a.out/'result.json').write_text(json.dumps(dict(scope=__doc__,passed=passed,source_sha256=hashes,sources_stable=stable,runs=runs,baseline=str(a.baseline) if a.baseline else None),indent=2)+'\n');raise SystemExit(not passed)
