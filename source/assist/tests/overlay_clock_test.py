"""Execute actual compositor configuration and frame-clock with OS boundaries controlled."""
from pathlib import Path
import argparse,hashlib,json,subprocess
p=argparse.ArgumentParser();p.add_argument('--out',type=Path,required=True);p.add_argument('--baseline',required=True);a=p.parse_args()
repo=Path(__file__).resolve().parents[3];out=a.out.resolve();out.mkdir(parents=True,exist_ok=True)
paths=['source/shared/source/overlay_compositor.h','source/shared/source/deadline_wake.h','source/shared/source/timer_resolution.h','source/assist/source/frame_clock.h','source/assist/source/pacing.h','source/cursor/source/main.cpp',str(Path(__file__).resolve().relative_to(repo)),'source/assist/tests/overlay_clock_wait.cpp','source/assist/tests/overlay_clock_runtime.cpp']
before={n:hashlib.sha256((repo/n).read_bytes()).hexdigest() for n in paths};results=[]
def execute(name,command,expected=0):
 build=subprocess.run(command,capture_output=True,text=True);(out/(name+'-build.log')).write_text(build.stdout+build.stderr);build.check_returncode()
 run=subprocess.run([command[-1]],capture_output=True,text=True,timeout=30);(out/(name+'-run.log')).write_text(run.stdout+run.stderr)
 assert run.returncode==expected,(name,run.returncode,run.stdout,run.stderr)
 results.append(dict(name=name,command=command,compile_exit=build.returncode,run_exit=run.returncode,expected_exit=expected));print(name,run.returncode,run.stdout,end='\n')
flags=['g++','-std=c++17','-O2','-pthread','-fsanitize=undefined','-fno-sanitize-recover=all']
for label in ('baseline','current'):
 raw=(repo/paths[0]).read_text() if label=='current' else subprocess.check_output(['git','show',a.baseline+':'+paths[0]],cwd=repo,text=True)
 start=raw.index(' Context configure(');end=raw.index('\nprivate:',start);method=raw[start:end]
 fixture='''#include <mutex>
#include <thread>
#include <cassert>
#include <cstdio>
#include <cstdint>
using std::uint64_t;using std::uint32_t;using std::uintptr_t;
static uint64_t tick=1000;uint64_t GetTickCount64(){return tick;}
struct Context{uint64_t token=0;uintptr_t window=0;uint32_t pid=0,match=0;int width=0,height=0,cameraX=0,cameraY=0;uint64_t projection=0;};
struct Host{
std::mutex mutex;struct{void clear(){}}nodes;struct{unsigned notices=0;void notify_one(){++notices;}}cv;
std::thread thread;uint64_t revision=0,generation=0,heartbeat=0;bool enabled=false,stop=false,top=false,uploadContinuous_=false;int rate=60;Context context;struct{}uploadStatistics_;
void run(){}~Host(){if(thread.joinable())thread.join();}
'''+method+'''\n};
int main(){Host h;auto initial=h.configure(1,2,3,100,100,0,0,true,false,170);auto initialNotices=h.cv.notices;
for(unsigned n=0;n<2000;++n){++tick;auto c=h.configure(1,2,3,100,100,0,0,true,false,170);assert(c.token==initial.token);}
const auto renewals=h.cv.notices-initialNotices;
'''+('assert(renewals==0);' if label=='current' else 'assert(renewals==2000);')+'''
auto prior=h.revision;h.configure(1,2,3,100,100,0,0,true,false,240);
'''+('assert(h.revision>prior);prior=h.revision;tick+=501;h.configure(1,2,3,100,100,0,0,true,false,240);assert(h.revision>prior);' if label=='current' else '')+'''
prior=h.revision;assert(!h.configure(1,2,3,100,100,0,0,false,false,240).token);assert(h.revision>prior);
assert(h.configure(1,2,3,100,100,0,0,true,false,240).token!=initial.token);
std::printf("PASS configure: unchanged renewals=2000 notices=%u; activation/context/rate and stale heartbeat\\n",renewals);}
'''
 source=out/(label+'-configure.cpp');source.write_text(fixture);execute(label+'-configure',flags+[str(source),'-o',str(out/(label+'-configure'))])
 # Keep the real old frame-clock function as a fault-injection counterexample.
 clock=out/(label+'-clock.h');clock.write_text((repo/paths[3]).read_text() if label=='current' else subprocess.check_output(['git','show',a.baseline+':'+paths[3]],cwd=repo,text=True))
 execute(label+'-runtime',flags+(['-DEXPECT_IDLE_CLOCK'] if label=='current' else [])+['-I'+str(repo/'source/assist/source'),'-DFRAME_CLOCK_HEADER="'+str(clock)+'"',str(repo/'source/assist/tests/overlay_clock_runtime.cpp'),'-o',str(out/(label+'-runtime'))],0 if label=='current' else -6)
execute('wait',flags+[str(repo/'source/assist/tests/overlay_clock_wait.cpp'),'-o',str(out/'wait')])
assert all(hashlib.sha256((repo/n).read_bytes()).hexdigest()==sha for n,sha in before.items())
(out/'receipt.json').write_text(json.dumps(dict(passed=True,source_inputs=before,source_unchanged=True,baseline=a.baseline,results=results,windows_executed=False,game_tested=False,scope='Verbatim production configure() and frameClock; controlled Win32 calls. Real std::threads for DeadlineWake, portable fallback only; Windows high-resolution timing not measured.'),indent=2)+'\n')
