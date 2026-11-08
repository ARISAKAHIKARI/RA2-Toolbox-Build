from pathlib import Path
import hashlib,json,os,subprocess
repo=Path('/workspace/scratch/40121bc881b8/ra2-audit-followup')
root=Path(__file__).resolve().parent
out=root/'benchmark'
variant=out/'no-batch'
variant.mkdir(exist_ok=True)
original=(repo/'source/shared/source/world_frame_capture.h').read_text()
subject=original.replace('frame_.prefetchBatched(live_,p,n,scratch_->bytes.data(),scratch_->bytes.size())','frame_.prefetch(live_,p,n)')
start=subject.index('  // The initial actor roster is already known')
end=subject.index('  for(size_t i=0;i<scratch_->queue.size();++i)',start)
subject=subject[:start]+subject[end:]
start=subject.index('  for(size_t first=0;first<requests.size();)')
end=subject.index('  return working();',start)
subject=subject[:start]+'  for(uint32_t page:requests){if(!working())return false;block(page,PageSize);}\n'+subject[end:]
(variant/'world_frame_capture.h').write_text(subject)
(variant/'world_frame.h').write_bytes((repo/'source/shared/source/world_frame.h').read_bytes())
command=['g++','-std=c++17','-O2','-fno-exceptions','-fno-builtin','-DREUSE_CAPTURE_SCRATCH','-DCAPTURE_HEADER="'+str(variant/'world_frame_capture.h')+'"','-DCRT_HEADER="'+str(out/'new-crt.h')+'"',str(root/'capture_benchmark.cpp'),'-o',str(out/'subject-no-batch-new-crt')]
cpu=max(os.sched_getaffinity(0));os.sched_setaffinity(0,{cpu})
build=subprocess.run(command,capture_output=True,text=True)
(out/'subject-no-batch-build.log').write_text(build.stdout+build.stderr)
if build.returncode:raise SystemExit(build.returncode)
run=subprocess.run([str(out/'subject-no-batch-new-crt')],capture_output=True,text=True)
(out/'subject-no-batch-new-crt.jsonl').write_text(run.stdout)
(out/'subject-no-batch-stderr.log').write_text(run.stderr)
if run.returncode:raise SystemExit(run.returncode)
rows=[json.loads(line) for line in run.stdout.splitlines()]
baseline=[json.loads(line) for line in (out/'baseline-old-crt.jsonl').read_text().splitlines()]
for before,after in zip(baseline,rows):
 for field in ['case','actors','demand','requested_extra_pages','pages','read_calls_per_frame','read_bytes_per_frame','rosters','payload_fnv64']:assert before[field]==after[field],(field,before,after)
receipt={'passed':True,'build_command':command,'baseline_receipt':'receipt.json','results':rows,'cpu_affinity':[cpu],'source_bindings':[{'path':str(repo/'source/shared/source/world_frame_capture.h'),'sha256':hashlib.sha256(original.encode()).hexdigest()},{'path':str(variant/'world_frame_capture.h'),'sha256':hashlib.sha256(subject.encode()).hexdigest()}],'adaptations':['block uses original Snapshot::prefetch','actor page read-ahead worklist/sort/read removed','per-request PageSize loop restored'],'windows_executed':False,'game_tested':False,'limits':['Uses same 5 worlds, 24 measured frames, actual new CRT wrappers and -fno-builtin from parent benchmark; no Windows RPM or game FPS.','Only generated external capture header changes; production repo remains unchanged.']}
(out/'no-batch-receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
print(run.stdout,end='')
