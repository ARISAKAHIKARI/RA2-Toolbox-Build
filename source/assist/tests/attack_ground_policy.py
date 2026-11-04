"""Compile and exercise production own-ground batching and hotkey admission with fake game IO.
Native event delivery, full game execution and multiplayer behavior are not tested here.
"""
import argparse,json,os,shlex,shutil,subprocess,tempfile
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--output',type=Path);a=p.parse_args()
root=Path(__file__).resolve().parents[1]
def extract(text,signature):
 start=text.index(signature);opening=text.index('{',start);depth=1;end=opening+1
 while depth:depth+=(text[end]=='{')-(text[end]=='}');end+=1
 return text[start:end]
protocol=(root/'source/protocol.h').read_text(encoding='utf-8')
commands=protocol[protocol.index('enum Command '):protocol.index('enum Flags ')]
hotkey=extract((root/'source/hotkey_host.h').read_text(encoding='utf-8'),'static void ownGroundHotkey(')
code=r'''
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <unordered_map>
#include <set>
using DWORD=uint32_t;using std::size_t;
'''+commands+r'''
static_assert(AttackOwnGround==34&&PlaceCompletedBuilding==33&&SetSleep==29);
static_assert(selectedmode::Count==4&&selectedmode::command(3)==AttackOwnGround&&selectedmode::index(AttackOwnGround)==3);
struct Shared{uint32_t request=1,stop=0;int result=0,count=0;}shared,*sh=&shared;
uint32_t match=1,pendingAck=0;bool pendingAckReady=false,fault=false,ready=true,session=true;
int budget=24,voices=0,forcedCount=-1,failed=false,cancelled=0;
std::unordered_map<uint32_t,uint32_t>memory,kinds,types,ids,tokens;
std::vector<uint32_t>selection;
struct Event{uint32_t id,target;int tag;};std::vector<Event>events;
template<class T>T get(uint32_t at){uint32_t v=memory[at];T out{};std::memcpy(&out,&v,sizeof(T));return out;}
void w(uint32_t at,uint32_t value){memory[at]=value;}
struct Objects{bool contains(uint32_t p){return ids.count(p)!=0;}}worldObjects;
struct Versions{uint32_t get(uint32_t id){return tokens[id];}}manualVersions;
struct Vec{int count;};Vec vec(uint32_t){return {forcedCount>=0?forcedCount:int(selection.size())};}
uint32_t item(Vec,int n){return selection.at(n);}int kind(uint32_t p){return kinds[p];}
bool own(uint32_t p,uint32_t house){return memory[p+0x21c]==house;}
bool live(uint32_t p){return memory[p+0x6c]>0;}bool stickyOwns(uint32_t){return false;}
uint32_t worldByID(uint32_t id){for(auto x:ids)if(x.second==id)return x.first;return 0;}
uint32_t typeOf(uint32_t p,int){return types[p];}
int rawWeaponSlots(uint32_t,uint32_t,int(&slots)[2]){slots[0]=0;return 1;}
uint32_t rawWeapon(uint32_t type,float,int){return memory[type+0x898];}
uint32_t readMapCell(int x,int y){return x>=0&&x<512&&y>=0&&y<512?0x88000:0;}
bool simulationReady(){return ready;}bool outgoingRoom(){return budget>0;}
void cancelNative(uint32_t,bool){++cancelled;}void inputFeedback(uint32_t,int,bool){++voices;}
namespace eventdelivery{bool mayQueue(int){return session;}}
namespace taskarbiter{enum class Owner{Generic};}
bool nativeMission(uint32_t,uint32_t id,int mission,uint32_t target,uint32_t dest,int destTag,uint32_t nearest,int nearestTag,taskarbiter::Owner,uint32_t*,int tag){
 assert(mission==1&&!dest&&!destTag&&!nearest&&!nearestTag&&(tag==11||tag==0x34));
 if(failed)return false;assert(budget>0);--budget;events.push_back({id,target,tag});return true;
}
'''
code+='#include '+json.dumps((root/'source/attack_ground_bridge.h').as_posix())+'\n'
code+='#include '+json.dumps((root.parents[0]/'suite/source/settings_categories.h').as_posix())+'\n'
code+=r'''
namespace hotkeyroute{struct Group{std::vector<int>actions,bindings;};}
int notifications=0,keySends=0;
void notify(const wchar_t*){++notifications;}void send(int c){assert(c==AttackOwnGround);++keySends;}
'''+hotkey+r'''
uint32_t actor(int index,int range=256,bool area=false){
 uint32_t p=0x1000000+index*4096,t=p+0x1000000,weapon=p+0x2000000,id=index+1000;
 ids[p]=id;kinds[p]=1;types[p]=t;w(p+0x10,id);w(p+0x21c,0x90000);w(p+0x6c,130);
 w(p+0x9c,2600);w(p+0xa0,2700);w(t+0x898,weapon);w(weapon+0xb4,range);w(weapon+0x134,area);
 selection.push_back(p);return p;
}
void fresh(){
 resetOwnGround();shared={};shared.request=1;sh=&shared;match=1;fault=false;ready=session=true;
 budget=24;voices=0;forcedCount=-1;failed=false;cancelled=0;pendingAckReady=false;pendingAck=0;
 memory.clear();kinds.clear();types.clear();ids.clear();tokens.clear();selection.clear();events.clear();w(0xa8ed84,10);
}
int main(){unsigned checks=0;auto check=[&](bool x){++checks;if(!x){std::fprintf(stderr,"failed check %u\n",checks);std::abort();}};
 fresh();actor(0,0);actor(1,0,true);actor(2,256);attackOwnGroundCommand(0x90000,100,1);
 check(events.size()==3&&events[0].tag==0x34&&events[0].target==1000);
 check(events[1].tag==11&&events[2].tag==11&&events[1].target==10010);
 check(!ownground::job.pending&&pendingAckReady&&shared.count==3&&voices==1);
 fresh();for(int i=0;i<60;++i)actor(i,i%2?0:256);attackOwnGroundCommand(0x90000,100,1);
 check(events.size()==24&&ownground::job.pending&&!pendingAckReady);
 budget=24;ownGroundTick(0x90000,10,101);check(events.size()==24); // no second send in same frame
 tokens[1050]=1;w(0x1000000+49*4096+0x6c,0);selection.clear();actor(500,0);
 ownGroundTick(0x90000,11,116);check(events.size()==48&&ownground::job.pending);
 budget=24;ownGroundTick(0x90000,12,132);
 check(events.size()==58&&shared.count==58&&!ownground::job.pending&&pendingAckReady&&voices==1);
 bool frozen=true;for(auto e:events)frozen&=e.id!=1500&&e.id!=1049&&e.id!=1050;check(frozen);
 fresh();for(int i=0;i<25;++i)actor(i);attackOwnGroundCommand(0x90000,100,1);
 match=2;budget=24;ownGroundTick(0x90000,11,116);check(events.size()==24&&shared.result==-7&&!ownground::job.pending);
 fresh();actor(0);budget=0;attackOwnGroundCommand(0x90000,100,1);check(ownground::job.pending&&events.empty());
 budget=24;ownGroundTick(0x90000,11,3601);check(events.empty()&&shared.result==-7);
 fresh();uint32_t unit=actor(0);budget=0;attackOwnGroundCommand(0x90000,100,1);w(unit+0x10,9999);
 budget=24;ownGroundTick(0x90000,11,116);check(events.empty()&&shared.count==0&&!ownground::job.pending);
 fresh();unit=actor(0);w(unit+0x21c,0x90001);attackOwnGroundCommand(0x90000,100,1);check(events.empty()&&shared.count==0);
 fresh();unit=actor(0);w(unit+0x11c,123);attackOwnGroundCommand(0x90000,100,1);check(events.empty());
 fresh();unit=actor(0);w(unit+0xac,6);attackOwnGroundCommand(0x90000,100,1);check(events.empty()&&shared.result==-41);
 fresh();actor(0);session=false;attackOwnGroundCommand(0x90000,100,1);check(events.empty()&&shared.result==-40);
 fresh();forcedCount=8193;attackOwnGroundCommand(0x90000,100,1);check(events.empty()&&shared.result==-42);
 fresh();actor(0);failed=true;attackOwnGroundCommand(0x90000,100,1);check(events.empty()&&shared.result==-6);
 fresh();actor(0);budget=0;attackOwnGroundCommand(0x90000,100,1);shared.request=2;resetOwnGround();
 check(!pendingAckReady&&!ownground::job.pending); // never ACK an unrelated request after reset
 fresh();actor(0);budget=0;attackOwnGroundCommand(0x90000,100,1);fault=true;ownGroundTick(0x90000,11,116);
 check(events.empty()&&shared.result==-7);
 for(bool pause:{false,true}){fresh();actor(0);budget=0;attackOwnGroundCommand(0x90000,100,1);
  if(pause)ready=false;else w(0xa8bc60+25,1);budget=24;ownGroundTick(0x90000,11,116);check(events.empty()&&shared.result==-7);}
 hotkeyroute::Group key{{1727},{1727}};ownGroundHotkey(key,true);check(keySends==1&&key.actions.empty());
 key={{1727},{1727}};ownGroundHotkey(key,false);check(keySends==1&&notifications==1&&key.actions.empty());
 key={{1727,1001},{1727,1001}};ownGroundHotkey(key,true);check(keySends==1&&notifications==2&&key.actions==std::vector<int>{1001});
 key={{1001},{1001}};ownGroundHotkey(key,true);check(keySends==1&&key.actions.size()==1);
 int ids[]={9176,9177};check(suite::categories::row(4,ids,2)==suite::categories::Control);
 std::printf("PASS %u production own-ground batching/input checks\n",checks);
}
'''
compiler=shlex.split(os.environ['CXX']) if os.environ.get('CXX') else [os.environ.get('ZIG_EXE') or os.environ['ZIG'],'c++',*(['-target','x86_64-windows-gnu'] if os.name=='nt' else [])] if os.environ.get('ZIG_EXE') or os.environ.get('ZIG') else [shutil.which('g++') or 'c++']
with tempfile.TemporaryDirectory(prefix='own-ground-') as directory:
 tmp=Path(directory);cpp=tmp/'test.cpp';exe=tmp/('test.exe' if os.name=='nt' else 'test');cpp.write_text(code,encoding='utf-8')
 subprocess.run([*compiler,'-std=c++17','-O2','-UNDEBUG',str(cpp),'-o',str(exe)],check=True)
 r=subprocess.run([str(exe)],capture_output=True,text=True)
 if r.returncode:raise RuntimeError(r.stderr or r.stdout)
 print(r.stdout,end='')
if a.output:
 a.output.parent.mkdir(parents=True,exist_ok=True)
 a.output.write_text(json.dumps({'scope':__doc__,'status':'PASS','output':r.stdout,'game_tested':False},indent=2)+'\n')
