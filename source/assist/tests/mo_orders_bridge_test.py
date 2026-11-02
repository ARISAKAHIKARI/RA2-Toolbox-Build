"""Actual MO bridge with bounded synthetic world and serialized event gateway.
Reuses the existing formation boundary fixture; never loads or controls a game.
"""
from pathlib import Path
import argparse, ast, json, os, subprocess, tempfile
HERE=Path(__file__).resolve().parent
def source():
    tree=ast.parse((HERE/'formation_quick_bridge_test.py').read_text(encoding='utf-8'))
    text=next(ast.literal_eval(n.value) for n in tree.body if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='fixture' for t in n.targets))
    text=text[:text.index('int main(){')]
    text=text.replace('using namespace assist;', '#include "constructor_adapter.h"\nusing namespace assist;\nstatic bool validateMoSubmission(const intent::EventOrder&);')
    text=text.replace('static bool read(uint32_t p,void*out,size_t n){if(size_t(p)+n>memory.size())return false;', 'static uint32_t failAt=0;static bool read(uint32_t p,void*out,size_t n){if(p<0x10000||size_t(p)+n>memory.size()||(failAt>=p&&failAt<uint64_t(p)+n))return false;')
    text=text.replace('uint32_t get(uint32_t id){return tokens[id];}', 'bool trackable(uint32_t id){return id!=0;}uint32_t get(uint32_t id){return tokens[id];}')
    text=text.replace('static void cancelNative(uint32_t p)', 'static void cancelNative(uint32_t p,bool=false)')
    text=text.replace('uint32_t moFeatures=0;', 'volatile long moOrdersPreviewSeq=0;moorders::Preview moOrdersPreview;uint32_t moOrderStatus=0,moOrderAccepted=0,moOrderRejected=0;uint32_t moFeatures=0;')
    text=text.replace('enum class Owner{Formation}', 'enum class Owner{Formation,Generic}')
    start=text.index('static bool nativeMission(');end=text.index('static void inputFeedback',start)
    text=text[:start]+r'''
enum class OrderAuthority{Local};
static struct {int frame=-1;std::map<uint32_t,int>issued;void begin(uint32_t,int f){if(frame!=f){frame=f;issued.clear();}}bool canIssue(uint32_t id,uint32_t){return !issued.count(id);}void record(uint32_t id,uint32_t,taskarbiter::Owner){issued[id]=1;}}actorFrameArbiter;
static struct {bool allowed=true;bool canIssue(uint32_t,int,uint32_t,uint32_t,uint32_t,taskarbiter::Owner){return allowed;}}actorLeaseArbiter;
static std::vector<intent::EventOrder> events;
static bool submitConstructed(const intent::EventOrder&o){if(!validateMoSubmission(o)||constructors::admit(o)!=constructors::Verdict::Ready)return false;events.push_back(o);return true;}
static bool nativeMission(uint32_t house,uint32_t id,int mission,uint32_t target,uint32_t cell,int targetKind,int extra1=0,int extra2=0,taskarbiter::Owner owner=taskarbiter::Owner::Generic,void*receipt=nullptr,int tag=-1,OrderAuthority=OrderAuthority::Local,bool append=false){
 actorFrameArbiter.begin(match,snap.frame);if(!actorFrameArbiter.canIssue(id,manualVersions.get(id)))return false;
 auto order=intent::mega(get<int>(house+0x30),id,mission,target,target?0x34:0,cell,uint8_t(targetKind),extra1,uint8_t(extra2));order.appendWaypoint=append;
 if(!submitConstructed(order))return false;actorFrameArbiter.record(id,manualVersions.get(id),owner);commands.push_back({id,cell});return true;
}
''' +text[end:]
    text+=r'''
static bool nativeReady(){return true;}static bool stickyOwns(uint32_t){return false;}
static bool nativeEnemy(uint32_t p,uint32_t h){return get<uint32_t>(p+0x21C)!=h;}
static bool GuardedEnterAction(uint32_t,uint32_t,uint32_t,unsigned*out){*out=5;return true;}
static struct {struct Capacity{int occupants=0,maximum=0,used=0;};bool entryCompatible(uint32_t,uint32_t,uint32_t){return false;}Capacity capacity(uint32_t){return {};}int entrySlots(uint32_t,uint32_t){return 1;}}clickView;
static bool IsWindow(HWND h){return h==1;}static int posted=0;static bool PostMessageW(HWND,UINT,WPARAM,LPARAM){++posted;return true;}
#include "mo_orders_bridge.h"
static int checks=0;static void check(bool b){++checks;assert(b);}
static void begin(){setup();resetMoOrders();events.clear();commands.clear();failAt=0;posted=0;actors.resize(1);selected=actors;sh->moFeatures=4;held['Z']=true;actorFrameArbiter.frame=-1;actorLeaseArbiter.allowed=true;moEpoch=match;moHouse=lastHouse;moWaypointSubmitted=false;}
static bool messageMo(UINT m,unsigned vk=0){MSG msg;msg.message=m;msg.wParam=vk;msg.lParam=MAKELPARAM(100,100);return moOrdersMessage(&msg);}
static void step(){++snap.frame;put(0xA8ED84,snap.frame);clockValue+=30;shared.heartbeat=clockValue;moOrdersTick(lastHouse,snap.frame,clockValue);}
static void graph(bool closed=false){uint32_t actor=actors[0],token=0x400000;put(actor+0x514,token);put(token,actor);put(token+8,0x410000u);put(token+12,moRouteCount);put(token+20,moRouteCount);put(token+0x8C,0);put(token+0x90,closed?moRouteCount:-1);put(token+0x94,closed?0:-1);
 put(0xAC4C78+4,0x420000u);put(0xAC4C78+8,1);put(0xAC4C78+16,1);put(0x420000,token);
 for(int i=0;i<moRouteCount;++i){uint32_t node=0x430000+i*0x100,member=node+0x40,event=node+0x60;put(0x410000+i*4,node);put(node+4,node+0x30);put(node+8,1);put(node+16,1);put(node+0x30,member);put(member,actor);put(member+4,event);put<uint8_t>(event,4);put(event+7,get<uint32_t>(actor+0x10));put<uint8_t>(event+11,0x34);put<uint8_t>(event+12,closed?29:2);put(event+19,uint32_t(moRoute[i].y*1000+moRoute[i].x));put<uint8_t>(event+23,11);put(event+24,uint32_t(moRoute[i].y*1000+moRoute[i].x));put<uint8_t>(event+28,11);}
}
static void submitRoute(){check(messageMo(WM_MBUTTONDOWN)&&moHolding);check(messageMo(WM_MBUTTONUP)&&moStage==1);for(int i=0;i<4;++i)step();check(moStage==2&&events.size()==4);}
int main(){
 begin();submitRoute();graph();step();check(moStage==4&&events.size()==5&&events.back().eventKind==vanilla::EventType::PlanConnect);graph(true);step();check(!moStage&&sh->moOrderStatus==2&&events.size()==6&&events.back().eventKind==vanilla::EventType::PlanCommit&&!moOrdersPending());
 // Every selected actor must begin with a verified empty plan.
 begin();moRouteCount=4;graph();check(messageMo(WM_MBUTTONDOWN)&&!moHolding&&int32_t(sh->moOrderStatus)==-17&&events.empty());
 begin();failAt=actors[0]+0x514;check(messageMo(WM_MBUTTONDOWN)&&!moHolding&&int32_t(sh->moOrderStatus)==-17&&events.empty());
 // Same node count with another destination is not this captured route.
 begin();submitRoute();graph();put(0x430000+0x60+19,25025u);step();check(moStage==2&&events.size()==4);
 // A closed flag alone is insufficient: native mission conversion must match.
 begin();submitRoute();graph();step();graph(true);put<uint8_t>(0x430000+0x60+12,2);step();check(moStage==4&&events.size()==5);put<uint8_t>(0x430000+0x60+12,29);step();check(events.size()==6&&!moStage);
 // Another own actor's pending graph blocks House-wide Commit.
 begin();submitRoute();graph();step();graph(true);put(0xAC4C78+8,2);put(0xAC4C78+16,2);put(0x420004,0x440000u);put(0x440000,0x450000u);put(0x450000+0x514,0x440000u);put(0x450000+0x21C,lastHouse);put(0x440000+0x8C,0);step();check(moStage==4&&events.size()==5);put(0x440000+0x8C,-1);step();check(!moStage&&events.size()==6);
 // Esc stops the remaining tail; no synthetic Commit or raw plan deletion.
 begin();check(messageMo(WM_MBUTTONDOWN));check(messageMo(WM_MBUTTONUP));step();check(events.size()==1);check(messageMo(WM_KEYDOWN,VK_ESCAPE));check(int32_t(sh->moOrderStatus)==-16&&!moOrdersPending()&&events.size()==1);
 // A later manual order invalidates the batch before another event is sent.
 begin();check(messageMo(WM_MBUTTONDOWN));check(messageMo(WM_MBUTTONUP));step();++manualVersions.tokens[get<uint32_t>(actors[0]+0x10)];step();check(int32_t(sh->moOrderStatus)==-16&&events.size()==1&&!moOrdersPending());
 // Directly manufactured PlanConnect is outside the closed runtime scope.
 begin();intent::EventOrder direct{};direct.domain=vanilla::Domain::Waypoints;direct.player=0;direct.eventKind=vanilla::EventType::PlanConnect;direct.a=100;direct.tagA=0x34;direct.b=30030;direct.tagB=11;check(!validateMoSubmission(direct));
 begin();submitRoute();graph();actorLeaseArbiter.allowed=false;step();check(moStage==3&&events.size()==4);actorLeaseArbiter.allowed=true;step();check(moStage==4&&events.size()==5);
 // Both fuzzy and area attacks use the same existing native mission gateway.
 for(int mode:{1,2}){begin();held.fill(false);held[mode==1?17:16]=true;sh->moFeatures=mode;uint32_t target=0x180000;actors.push_back(target);put(target+0x10,900u);put(target+0x21C,lastHouse+0x1000u);put(target+0x6C,100);put(target+0x6C4,0x190000u);put(readMapCell(30,30)+0xE4,target);check(messageMo(WM_MBUTTONDOWN)&&moHolding);check(messageMo(WM_MBUTTONUP)&&moQueueCount==1);step();check(events.size()==1&&events[0].mission==1&&events[0].b==900&&sh->moOrderAccepted==1&&!moOrdersPending());}
 begin();held.fill(false);held[17]=true;sh->moFeatures=3;sh->moGestures.keys[1]=17;check(messageMo(WM_MBUTTONDOWN)&&!moHolding&&int32_t(sh->moOrderStatus)==-14&&events.empty());
 std::cout<<"PASS "<<checks<<" actual MO bridge staged patrol/cancellation/admission checks\n";
}
'''
    text=text.replace('using HWND=uintptr_t','using HWND=void*').replace('HWND hwnd=1','HWND hwnd=reinterpret_cast<HWND>(1)').replace('GetForegroundWindow(){return 1;}','GetForegroundWindow(){return reinterpret_cast<HWND>(1);}').replace('return h==1;','return h==reinterpret_cast<HWND>(1);')
    return text.replace('@LAYOUT_HEADER@',json.dumps(str(HERE.parent/'source/formation.h')))
def run(directory,zig):
    cpp=directory/'mo_orders_bridge.cpp';exe=directory/('mo_orders_bridge.exe' if os.name=='nt' else 'mo_orders_bridge');cpp.write_text(source(),encoding='utf-8')
    compiler=[str(zig),'c++','-target','x86_64-windows-gnu'] if zig else ['g++']
    subprocess.run(compiler+['-std=c++17','-O2','-I'+str(HERE.parent/'source'),str(cpp),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True,timeout=30)
if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--zig',type=Path);parser.add_argument('--build-dir',type=Path);args=parser.parse_args()
    if args.build_dir:args.build_dir.mkdir(parents=True,exist_ok=True);run(args.build_dir,args.zig)
    else:
        with tempfile.TemporaryDirectory() as tmp:run(Path(tmp),args.zig)
