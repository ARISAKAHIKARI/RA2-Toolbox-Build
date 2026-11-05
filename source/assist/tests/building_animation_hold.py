"""Source-bound native draw eligibility + current/old-guard capture regression.

The private executable is required explicitly and checked by the native probe.
The baseline header must be the frozen 513aeb4 source file, not a reconstructed
approximation. No game is started; failed negative assertions return 31 instead
of aborting, so the regression does not invoke Windows Error Reporting.
"""
from pathlib import Path
import argparse, hashlib, json, os, shutil, subprocess, sys, time

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--compiler',default=os.environ.get('CXX','g++'))
p.add_argument('--compiler-arg',action='append',default=[])
p.add_argument('--baseline-header',type=Path,required=True)
p.add_argument('--exe',type=Path,required=True)
p.add_argument('--unicorn-path',type=Path)
p.add_argument('--output',type=Path,required=True)
a=p.parse_args()
here=Path(__file__).resolve().parent;source=here.parents[1];headers=source/'external_reader/source'
out=a.output.resolve().parent;out.mkdir(parents=True,exist_ok=True)
work=out/'building-animation-hold';work.mkdir(exist_ok=True)
baseline=work/'baseline';baseline.mkdir(exist_ok=True)
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
expected='b895964cfcbb3c1f9f80bd10052e6fbb56ddf58c5bf49fb5807b5762a2d48ab7'
if sha(a.baseline_header)!=expected:raise SystemExit('baseline header is not the frozen 513aeb4 source')
shutil.copyfile(a.baseline_header,baseline/'building_animations.h')
files=sorted([*headers.glob('*.h'),*(source/'shared/source').glob('*.h'),here/'building_animations.cpp',here/'building_animation_draw_native.py',Path(__file__).resolve()])
def manifest():return {path.relative_to(source).as_posix():sha(path) for path in files}
before=manifest()
test=(here/'building_animations.cpp').read_text(encoding='utf-8-sig').replace('#include "../../external_reader/source/','#include "')
test=test.replace('#include <cassert>','#include <cassert>\n#undef assert\n#define assert(...) do { if(!(__VA_ARGS__)) { std::cerr<<"assertion failed at line "<<__LINE__<<": "<<#__VA_ARGS__<<"\\n"; return 31; } } while(0)')
(work/'capture.cpp').write_text(test,encoding='utf-8',newline='\n')
compiler=[a.compiler]
if Path(a.compiler).name.lower() in ['zig','zig.exe']:compiler+=['c++']
compiler+=a.compiler_arg
results=[]
for name,include,expected_exit in [('current',headers,0),('old_guard',baseline,31)]:
    exe=work/(name+('.exe' if os.name=='nt' else ''))
    command=compiler+['-std=c++17','-O2','-UNDEBUG','-I'+str(include),'-I'+str(headers),str(work/'capture.cpp'),'-o',str(exe)]
    started=time.monotonic();build=subprocess.run(command,text=True,capture_output=True)
    (work/(name+'-build.log')).write_text(build.stdout+build.stderr,encoding='utf-8')
    row={'name':name,'build_exit':build.returncode,'build_seconds':time.monotonic()-started,'expected_exit':expected_exit};results.append(row)
    if not build.returncode:
        run=subprocess.run([str(exe)],text=True,capture_output=True,timeout=60)
        row.update(exit=run.returncode,output=run.stdout+run.stderr,binary_sha256=sha(exe))
        row['passed']=run.returncode==expected_exit and (name!='old_guard' or 'states.size()==1&&states[0].frame==39' in run.stderr)
    else:row['passed']=False
native_output=work/'native.json'
command=[sys.executable,str(here/'building_animation_draw_native.py'),'--exe',str(a.exe),'--output',str(native_output)]
if a.unicorn_path:command+=['--unicorn-path',str(a.unicorn_path)]
native=subprocess.run(command,text=True,capture_output=True,timeout=60)
native_report=json.loads(native_output.read_text()) if native.returncode==0 and native_output.exists() else {}
after=manifest()
report={'passed':all(r['passed'] for r in results) and native.returncode==0 and native_report.get('passed') is True and native_report.get('native_cases')==38 and before==after,
        'baseline_commit':'513aeb4cf03238cce956bd35e0c9381c2a7cc484','baseline_header_sha256':expected,
        'source_hash_stable':before==after,'source_sha256':before,'generated_test_sha256':sha(work/'capture.cpp'),
        'results':results,'native_exit':native.returncode,'native':native_report,
        'game_started':False,'game_visual_tested':False,'hardware_gpu_tested':False}
if native.returncode:report['native_error']=native.stdout+native.stderr
a.output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
print(json.dumps({'passed':report['passed'],'native_cases':native_report.get('native_cases'),'source_hash_stable':before==after,'cases':[{k:r.get(k) for k in ['name','build_exit','exit','passed']} for r in results]}))
raise SystemExit(not report['passed'])
