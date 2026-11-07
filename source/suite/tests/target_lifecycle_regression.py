#!/usr/bin/env python3
"""Execute production target-switch and cleanup bodies with controlled OS data."""
import argparse
import hashlib
import json
import subprocess
from pathlib import Path


def block(text, marker):
    start = text.index(marker)
    at = text.index('{', start)
    depth, end = 1, at + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


parser = argparse.ArgumentParser()
parser.add_argument('--out', required=True)
parser.add_argument('--cxx', default='g++')
args = parser.parse_args()
here = Path(__file__).resolve().parent
root = here.parents[1]
paths = [root / path for path in ('cursor/source/main.cpp', 'unit_monitor/source/main.cpp',
                                'external_reader/source/process_target_selection.h')]
tracked = [*paths, Path(__file__).resolve()]
inputs = {path: path.read_bytes() for path in tracked}
before = {str(path.relative_to(root)): hashlib.sha256(data).hexdigest() for path, data in inputs.items()}
cursor, monitor, selection = (inputs[path].decode('utf-8-sig') for path in paths)
assert 'static void autoTick(){syncTargetSelection();DWORD now=GetTickCount();' in cursor
assert 'DWORD now=GetTickCount();syncTargetSelection(now);consumeSample(now);' in monitor
out = Path(args.out).resolve()
out.mkdir(parents=True, exist_ok=True)
admission_start = cursor.index('if((scan.processCount!=1&&!scan.explicitSelection)||!scan.best.pid)')
admission = cursor[admission_start:cursor.index(';', admission_start) + 1]
fixture = r'''
#ifdef NDEBUG
#error "target lifecycle assertions must be enabled"
#endif
#define RA2_UNIFIED
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>
using DWORD=uint32_t;using HWND=void*;
struct FILETIME {DWORD dwLowDateTime=0,dwHighDateTime=0;};
struct Process {DWORD pid=0;uint64_t birth=0;bool active=true,times=true;};
using HANDLE=Process*;
constexpr DWORD PROCESS_QUERY_LIMITED_INFORMATION=0x1000,SYNCHRONIZE=0x100000,WAIT_TIMEOUT=258;
constexpr int FALSE=0,SW_HIDE=0;
static DWORD nowTick=10000;static unsigned checks=0,opens=0,policyCopies=0;static std::vector<std::string> events;
static std::map<DWORD,std::shared_ptr<Process>> processes;
static DWORD GetTickCount(){return nowTick;}
static HANDLE OpenProcess(DWORD,int,DWORD pid){++opens;auto p=processes.find(pid);return p==processes.end()?nullptr:p->second.get();}
static DWORD WaitForSingleObject(HANDLE p,DWORD){return p&&p->active?WAIT_TIMEOUT:0;}
static bool GetProcessTimes(HANDLE p,FILETIME* created,FILETIME*,FILETIME*,FILETIME*){if(!p||!p->times)return false;created->dwLowDateTime=DWORD(p->birth);created->dwHighDateTime=DWORD(p->birth>>32);return true;}
static void CloseHandle(HANDLE){}
static void ShowWindow(HWND,int){}
static void check(bool okay,const char* why){++checks;if(!okay){std::fprintf(stderr,"FAIL %s\n",why);std::exit(1);}}
static std::shared_ptr<Process> spawn(DWORD pid,uint64_t birth,bool active=true){auto p=std::make_shared<Process>();p->pid=pid;p->birth=birth;p->active=active;processes[pid]=p;return p;}
namespace external {
struct Connection {std::shared_ptr<Process> process;HANDLE handle=nullptr;DWORD pid=0;bool alive()const{return process&&process->active;}};
struct ReadSession {
 std::string tag;std::shared_ptr<Connection> value;unsigned closes=0;
 std::shared_ptr<const Connection> connectionIdentity()const{return value;}
 bool valid()const{return bool(value);}bool alive()const{return value&&value->alive();}
 void close(){events.push_back(tag+".close");++closes;value.reset();}
 void bind(std::shared_ptr<Process> p){value=std::make_shared<Connection>(Connection{p,p.get(),p->pid});}
};
namespace targetselection {
struct Policy {DWORD selectedPid=0;uint64_t selectedCreation=0,revision=1;};
static Policy choice;
static uint64_t revision(){return choice.revision;}
static Policy current(){++policyCopies;return choice;}
PREFERRED_ALIVE
}
}
static void select(DWORD pid,uint64_t birth){auto& c=external::targetselection::choice;c.selectedPid=pid;c.selectedCreation=birth;++c.revision;}
static void editDetectionList(){++external::targetselection::choice.revision;}
struct Resettable {unsigned count=0;void clear(){++count;}void reset(){++count;}void cancel(){++count;}void resetObservation(){++count;}void suspend(){++count;}void hide(){++count;}};
namespace suite {static void publishHardwareCursor(bool){}}
namespace cursor {
static external::ReadSession cursorReader{"cursor"};static DWORD pid=0,threadId=0,gameParentPid=0,lastWindowCheck=0,lastAutoCheck=0;
static HWND game=0,overlay=0,hud=0;static bool running=false,autoStart=true,autoPaused=false,manualCursorPause=false,readRestricted=false,playbackReady=true;
static int sentinel=0;static int* shared=nullptr;static unsigned hook=0;static HANDLE mapping=nullptr;
static Resettable bridgeFrame,lanboStream,fallbackTracker,burst,tween,timing,activeMotion;
static bool usingHeldSample=false,nativeMenuHold=false,wasInside=false;static size_t lastPhase=0;
static int diagFrame=0,diagHotX=0,diagHotY=0,previousObservedFrame=0,refreshTries=0,clickGameWidth=0,clickGameHeight=0;
static DWORD nativeMenuCandidateAt=0;static std::wstring mode=L"hide",lastStatus;
struct Rect {int left=1,top=1,right=2,bottom=2;};static Rect clickRegion;
static void SetRectEmpty(Rect* r){*r={0,0,0,0};}
static void status(const std::wstring& text){lastStatus=text;}
static void fail(const char* text){throw std::runtime_error(text);}
static void hookStop(){check(cursorReader.valid()&&cursorReader.connectionIdentity()->pid==pid,"paired cursor restore still owns old reader and PID");events.push_back("cursor.restore:"+std::to_string(pid));shared=nullptr;hook=0;mapping=nullptr;}
static bool live(){return cursorReader.alive();}
static void attach(bool){}static void start(){}static std::wstring widen(const char*){return L"failure";}
CURSOR_MATCHES
CURSOR_STOP
CURSOR_SWITCH
CURSOR_AUTO_TICK
struct Scan {unsigned processCount=1;bool explicitSelection=false;struct {DWORD pid=1;}best;};
static bool admitted(Scan scan){try{CURSOR_ADMISSION return true;}catch(const std::exception&){return false;}}
static void afterOpen(Scan scan){CURSOR_OPEN_GUARD}
static void bind(std::shared_ptr<Process> process,bool active=true){cursorReader.bind(process);pid=process->pid;threadId=pid+1;game=reinterpret_cast<HWND>(uintptr_t(pid+2));gameParentPid=pid+3;running=active;shared=active?&sentinel:nullptr;hook=active?1:0;mapping=active?process.get():nullptr;}
static void run(){
 auto old=spawn(10,100);bind(old);select(0,0);syncTargetSelection();events.clear();
 select(10,100);syncTargetSelection();check(events.empty()&&pid==10,"selecting current cursor identity keeps connection");
 auto beforeOpens=opens;editDetectionList();syncTargetSelection();check(events.empty()&&opens==beforeOpens,"cursor detection-list edit does not revalidate or detach selection");
 auto beforeCopies=policyCopies;syncTargetSelection();check(policyCopies==beforeCopies,"unchanged cursor revision avoids policy copy");
 spawn(20,200,false);select(20,200);syncTargetSelection();check(events.empty()&&pid==10,"exited selected cursor target does not stop healthy old game");
 const uint64_t nextBirth=(uint64_t(0x12345678)<<32)|201;spawn(20,nextBirth);select(0,0);syncTargetSelection();select(20,200);syncTargetSelection();check(events.empty()&&pid==10,"reused selected PID with stale birth cannot switch cursor");
 autoStart=true;autoPaused=true;manualCursorPause=true;select(20,nextBirth);syncTargetSelection();
 check(events==std::vector<std::string>{"cursor.restore:10","cursor.close"},"cursor restores old hook before closing reader");
 check(!cursorReader.valid()&&!running&&!pid&&!threadId&&!game&&!gameParentPid,"cursor switch clears old connection identity");
 check(autoStart&&!autoPaused&&manualCursorPause,"cursor switch preserves enablement/manual pause and retries old target failure");
 check(!hook&&!shared&&!mapping&&lastAutoCheck==0&&lastWindowCheck==0,"cursor cleanup releases old bridge and schedules discovery");
 const auto stopped=events.size();syncTargetSelection();check(events.size()==stopped,"one cursor selection change performs one cleanup");
 bind(processes[20]);events.clear();select(0,0);syncTargetSelection();check(events.empty()&&pid==20,"clearing cursor preferred PID keeps healthy connection");
 autoStart=false;autoPaused=true;manualCursorPause=true;spawn(30,300);select(30,300);lastAutoCheck=nowTick;autoTick();
 check(!cursorReader.valid()&&!pid&&!autoStart&&autoPaused&&manualCursorPause,"cursor selection is observed before automatic polling gate without enabling module");
 auto dead=spawn(40,400);bind(dead);dead->active=false;auto current=spawn(40,401);select(40,401);events.clear();syncTargetSelection();
 check(!cursorReader.valid()&&!pid,"same numeric cursor PID with new birth replaces dead connection");
 bind(current,false);check(selectedReaderMatches(external::targetselection::current(),cursorReader),"cursor reader birth matches selected incarnation");
 Scan scan;scan.processCount=2;scan.explicitSelection=false;check(!admitted(scan),"ambiguous automatic cursor scan is rejected");
 scan.explicitSelection=true;check(admitted(scan),"explicit cursor selection admits one of multiple instances");scan.best.pid=0;check(!admitted(scan),"explicit selection still requires candidate PID");scan.best.pid=40;
 afterOpen(scan);check(cursorReader.valid(),"matching explicit cursor handle survives post-open check");
 auto replacement=spawn(40,402);bind(replacement,false);bool rejected=false;try{afterOpen(scan);}catch(const std::exception&){rejected=true;}
 check(rejected&&!cursorReader.valid()&&!pid,"cursor post-open guard rejects PID reuse after discovery");
}
}
namespace um {struct Snapshot {int value=0;};}
namespace monitor {
static external::ReadSession reader{"monitor"};static DWORD pid=0,threadId=0,lastGood=0,lastSearch=0,lastData=0,oldMatch=0;static HWND game=0;
static bool enabled=true,secondaryEnabled=true,demo=true,gameActive=true,renderDirty=false,secondaryDirty=false,dragging=true;
static int page=3,paintedAlpha=255;static uint64_t samplingGeneration=9;
static std::shared_ptr<const int> appliedSample=std::make_shared<int>(1);static std::shared_ptr<const um::Snapshot> snap=std::make_shared<um::Snapshot>();
static Resettable overlayRecovery,tabs,fade,surface;static unsigned primaryResets=0,secondaryResets=0;
struct Clock {void state(bool,unsigned){}};static Clock statsFrameClock;
struct Request {uint64_t generation=0;DWORD pid=0;};
struct Worker {unsigned invalidated=0;Request last{};void invalidate(){++invalidated;}void submit(Request r){last=r;}};static Worker samplingWorker;
static unsigned statsInformationFPS(){return 60;}static void statsVisible(bool){}static void hidePrimaryStats(){}
static void resetPrimaryRender(){++primaryResets;}static void resetSecondary(){++secondaryResets;}
static void finishSecondaryDrag(){events.push_back("monitor.finishDrag");dragging=false;}
static std::wstring lastStatus;static void setStatus(const std::wstring& s){lastStatus=s;}
MONITOR_MATCHES
MONITOR_DETACH
MONITOR_SWITCH
struct Scan {bool explicitSelection=true;};
static bool afterOpen(Scan scan){MONITOR_OPEN_GUARD return true;}
static void bind(std::shared_ptr<Process> process){reader.bind(process);pid=process->pid;threadId=pid+1;game=reinterpret_cast<HWND>(uintptr_t(pid+2));appliedSample=std::make_shared<int>(1);}
static void run(){
 select(0,0);bind(spawn(50,500));syncTargetSelection(nowTick);events.clear();
 select(50,500);syncTargetSelection(nowTick);check(events.empty()&&pid==50,"selecting current monitor identity keeps connection");
 const auto beforeOpens=opens;editDetectionList();syncTargetSelection(nowTick);check(events.empty()&&opens==beforeOpens,"monitor detection-list edit never detaches healthy session");
 auto beforeCopies=policyCopies;syncTargetSelection(nowTick);check(policyCopies==beforeCopies,"unchanged monitor revision avoids policy copy");
 spawn(60,600,false);select(60,600);syncTargetSelection(nowTick);check(events.empty()&&pid==50,"exited monitor preference preserves current game");
 spawn(60,601);select(0,0);syncTargetSelection(nowTick);select(60,600);syncTargetSelection(nowTick);check(events.empty()&&pid==50,"stale selected creation cannot switch monitor to reused PID");
 select(60,601);const auto beforeGeneration=samplingGeneration;syncTargetSelection(nowTick);
 check(events==std::vector<std::string>{"monitor.finishDrag","monitor.close"},"monitor ends drag before detaching old reader");
 check(samplingGeneration==beforeGeneration+1&&samplingWorker.invalidated==1&&!appliedSample,"normal monitor detach invalidates old sample generation");
 check(samplingWorker.last.generation==samplingGeneration&&!samplingWorker.last.pid,"monitor publishes empty sampling request on switch");
 check(primaryResets==1&&secondaryResets==1&&!reader.valid()&&!pid&&!game&&!threadId,"monitor switch clears old rendering and reader identity");
 check(!dragging&&enabled&&secondaryEnabled&&demo,"monitor switch preserves user module/secondary/demo settings");
 check(lastSearch==nowTick-1500&&lastData==nowTick-250,"monitor selection schedules immediate discovery cadence");
 auto count=samplingWorker.invalidated;syncTargetSelection(nowTick);check(samplingWorker.invalidated==count,"one monitor selection change detaches once");
 bind(processes[60]);events.clear();select(0,0);syncTargetSelection(nowTick);check(events.empty()&&pid==60,"clearing monitor preferred PID preserves healthy session");
 select(60,601);syncTargetSelection(nowTick);check(afterOpen({true})&&reader.valid(),"matching explicit monitor handle passes post-open identity check");
 bind(spawn(60,602));check(!afterOpen({true})&&!reader.valid(),"monitor post-open guard rejects PID reuse after discovery");
}
}
int main(){cursor::run();monitor::run();std::printf("{\"checks\":%u,\"assertions_enabled\":true}\n",checks);}
'''
replacements = {
    'PREFERRED_ALIVE': block(selection, 'inline bool preferredAlive(const Policy& policy)'),
    'CURSOR_MATCHES': block(cursor, 'static bool selectedReaderMatches('),
    'CURSOR_STOP': block(cursor, 'static void stop(){'),
    'CURSOR_SWITCH': block(cursor, 'static void syncTargetSelection(){'),
    'CURSOR_AUTO_TICK': block(cursor, 'static void autoTick(){'),
    'CURSOR_ADMISSION': admission,
    'CURSOR_OPEN_GUARD': block(cursor, 'if(scan.explicitSelection&&!selectedReaderMatches('),
    'MONITOR_MATCHES': block(monitor, 'static bool selectedReaderMatches('),
    'MONITOR_DETACH': block(monitor, 'static void detach(){'),
    'MONITOR_SWITCH': block(monitor, 'static void syncTargetSelection(DWORD now){'),
    'MONITOR_OPEN_GUARD': block(monitor, 'if(scan.explicitSelection&&!selectedReaderMatches('),
}
for key, value in replacements.items():
    fixture = fixture.replace(key, value)
generated = out / 'target-lifecycle-production.cpp'
generated.write_text(fixture, encoding='utf-8')
binary = out / 'target-lifecycle-production'
built = subprocess.run([args.cxx, '-std=c++17', '-O1', '-UNDEBUG', str(generated), '-o', str(binary)],
                       capture_output=True, text=True)
(out / 'build.log').write_text(built.stdout + built.stderr, encoding='utf-8')
if built.returncode:
    raise SystemExit(built.stdout + built.stderr)
ran = subprocess.run([str(binary)], capture_output=True, text=True, timeout=10)
(out / 'run.log').write_text(ran.stdout + ran.stderr, encoding='utf-8')
if ran.returncode:
    raise SystemExit(ran.stdout + ran.stderr)
receipt = {
    'schema': 'ra2.target-lifecycle-regression.v1',
    'source_sha256': before,
    'changed_during_run': [str(path.relative_to(root)) for path in tracked
                           if hashlib.sha256(path.read_bytes()).hexdigest() != before[str(path.relative_to(root))]],
    'result': json.loads(ran.stdout),
    'production_bodies_executed': list(replacements),
    'win32_calls': 'controlled process/handle fixture only; Hook restoration mocked',
    'windows_ui_executed': False,
    'game_process_accessed': False,
    'exit_code': ran.returncode,
}
(out / 'result.json').write_text(json.dumps(receipt, ensure_ascii=False, indent=2), encoding='utf-8')
print(json.dumps(receipt, ensure_ascii=False, indent=2))
