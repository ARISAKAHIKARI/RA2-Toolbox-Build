from pathlib import Path
import hashlib,json,os,subprocess
repo=Path('/workspace/scratch/40121bc881b8/ra2-audit-followup')
root=Path(__file__).resolve().parent
out=root/'selected-benchmark';out.mkdir(exist_ok=True)
source=root/'capture_benchmark.cpp';bridge=repo/'source/assist/source/bridge.cpp'
text=bridge.read_text()
def extract(name):
 start=text.index('extern "C" void* __cdecl '+name+'(');body=text.index('{',start);depth=1;end=body+1
 while depth:
  depth+=(text[end]=='{')-(text[end]=='}');end+=1
 return text[start:body].replace('__cdecl ','')+'noexcept'+text[body:end]
crt=out/'production-crt.h'
crt.write_text('#include <cstring>\n#include <cstdint>\n#include "'+str(repo/'source/shared/source/runtime_memory.h')+'"\n'+extract('memset')+'\n'+extract('memcpy')+'\n')
command=['g++','-std=c++17','-O2','-fno-exceptions','-fno-builtin','-DREUSE_CAPTURE_SCRATCH','-DCAPTURE_HEADER="'+str(repo/'source/shared/source/world_frame_capture.h')+'"','-DCRT_HEADER="'+str(crt)+'"',str(source),'-o',str(out/'capture-selected')]
cpu=max(os.sched_getaffinity(0));os.sched_setaffinity(0,{cpu})
build=subprocess.run(command,capture_output=True,text=True);(out/'build.log').write_text(build.stdout+build.stderr)
if build.returncode:raise SystemExit(build.returncode)
run=subprocess.run([str(out/'capture-selected')],capture_output=True,text=True);(out/'run.jsonl').write_text(run.stdout);(out/'stderr.log').write_text(run.stderr)
if run.returncode:raise SystemExit(run.returncode)
rows=[json.loads(line) for line in run.stdout.splitlines()]
baseline=[json.loads(line) for line in (root/'benchmark/baseline-old-crt.jsonl').read_text().splitlines()]
for before,after in zip(baseline,rows):
 for field in ['case','actors','demand','requested_extra_pages','pages','read_calls_per_frame','read_bytes_per_frame','rosters','payload_fnv64']:assert before[field]==after[field],(field,before,after)
 assert after['heap_allocations_per_frame']==after['heap_frees_per_frame']==0
bindings=[]
for rel in ['source/shared/source/world_frame.h','source/shared/source/world_frame_capture.h','source/assist/source/world_frame_runtime.h','source/shared/source/runtime_memory.h','source/assist/source/bridge.cpp']:
 bindings.append({'path':rel,'sha256':hashlib.sha256((repo/rel).read_bytes()).hexdigest()})
for path in [Path(__file__),source,crt]:bindings.append({'path':str(path),'sha256':hashlib.sha256(path.read_bytes()).hexdigest()})
receipt={'passed':True,'selected_for_release':True,'batching_shipped':False,'build_command':command,'baseline_receipt':'../benchmark/receipt.json','baseline_results':baseline,'results':rows,'cpu_affinity':[cpu],'source_bindings':bindings,'windows_executed':False,'game_tested':False,'limits':['Exact production capture headers and CRT wrappers; only __cdecl/noexcept linkage adapted to Linux.','Same5 synthetic fully readable worlds,1warm+24measured frames, -fno-builtin. No native Windows RPM timing or game FPS.','Final payload hashes, read calls/bytes, page and roster counts match frozen1.0.45 baseline.','Elapsed time excludes fixture setup/hash. C++ new/delete and Snapshot/CaptureScratch PageAllocator allocations counted.']}
(out/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n');print(run.stdout,end='')
