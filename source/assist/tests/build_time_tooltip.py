"""Execute the production tooltip entry with native-derived timing results.

Requires a private supported PE, pefile, Unicorn and g++. No PE bytes are
generated into the fixture, receipt, repository or release.
"""
import argparse, hashlib, importlib.util, json, re, subprocess
from pathlib import Path

p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('--out',required=True,type=Path);p.add_argument('--sanitize',action='store_true');a=p.parse_args()
root=Path(__file__).resolve().parents[3];out=a.out.resolve();out.mkdir(parents=True,exist_ok=True)
paths=['source/assist/source/build_time_tooltip.h','source/assist/source/build_time_types.h','source/assist/source/build_time_rate.h','source/assist/source/bridge.cpp',
 'source/assist/source/guard.cpp','source/game_runtime/source/native_tooltip_runtime.h','source/shared/source/runtime_fp_scope.h',
 'source/shared/source/debug_entry_checks.h','source/shared/source/debug_entry_policy.h','source/shared/source/debug_entry_catalog.h',
 'source/assist/tests/build_time_tooltip.cpp','source/assist/tests/build_time_tooltip.py','source/assist/tests/build_time_native.py']
hashes={name:hashlib.sha256((root/name).read_bytes()).hexdigest() for name in paths}
module=Path(__file__).with_name('build_time_native.py');spec=importlib.util.spec_from_file_location('build_time_native',module);native=importlib.util.module_from_spec(spec);spec.loader.exec_module(native)
native_report=native.Native(a.game).evidence();(out/'native-result.json').write_text(json.dumps(native_report,indent=2)+'\n')
info=(root/'source/assist/source/build_time_types.h').read_text()
rows=re.findall(r'\{(\d+),(\d+),(\d+),(0x[0-9A-F]+),(0x[0-9A-F]+),(0x[0-9A-F]+),(0x[0-9A-F]+),(0x[0-9A-F]+),(0x[0-9A-F]+),(0x[0-9A-F]+)\}',info)
assert len(rows)==4
for row,k in zip(rows,native.KINDS):assert list(map(lambda x:int(x,0),row))==[k[x] for x in ['cameo','kind','abstract','array','vt','tvt','offset','getter','what','twhat']]
def function(text,start):
 begin=text.index(start);brace=text.index('{',begin);end=brace+1;depth=1
 while depth:
  if text[end]=='{':depth+=1
  elif text[end]=='}':depth-=1
  end+=1
 return text[begin:end]
bridge=(root/'source/assist/source/bridge.cpp').read_text()
helpers=function(bridge,'struct Vec {')+';\n'+'\n'.join(function(bridge,start) for start in ['static Vec vec(','static uint32_t item(','static int kind('])
(out/'production-bridge-helpers.inc').write_text(helpers+'\n')
header=root/'source/assist/source/build_time_tooltip.h';text=header.read_text()
omitted='#include "../../shared/source/runtime_fp_scope.h"';assert text.count(omitted)==1
text=text.replace(omitted,'// Unused MSVC-only FP scope include omitted by the Linux entry fixture. The outer runtime/SEH owners are unchanged.')
text=re.sub(r'#include "([^\"]+)"',lambda m:'#include '+json.dumps(str((header.parent/m[1]).resolve())),text)
(out/'production-tooltip.inc').write_text(text)
kindmap={k['name']:k for k in native.KINDS};lines=[]
for case in native_report['cases']:
 k=kindmap[case['name'].split('_')[0]];tab=1 if case['name']=='building_defense' else {7:0,40:3,16:2,3:3}[k['cameo']]
 lines.append(' {%d,%d,%d,%d,%s},'%(k['cameo'],tab,case['raw'],case['frames'],json.dumps(case['name'])))
(out/'native-times.inc').write_text('static const NativeOutput NativeOutputs[]={\n'+'\n'.join(lines)+'\n};\n')
binary=out/'build-time-tooltip';cmd=['g++','-std=c++17','-O2','-UNDEBUG','-fshort-wchar','-no-pie','-I',str(out),str(root/'source/assist/tests/build_time_tooltip.cpp'),'-o',str(binary)]
if a.sanitize:cmd[2:2]=['-fsanitize=undefined','-fno-sanitize-recover=undefined','-fno-omit-frame-pointer']
commands=[]
for command in [cmd,[str(binary),str(a.game.resolve())]]:
 result=subprocess.run(command,text=True,capture_output=True);commands.append(dict(command=command,exit_code=result.returncode,stdout=result.stdout,stderr=result.stderr));print(result.stdout,result.stderr,flush=True)
 if result.returncode:break
stable=all(hashlib.sha256((root/name).read_bytes()).hexdigest()==value for name,value in hashes.items())
entry=json.loads(commands[-1]['stdout'].strip().splitlines()[-1]) if len(commands)==2 and commands[-1]['exit_code']==0 else None
receipt=dict(version='1.0.46 revision',passed=stable and native_report['passed'] and entry is not None and entry['passed'],source_sha256=hashes,source_hash_stable=stable,
 executable_sha256=native.SHA,native_case_count=len(native_report['cases']),native_service_seams=[],entry=entry,sanitizer='UBSan' if a.sanitize else None,commands=commands,
 entry_service_seams=['synthetic process-memory image loaded from the private PE, with synthetic sidebar/House/Type/factory state','Win32 runtime ownership and clock','native thiscall receives and validates the real production scratch receiver; return values come from the complete original-instruction executions in native-result.json','unused MSVC-only FP include omitted from Linux translation; native_tooltip_runtime + guard + FP owner source-bound and unchanged, not executed here'],
 boundaries=['No Windows/game/visual tooltip or multiplayer run.','The retained +1 is the existing ready-display convention, not a proof of an extra native frame in every call order.','Count 75 is accepted by Assist when original text exists; native Strip.Help rejects >=75 and the shared hook still retains null.','Unknown receiver/Type vtables, methods and changed native TimeToBuild bodies return original text; no compatibility override is introduced.'])
(out/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n');raise SystemExit(not receipt['passed'])
