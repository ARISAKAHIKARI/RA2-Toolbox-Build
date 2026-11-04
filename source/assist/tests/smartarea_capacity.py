"""Actual MO bridge, GameView entry readers and nativeMission router.

The engine callback/queue and Win32 boundaries are controlled substitutes;
this fixture neither loads nor controls a game process.
"""
from pathlib import Path
import argparse,hashlib,json,subprocess,sys
sys.path.insert(0,str(Path(__file__).resolve().parent))
import mo_orders_bridge_test as mo

HERE=Path(__file__).resolve().parent
SOURCE=HERE.parent/'source'
ROOT=HERE.parents[1]

def block(text,needle):
 start=text.index(needle);at=text.index('{',text.index(')const',start)) if needle.startswith('template<class T>T get(') else text.index('{',start);end=at+1;depth=1
 while depth:depth+=(text[end]=='{')-(text[end]=='}');end+=1
 return text[start:end]

def source(bridge,orders,body=None):
 text=mo.source();text=text[:text.index('static void begin(){')]
 text=text.replace('#include <vector>','#include <vector>\n#include <functional>\n#include <cstdio>')
 text=text.replace('static int kind(uint32_t p){return worldObjects.contains(p)?1:0;}',r'''static int kind(uint32_t p){uint32_t v=get<uint32_t>(p);return v==0x7F5C70?2:v==0x7EB058?1:v==0x7E3EBC?3:v==0x7E22A4?0:-1;}''')
 text=text.replace('static uint32_t typeOf(uint32_t p,int){return get<uint32_t>(p+0x6C4);}','static uint32_t typeOf(uint32_t p,int k){return k>=0?get<uint32_t>(p+(k==3?0x520:k==1?0x6C0:0x6C4)):0;}')
 text=text.replace('struct Vec{int count=0;};static Vec vec(uint32_t p){assert(p==0xA8ECB8);return {int(selected.size())};}\nstatic uint32_t item(Vec,int n){return selected.at(n);}',r'''struct Vec{int count=0;uint32_t at=0;};static Vec vec(uint32_t p){assert(p==0xA8ECB8||p==0xA8EC78);return {int(p==0xA8ECB8?selected.size():actors.size()),p};}
static uint32_t item(Vec v,int n){return v.at==0xA8ECB8?selected.at(n):actors.at(n);}''')
 text=text.replace('namespace taskarbiter{enum class Owner{Formation,Generic};}', '#include "command_gateway.h"\n#include "order_policy.h"')
 text=text.replace('enum class OrderAuthority{Local};','enum class OrderAuthority{Local,PlayerTest};')
 text=text.replace('static struct {bool allowed=true;bool canIssue(uint32_t,int,uint32_t,uint32_t,uint32_t,taskarbiter::Owner){return allowed;}}actorLeaseArbiter;','static taskarbiter::LeaseArbiter<> actorLeaseArbiter;')
 text=text.replace('int status=1,commandStatus=1,frame=100,session=0,chat=0;', 'int status=1,commandStatus=1,frame=100,session=0,chat=0,commandRejected=0,receiptDropped=0;')
 text=text.replace('bool stop=false,foreground=true;uint32_t heartbeat=1000;', 'bool stop=false,foreground=true;uint32_t heartbeat=1000,controlAll=0;')
 start=text.index('static bool submitConstructed(');end=text.index('static void inputFeedback',start)
 native=block(bridge,'static bool nativeMission(')
 text=text[:start]+r'''
static std::function<void()> beforeQueue;static int highLevelCalls=0,constructCalls=0;static bool highBackend=false;
static bool stickyOwns(uint32_t);
static bool submitConstructed(const intent::EventOrder&o,OrderAuthority=OrderAuthority::Local){
 ++constructCalls;if(beforeQueue){auto run=std::move(beforeQueue);beforeQueue={};run();}
 if(!validateMoSubmission(o)||constructors::admit(o)!=constructors::Verdict::Ready)return false;events.push_back(o);commands.push_back({uint32_t(o.a),uint32_t(o.c)});return true;
}
static bool submitMegaMission(uint32_t house,uint32_t id,int mission,uint32_t target,uint8_t targetTag,uint32_t dest,uint8_t destTag,uint32_t nearest,uint8_t nearestTag){
 auto order=intent::mega(get<int>(house+0x30),id,mission,target,targetTag,dest,destTag,nearest,nearestTag);
 if(!highBackend)return submitConstructed(order);
 ++highLevelCalls;if(beforeQueue){auto run=std::move(beforeQueue);beforeQueue={};run();}events.push_back(order);return true;
}
namespace controlmode{static bool proxyOrders(uint32_t){return false;}}
static bool playableHouse(uint32_t){return true;}static bool loadInterfaces(){return true;}static bool missileCleanup=false;
static gateway::MissionObserver<> missionObserver;static void showOrderLines(uint32_t){}static void cancelReceiptTicket(uint32_t,uint32_t){}static void recordReceipt(const gateway::Receipt&){}
'''+native+'\n'+text[end:]
 text=text.replace('static bool GuardedEnterAction(uint32_t,uint32_t,uint32_t,unsigned*out){*out=5;return true;}',r'''static unsigned actionValue=3;static bool actionOK=true;static bool GuardedEnterAction(uint32_t,uint32_t,uint32_t,unsigned*out){*out=actionValue;return actionOK;}''')
 start=text.index('static struct {struct Capacity');end=text.index('#include "cooperative_types.h"',start)
 game=(ROOT/'external_reader/source/game_view.h').read_text(encoding='utf-8-sig')
 entry=game[game.index('struct EntryCapacity'):game.index('template<class Read>')]
 generated='#include "autoload.h"\nstruct FixtureRead{bool operator()(uint32_t at,void*out,size_t n)const{return read(at,out,n);}};\n'+entry+'\nclass EntryView{FixtureRead read_;public:\n'
 for needle in ['template<class T>T get(','bool bytes(','int kind(','uint32_t type(','bool live(','bool neutral(','bool allied(','int radioContacts(','bool radioLinked(','EntryCapacity capacity(','int entrySlots(','uint32_t entryDestination(','bool entered(','bool entryCompatible(']:generated+=block(game,needle)+'\n'
 generated+='};static EntryView clickView;\n'
 text=text[:start]+generated+text[end:]
 coop=(SOURCE/'cooperative_bridge.h').read_text(encoding='utf-8-sig')
 text=text.replace('static int entryReserved(uint32_t,uint32_t,uint32_t){return 0;}',block(coop,'static int entryReserved('))
 text=text.replace('#include "mo_orders_bridge.h"','#include '+json.dumps(str(orders)))
 text=text.replace('static int checks=0;static void check(bool b){++checks;assert(b);}', 'static int checks=0;static void checkAt(bool b,int line,const char*expression){++checks;if(!b){std::fprintf(stderr,"check %d line %d failed: %s\\n",checks,line,expression);std::abort();}}\n#define check(x) checkAt(bool(x),__LINE__,#x)')
 return text+'\n'+((HERE/'smartarea_capacity.cpp').read_text(encoding='utf-8-sig') if body is None else body)

if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--zig',required=True,type=Path);p.add_argument('--out',required=True,type=Path);p.add_argument('--baseline',type=Path,default=HERE/'fixtures/mo_smartarea_baseline');a=p.parse_args();a.out.mkdir(parents=True,exist_ok=True)
 paths=['assist/source/mo_orders_bridge.h','assist/source/mo_orders.h','assist/source/mo_gesture_bridge.h','assist/source/bridge.cpp','assist/source/cooperative_bridge.h','assist/source/cooperative_types.h','external_reader/source/game_view.h','assist/source/command_gateway.h','assist/source/unit_task_arbiter.h','assist/source/autoload.h','assist/tests/smartarea_capacity.cpp','assist/tests/smartarea_capacity.py','assist/tests/mo_orders_bridge_test.py','assist/tests/formation_quick_bridge_test.py']
 manifest=json.loads((a.baseline/'manifest.json').read_text()) if (a.baseline/'manifest.json').is_file() else {}
 for name,digest in manifest.get('files',{}).items():assert hashlib.sha256((a.baseline/name).read_bytes()).hexdigest()==digest
 paths += [str(p.relative_to(ROOT)).replace('\\','/') for p in (HERE/'fixtures/mo_smartarea_baseline').iterdir() if p.is_file()]
 snapshot=lambda:{n:hashlib.sha256((ROOT/n).read_bytes()).hexdigest() for n in paths};before=snapshot();reports={}
 variants=[('current',SOURCE/'bridge.cpp',SOURCE/'mo_orders_bridge.h')]
 if a.baseline:variants.append(('baseline',a.baseline/'native_mission.inc' if (a.baseline/'native_mission.inc').is_file() else a.baseline/'source/assist/source/bridge.cpp',a.baseline/'mo_orders_bridge.h' if (a.baseline/'mo_orders_bridge.h').is_file() else a.baseline/'source/assist/source/mo_orders_bridge.h'))
 for label,bridgepath,orders in variants:
  cpp=a.out/(label+'.cpp');cpp.write_text(source(bridgepath.read_text(encoding='utf-8-sig'),orders.resolve()),encoding='utf-8');reports[label]={}
  for arch in ('x86_64','x86'):
   exe=a.out/(label+'-'+arch+'.exe');cmd=[str(a.zig.resolve()),'c++','-target',arch+'-windows-gnu','-std=c++17','-O1','-UNDEBUG','-I'+str(SOURCE),str(cpp),'-o',str(exe)]
   if label=='baseline':cmd.append('-DEXPECT_BASELINE=1')
   run=subprocess.run(cmd,capture_output=True,text=True);(a.out/(label+'-'+arch+'-build.log')).write_text(run.stdout+run.stderr,encoding='utf-8')
   if run.returncode:raise SystemExit(run.stdout+run.stderr)
   run=subprocess.run([str(exe)],capture_output=True,text=True,timeout=90);(a.out/(label+'-'+arch+'.log')).write_text(run.stdout+run.stderr,encoding='utf-8')
   if run.returncode:raise SystemExit(run.stdout+run.stderr)
   reports[label][arch]=json.loads(run.stdout.strip().splitlines()[-1])
 changed=[n for n,d in snapshot().items() if d!=before[n]]
 report={'results':reports,'baseline':manifest,'sources':before,'changed_during_run':changed,'scope':'Actual mo_orders bridge, GameView entry readers, entryReserved and nativeMission routing; synthetic world and modeled queue/clock/Win32 boundaries; no game process or multiplayer acceptance.'}
 (a.out/'result.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2));assert not changed,changed
