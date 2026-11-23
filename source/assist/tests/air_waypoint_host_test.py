"""Run the shipped host route recorder/service with controlled reads and receipts.
This checks hold/release, ordered continuation and cancellation, not game physics.
"""
from pathlib import Path
import subprocess, tempfile
from tactical_frame_sampling import function
r = Path(__file__).resolve().parents[2]
scope=function((r/'external_reader/source/process_reader.h').read_text(), 'class WorldFrameScope')+';'
air=(r/'assist/source/air_path_host.h').read_text().replace('#include "native_viewport_host.h"','')
program = r'''
#include "protocol.h"
#include "additional_settings.h"
#include "air_path_policy.h"
#include "fixtures/tactical_frame/sealed_host_reader.h"
#include <map>
#include <vector>
#include <string>
#include <cassert>
#include <algorithm>
#include <cstring>
using namespace assist;using DWORD=uint32_t;
static void fixtureService();
constexpr int VK_CONTROL=17;
static DWORD clockNow=1000;static DWORD GetTickCount(){return clockNow;}
static bool held[256]{};static short GetAsyncKeyState(int key){return held[key]?short(0x8000):0;}
static Snapshot snap{};static AdditionalSettings extra;static bool enabled=true;
static int connected=1;static int*shared=&connected;static uint32_t epoch=8,paintRevision=0;
static bool fresh=true;static uint32_t commandMatch(){return epoch;}static bool commandFresh(DWORD){return fresh;}
static std::wstring lastNotice;static unsigned diagnostics=0;
static void notify(const std::wstring&s){lastNotice=s;}static void critical(const std::wstring&){++diagnostics;}
struct Request{int command,arg,arg2;uint32_t match,token;};
static struct{Request slots[64]{};int size=0;void discard(int n){for(int j=n;j+1<size;++j)slots[j]=slots[j+1];--size;}}commandQueue;
static bool queueCommand(int c,int arg,int arg2,uint32_t match,int,DWORD,uint32_t token){
 if(commandQueue.size>=64)return false;commandQueue.slots[commandQueue.size++]={c,arg,arg2,match,token};return true;
}
static std::map<uint32_t,uint8_t> memory;
template<class T>static void put(uint32_t at,T value){auto*p=reinterpret_cast<uint8_t*>(&value);for(size_t n=0;n<sizeof value;++n)memory[at+n]=p[n];}
struct Reader:fixtureframe::SealedHostReader<Reader>{
 bool readLive(uint32_t at,void*out,size_t n){for(size_t j=0;j<n;++j){auto i=memory.find(at+j);if(i==memory.end())return false;static_cast<uint8_t*>(out)[j]=i->second;}return true;}
 void capture(uint32_t epoch,int frame){
  auto next=std::make_shared<worldframe::Snapshot>();worldframe::Key key{};key.match=epoch;key.frame=frame;next->reset(key);
  auto read=[&](uint32_t at,void*out,size_t n){return readLive(at,out,n);};
  for(auto at=memory.begin();at!=memory.end();){uint32_t start=at->first;size_t count=0;while(at!=memory.end()&&at->first==start+count){++at;++count;}next->prefetch(read,start,count);}
  assert(next->seal());available=std::move(next);
 }
}externalReader;
namespace nativekeys{constexpr int Planning=0;}
namespace nativekeyhost{
struct Binding{bool held(int,const uint8_t*keys)const{return keys['Z']!=0;}};
static Binding current(){return {};}
static void down(uint8_t*keys){for(int i=0;i<256;++i)keys[i]=::held[i];}
}
namespace gameinput{struct PointerPress{int operation=0;uint8_t keys[256]{};};}
namespace external{struct Projection{int x,y,w,h;};static void project(Projection,int x,int y,int,int&sx,int&sy){sx=x;sy=y;}}
namespace nativeviewhost{struct Transform{bool active=false;struct{int w=0,h=0;}logical;};template<class T>static bool sampleTransform(T*,Transform&){return true;}static bool localPoint(Transform,int&,int&){return true;}}
''' + 'namespace external {using ProcessReader=::Reader;'+scope+'}\n' + air + r'''
constexpr uint32_t air=0x40000,house=0x10000;
static void position(uint32_t cell){put<int>(air+0x9c,int(cell%1000)*256+128);put<int>(air+0xa0,int(cell/1000)*256+128);put<int>(air+0xa4,512);}
static pointerinput::Intent input;static pointerinput::Mailbox box;
static void initialize(int mode){
 airway::reset();memory.clear();commandQueue.size=0;epoch=8;snap.session=mode;extra.nativeAirWaypoints=false;extra.airWaypoints=true;extra.nativeAirNetwork=0;extra.airPathKey=0;
 for(unsigned n=0;n<0x800;++n)memory[air+n]=0;
 held['Z']=true;enabled=fresh=true;lastNotice.clear();input={};box={};
 put<uint32_t>(0xa83d4c,house);put<uint32_t>(air,0x7e22a4);put<uint32_t>(air+0x10,40);put<uint32_t>(air+0x21c,house);
 put<int>(air+0x6c,100);put<uint8_t>(air+0x6ca,0);put<uint8_t>(air+0x83,1);put<int>(air+0xac,5);put<uint32_t>(air+0x2b4,0);position(10010);
 put<uint32_t>(0xa8ecb8+4,0x30000);put<int>(0xa8ecb8+16,1);put<uint32_t>(0x30000,air);
 input.match=epoch;input.count=1;input.sources[0]={40,air};box.result=1;box.accepted=1;box.sourceAccepted[0]=1;
}
static void capture(uint32_t cell,uint32_t token){box.resolvedCell=cell;box.sourceTokens[0]=token;airway::capture(input,box);}
int main(){for(int mode:{3,4}){
 initialize(mode);assert(airway::active()&&!airway::native()&&airway::selectedAircraft());
 capture(30020,1);capture(30030,2);assert(airway::jobs.size()==1&&airway::jobs[0].points.size()==2);
 airway::service();assert(commandQueue.size==0); // Z is still held.
 held['Z']=false;airway::service();assert(commandQueue.size==1);
 auto q=commandQueue.slots[0];assert(q.command==AirPathMove&&q.arg2==30020&&q.token==2);commandQueue.discard(0);
 airway::result(1,1,40,2);clockNow+=20;airway::service();assert(commandQueue.size==0); // accepted is not arrived
 position(30020);airway::service();assert(commandQueue.size==1&&commandQueue.slots[0].arg2==30030);
 commandQueue.discard(0);airway::result(1,1,40,2);position(30030);airway::service();assert(airway::jobs.empty());
 initialize(mode);box.result=-4;unsigned n=diagnostics;airway::capture(input,box);assert(airway::jobs.empty()&&!lastNotice.empty()&&diagnostics==n+1);
 initialize(mode);capture(30020,3);box.resolvedTarget=50;airway::capture(input,box);assert(airway::jobs.empty()&&airway::ended);
 initialize(mode);capture(30020,4);gameinput::PointerPress e;e.operation=pointerinput::Cancel;e.keys['S']=1;airway::manual(e);assert(airway::jobs.empty());
 initialize(mode);capture(30020,5);++epoch;held['Z']=false;airway::service();assert(airway::jobs.empty()&&commandQueue.size==0);
 // Automatic acquisition must not erase the next waypoint.
 initialize(mode);capture(30020,8);capture(30030,8);held['Z']=false;airway::service();commandQueue.discard(0);airway::result(1,1,40,8);
 put<int>(air+0xac,1);put<uint32_t>(air+0x2b4,0x50000);airway::service();assert(airway::jobs.size()==1&&airway::jobs[0].points.size()==2);
 position(30020);airway::service();assert(commandQueue.size==1&&commandQueue.slots[0].arg2==30030);
 // More than three temporary queue failures retain the route.
 initialize(mode);capture(30020,9);held['Z']=false;
 for(int retry=0;retry<5;++retry){airway::service();assert(commandQueue.size==1);commandQueue.discard(0);airway::result(-6,0,40,9);clockNow+=200;assert(airway::jobs.size()==1);}
 airway::service();assert(commandQueue.size==1);commandQueue.discard(0);airway::result(1,1,40,9);position(30020);airway::service();assert(airway::jobs.empty());
 // A high-speed ground segment crosses between two host samples.
 initialize(mode);capture(30020,10);capture(30030,10);position(30017);held['Z']=false;airway::service();commandQueue.discard(0);airway::result(1,1,40,10);
 put<int>(air+0xac,2);put<uint32_t>(air+0x5a4,0x60000);put<int16_t>(0x60024,20);put<int16_t>(0x60026,30);airway::service();
 position(30023);airway::service();assert(commandQueue.size==1&&commandQueue.slots[0].arg2==30030);
 initialize(mode);capture(30020,11);held['Z']=false;e={};e.operation=7;airway::manual(e);assert(airway::jobs.size()==1);
 e.operation=4;airway::manual(e);assert(airway::jobs.empty());
 // Return-to-base before arrival retries the same point and retains the rest.
 initialize(mode);capture(30020,12);capture(30030,12);held['Z']=false;airway::service();commandQueue.discard(0);airway::result(1,1,40,12);
 put<int>(air+0xac,2);put<uint32_t>(air+0x5a4,0x60000);put<int16_t>(0x60024,20);put<int16_t>(0x60026,30);airway::service();
 put<int>(air+0xac,7);put<uint32_t>(air+0x5a4,0x70000);clockNow+=300;airway::service();clockNow+=200;airway::service();
 assert(commandQueue.size==1&&commandQueue.slots[0].arg2==30020&&airway::jobs[0].points.size()==2);
 // A stalled aircraft does not reset its no-progress timer on every ACK.
 initialize(mode);capture(30020,13);held['Z']=false;
 for(int n=0;n<40&&!airway::jobs.empty();++n){airway::service();if(commandQueue.size){commandQueue.discard(0);airway::result(1,1,40,13);}clockNow+=1000;}
 assert(airway::jobs.empty());
}
 initialize(5);assert(!airway::native()&&airway::active());capture(30020,1);assert(airway::jobs.size()==1); // explicit compatibility mode in SP too
 initialize(5);extra.nativeAirWaypoints=true;extra.nativeAirNetwork=1;assert(airway::native()&&!airway::active());capture(30020,1);assert(airway::jobs.empty());
 for(int mode:{3,4})for(unsigned experiment:{1u,2u}){
  initialize(mode);extra.nativeAirWaypoints=true;extra.nativeAirNetwork=experiment;
  assert(airway::native()&&!airway::active());capture(30020,1);held['Z']=false;airway::service();
  assert(airway::jobs.empty()&&commandQueue.size==0); // native click has one owner
  assert(airpathpolicy::arrival(mode,true,experiment)==(experiment==2));
  extra.airPathKey='P';assert(!airway::native()&&airway::active()); // explicit custom key remains compatible
 }
 for(int session:{1,2,6})for(unsigned experiment:{0u,1u,2u,3u}){
  assert(!airpathpolicy::native(session,true,experiment));
  assert(!airpathpolicy::arrival(session,true,experiment));
  assert(!airpathpolicy::events(session,true,true,experiment));
 }
}
'''
# Each legacy service step represents a new simulated logic frame. The separate
# host_tactical_frame_sampling fixture covers repeated same-frame observations.
program=program.replace('airway::service();','fixtureService();')
program+='\nstatic void fixtureService(){externalReader.capture(epoch,++snap.frame);airway::service();}\n'
with tempfile.TemporaryDirectory() as d:
 p = Path(d)/'air.cpp'; exe = Path(d)/'air'; p.write_text(program)
 subprocess.run(['g++', '-std=c++17', '-O2', '-I', str(r/'assist/source'), '-I', str(r/'assist/tests'), str(p), '-o', str(exe)], check=True)
 subprocess.run([str(exe)], check=True)
print('PASS: shipped host records ordered points, continues within 16 steps, waits for Z release and observed arrival, preserves newest token, reports rejection, cancels attack/stop/new-match, keeps SP compatibility/native modes')

