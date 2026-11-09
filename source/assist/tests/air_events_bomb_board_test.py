"""Execute production native-air input and bomb boarding against controlled boundaries.
No Windows message loop, game physics or multiplayer peer is simulated.
"""
from pathlib import Path
import argparse, subprocess, tempfile
from mo_orders_bridge_test import source as mo_source

def source():
    text=mo_source().split('int main(){',1)[0]
    text=text.replace('#include "constructor_adapter.h"', '#include "constructor_adapter.h"\n#include "air_path_policy.h"\n#include "air_path_events.h"')
    text=text.replace('static bool validateMoSubmission(const intent::EventOrder&);','static bool validateMoSubmission(const intent::EventOrder&);\nstatic bool validateDirectAirSubmission(const intent::EventOrder&);\nstatic bool validateBombBoardSubmission(const intent::EventOrder&);\nstatic void(*beforeSubmit)()=nullptr;')
    text=text.replace('static int kind(uint32_t p){return worldObjects.contains(p)?1:0;}', 'static std::map<uint32_t,int> kinds;static int kind(uint32_t p){return kinds.count(p)?kinds[p]:1;}')
    text=text.replace('struct Vec{int count=0;};static Vec vec(uint32_t p){assert(p==0xA8ECB8);return {int(selected.size())};}\nstatic uint32_t item(Vec,int n){return selected.at(n);}', 'struct Vec{int count=0;bool all=false;};static Vec vec(uint32_t p){return {int(p==0xA8EC78?actors.size():selected.size()),p==0xA8EC78};}\nstatic uint32_t item(Vec v,int n){return v.all?actors.at(n):selected.at(n);}')
    text=text.replace('volatile long moOrdersPreviewSeq=0;', 'volatile long airEventSeq=0;airevents::Report airEventReport;uint32_t flags=0,nativeAirNetwork=0,bombBoardEnabled=0,bombBoardRangeCells=8,bombBoardActive=0,bombBoardIssued=0,bombBoardWaiting=0;volatile long moOrdersPreviewSeq=0;')
    text=text.replace('Formation,Generic}', 'Formation,Generic,BombBoard}')
    text=text.replace('if(!validateMoSubmission(o)||constructors::admit(o)', 'if(beforeSubmit)beforeSubmit();if(!validateDirectAirSubmission(o)||!validateBombBoardSubmission(o)||!validateMoSubmission(o)||constructors::admit(o)')
    text=text.replace('static int outgoingRoom(){return 256-int(commands.size());}', 'static int queueRoom=256;static int outgoingRoom(){return queueRoom-int(commands.size());}')
    start=text.index('static struct {struct Capacity')
    end=text.index('#include "cooperative_types.h"',start)
    text=text[:start]+r'''
static struct {struct Capacity{int occupants=0,maximum=0,used=0;};
 bool entryCompatible(uint32_t,uint32_t target,uint32_t){return kind(target)==2;}
 Capacity capacity(uint32_t target){return {0,get<int>(typeOf(target,2)+0x5E0),get<int>(target+0x114)};}
 int entrySlots(uint32_t,uint32_t){return 1;}uint32_t entryDestination(uint32_t p){return get<uint32_t>(p+0x5A4);}
}clickView;
''' + text[end:]
    text=text.replace('static int entryReserved(uint32_t,uint32_t,uint32_t){return 0;}', 'static int entryReserved(uint32_t target,uint32_t ignore,uint32_t){int n=0;for(int i=0;i<entryReservationCount;++i){auto&r=entryReservations[i];if(r.id!=ignore&&r.target==get<uint32_t>(target+0x10))n+=r.slots;}return n;}')
    text+=r'''
constexpr uint32_t NativeAirWaypoints=0x80000000u;
#include "air_path_events_bridge.h"
struct SampleReadScope{static bool available;explicit operator bool()const{return available;}bool complete()const{return available;}};bool SampleReadScope::available=true;
static bool sampleRoster(uint32_t at,Vec&v){v=vec(at);return true;}
static uint32_t incomingVehicle(uint32_t p){return get<uint32_t>(p+0x5A4);}
static struct {struct Reservation{uint32_t id=0,vehicle=0,object=0;int slots=1;}records[16];int count=0;bool contains(uint32_t id){for(int i=0;i<count;++i)if(records[i].id==id)return true;return false;}}loadLedger;
static bool loadQueryFailed=false;static std::set<uint32_t> illegal;
static bool canEnter(uint32_t,uint32_t target){return !illegal.count(target);}
#include "bomb_board_bridge.h"
static bool airMessage(UINT m,unsigned vk=0,int x=100,int y=100){MSG msg;msg.message=m;msg.wParam=vk;msg.lParam=MAKELPARAM(x,y);return directair::message(&msg);}
static void keyTable(){
 put(0x87F680,0x200000u);put(0x87F684,1u);put(0x87F688,1u);put(0x200000,uint32_t('Z'));put(0x200004,0x210000u);put(0x210000,0x220000u);put(0x220004,0x600000u);
 const uint8_t getName[]={0xB8,0,0,0x90,0,0xC3};std::memcpy(memory.data()+0x600000,getName,6);std::memcpy(memory.data()+0x900000,"PlanningMode",13);
}
static void airBegin(){begin();directair::reset();directair::planning={};beforeSubmit=nullptr;events.clear();posted=0;queueRoom=256;sh->moFeatures=0;sh->flags=NativeAirWaypoints;kinds.clear();kinds[actors[0]]=0;held['Z']=true;keyTable();put(0x87F7E8+0x11B3,true);}
static void airStep(){++snap.frame;put(0xA8ED84,snap.frame);clockValue+=30;sh->heartbeat=clockValue;directair::tick(lastHouse,snap.frame,clockValue);}
static uint32_t unit(unsigned id,int k,int x,int y){uint32_t p=0x10000+id*0x1000,t=0x500000+id*0x2000;actors.push_back(p);kinds[p]=k;put(p+0x10,id);put(p+0x21C,lastHouse);put(p+0x6C,100);put(p+0x6C4,t);put(p+0x9C,x*256);put(p+0xA0,y*256);if(k==2)put(t+0x5E0,1);return p;}
static void bombBegin(){begin();bombboard::reset();directair::reset();directair::planning={};beforeSubmit=nullptr;kinds.clear();actors.clear();selected.clear();events.clear();commands.clear();illegal.clear();entryReservationCount=loadLedger.count=0;queueRoom=256;SampleReadScope::available=true;sh->bombBoardEnabled=1;}
static void bombStep(){++snap.frame;put(0xA8ED84,snap.frame);clockValue+=30;sh->heartbeat=clockValue;bombboard::tick(lastHouse,snap.frame);}
int main(){
 for(int session:{0,5,3,4}){
  airBegin();put(0xA8B238,session);snap.session=session;
  check(airMessage(WM_LBUTTONDOWN));check(airMessage(WM_LBUTTONUP));check(airMessage(WM_LBUTTONDOWN,0,160,100));check(airMessage(WM_LBUTTONUP,0,160,100));
  check(directair::pointCount==2&&events.empty());held['Z']=false;check(airMessage(WM_KEYUP,'Z')&&posted==0);
  queueRoom=0;airStep();check(events.empty()&&posted==0&&directAirPending());queueRoom=256;
  airStep();check(events.size()==1&&events[0].appendWaypoint&&events[0].mission==2&&events[0].c==30030&&posted==0);
  airStep();check(events.size()==2&&events[1].appendWaypoint&&events[1].c==30032&&posted==1&&!directAirPending());
  check(sh->airEventReport.status==airevents::Committed&&sh->airEventReport.points==2&&sh->airEventReport.accepted==2&&feedbacks==2);
  airStep();check(events.size()==2&&posted==1); // no synthetic Commit and no arrival resend
 }
 airBegin();check(airMessage(WM_LBUTTONDOWN));check(airMessage(WM_LBUTTONUP));beforeSubmit=[](){++manualVersions.tokens[get<uint32_t>(actors[0]+0x10)];beforeSubmit=nullptr;};airStep();check(events.empty());airStep();check(!directAirPending());
 airBegin();airMessage(WM_LBUTTONDOWN);airMessage(WM_LBUTTONUP);held['Z']=false;airMessage(WM_KEYUP,'Z');++match;airStep();check(posted==0&&events.empty());
 airBegin();kinds[actors[0]]=1;check(!airMessage(WM_LBUTTONDOWN)&&!directAirPending()&&events.empty());
 airBegin();airMessage(WM_LBUTTONDOWN);airMessage(WM_LBUTTONUP);airMessage(WM_KEYDOWN,VK_ESCAPE);check(events.empty()&&!directAirPending());
 airBegin();held['Z']=false;held['S']=true;put(0x200000,uint32_t('S'));airMessage(WM_LBUTTONDOWN);airMessage(WM_LBUTTONUP);airMessage(WM_KEYDOWN,'S');check(directAirPending());held['S']=false;check(airMessage(WM_KEYUP,'S'));airStep();check(events.size()==1&&posted==1);
 // Nearest legal vehicle; full and incompatible vehicles cannot absorb seats.
 bombBegin();uint32_t p=unit(1,1,20,20),near=unit(2,2,21,20),far=unit(3,2,24,20);put(p+0x38,0x700000u);illegal.insert(near);bombStep();check(events.size()==1&&events[0].mission==7&&events[0].c==3&&!events[0].appendWaypoint&&entryReservationCount==1&&sh->bombBoardIssued==1);bombStep();check(events.size()==1);
 bombBegin();p=unit(1,1,20,20);near=unit(2,2,21,20);far=unit(3,2,24,20);put(p+0x38,0x700000u);put(near+0x114,1);bombStep();check(events.size()==1&&events[0].c==3);
 // Two bombed infantry cannot overbook a one-seat transport.
 bombBegin();p=unit(1,1,20,20);uint32_t q=unit(4,1,20,21);near=unit(2,2,21,20);put(p+0x38,0x700000u);put(q+0x38,0x710000u);bombStep();check(events.size()==1&&entryReservationCount==1);
 // Existing AutoBoard holds its seat before its event reaches the receiver.
 bombBegin();p=unit(1,1,20,20);q=unit(4,1,20,21);near=unit(2,2,21,20);put(p+0x38,0x700000u);loadLedger.count=1;loadLedger.records[0]={4,near,q,1};bombStep();check(events.empty());loadLedger.count=0;bombStep();check(events.size()==1);
 // No target, radius limit, then a newly available vehicle; token change
 // latches manual ownership for this bomb even if it later stays idle.
 bombBegin();p=unit(1,1,20,20);near=unit(2,2,30,20);put(p+0x38,0x700000u);bombStep();check(events.empty());put(near+0x9C,21*256);++manualVersions.tokens[1];bombStep();check(events.empty()&&bombboard::jobs[0].cancelled);put(p+0x38,0x720000u);bombStep();check(events.size()==1);
 bombBegin();p=unit(1,1,20,20);near=unit(2,2,21,20);put(p+0x38,0x700000u);SampleReadScope::available=false;bombStep();check(events.empty()&&bombboard::jobs.empty());SampleReadScope::available=true;bombStep();check(events.size()==1);
 // Final queue guard rechecks a bomb removed across an external callback.
 bombBegin();p=unit(1,1,20,20);near=unit(2,2,21,20);put(p+0x38,0x700000u);beforeSubmit=[](){put(actors[0]+0x38,0u);beforeSubmit=nullptr;};bombStep();check(events.empty()&&!entryReservationCount);bombStep();check(bombboard::jobs.empty());
 bombBegin();p=unit(1,1,20,20);near=unit(2,2,21,20);put(p+0x38,0x700000u);queueRoom=0;bombStep();check(events.empty());queueRoom=256;bombStep();check(events.size()==1);sh->bombBoardEnabled=0;bombStep();check(bombboard::jobs.empty());
 // A transient unavailable sample/pause retains manual cancellation for
 // this bomb; resuming cannot recapture its current token as a new job.
 bombBegin();p=unit(1,1,20,20);near=unit(2,2,30,20);put(p+0x38,0x700000u);bombStep();++manualVersions.tokens[1];bombStep();check(bombboard::jobs[0].cancelled);SampleReadScope::available=false;bombStep();SampleReadScope::available=true;put(near+0x9C,21*256);bombStep();check(events.empty()&&bombboard::jobs[0].cancelled);
 bombBegin();p=unit(1,1,20,20);near=unit(2,2,21,20);put(p+0x38,0x700000u);put(near+0x6C4,0u);bombStep();check(events.empty());
 std::cout<<"PASS "<<checks<<" native-air and bomb boarding checks; no real game or network peer\n";
}
'''
    return text

def run(out):
    out.mkdir(parents=True,exist_ok=True);cpp=out/'air-events-bomb-board.cpp';exe=out/'air-events-bomb-board'
    cpp.write_text(source())
    subprocess.run(['g++','-std=c++17','-O2','-UNDEBUG','-I'+str(Path(__file__).resolve().parents[1]/'source'),str(cpp),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True,timeout=30)
if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--out',type=Path);args=parser.parse_args()
    if args.out:run(args.out)
    else:
        with tempfile.TemporaryDirectory() as tmp:run(Path(tmp))
