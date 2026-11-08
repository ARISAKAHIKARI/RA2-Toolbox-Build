// The production ProcessReader/IPC and extracted Host function bodies execute.
// Only process bytes, Win32 I/O, keyboard, paint worker and command queue are fake.
#include "protocol.h"
#include "construction_catalog.h"
#include "../../shared/source/world_frame_capture.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <climits>
#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <map>
#include <deque>
#include "host_platform.h"
using namespace assist;
static external::ProcessReader externalReader;
static worldframe::Server frameServer;
static fixture::Memory memory;
static Snapshot snap{};
static uint32_t pid=fixture::GamePid,match=7,paintRevision=0,serial=0;
static bool enabled=true,fresh=true,routeActive=true,routeHeld=false,displayFresh=true;
static unsigned heldCalls=0,notifications=0,paintSubmissions=0,paintInvalidations=0,windowHides=0;
static std::function<void()> duringHeld;
static int sharedValue=1;static int*shared=&sharedValue;
static uint32_t commandMatch(){return match;}
static bool commandFresh(DWORD){return fresh;}
static void notify(const std::wstring&){++notifications;}
struct QueuedCommand {int command,arg,arg2;uint32_t match,token;};
static struct Queue {QueuedCommand slots[64]{};int size=0;void discard(int n){for(int i=n;i+1<size;++i)slots[i]=slots[i+1];--size;}} commandQueue;
static bool queueCommand(int command,int arg,int arg2,uint32_t epoch,int,DWORD,uint32_t token){
 CHECK(!externalReader.worldFrameActive());if(commandQueue.size>=64)return false;
 commandQueue.slots[commandQueue.size++]={command,arg,arg2,epoch,token};return true;
}
namespace airway {
#include "host_air_production.h"
}
using HFONT=void*;
struct LOGFONTW {int height=12;unsigned char fixtureData[88]{};};
constexpr int SW_HIDE=0;
static HFONT font=nullptr;
static int GetObjectW(HFONT,int n,void*out){std::memset(out,0,size_t(n));return n;}
static void ShowWindow(HWND,int){CHECK(!externalReader.worldFrameActive());++windowHides;}
static bool constructionDisplayFresh(){return displayFresh;}
static bool techMode=false;
static bool techAllowed(const assist::Type&){return true;}
namespace buildreserve {
#include "host_panel_declarations.h"
static construction::Book books[4];static std::vector<Choice> choices;
static HWND window=reinterpret_cast<HWND>(1);static uint32_t current[4]{},house=fixture::House;
static int activeTab=2,scroll=0,hoverChoice=-1,hoverQueue=-1;
static construction::Layout layout;
static HFONT panelFont=nullptr;static bool geometryReady=true,paintReady=false;
static std::shared_ptr<const Painted> paintedFrame;
static std::shared_ptr<const PaintRequest> requestedPaint;
static uint32_t requestedAt=0,paintRetryAt=0;
static struct Worker {
 void submit(const PaintRequest&request){CHECK(!externalReader.worldFrameActive());CHECK(request.sourceFrame&&request.sourceFrame->sealed());++paintSubmissions;}
 void invalidate(){CHECK(!externalReader.worldFrameActive());++paintInvalidations;}
} paintWorker;
static struct Buffer {void clear(){}} panelBuffer,panelSpare;
#include "host_panel_production.h"
}
static void seal(uint32_t omitted=0,bool failed=false){
 CHECK(!externalReader.worldFrameActive());
 externalReader.setWorldDemand(worldframe::Base|worldframe::Combat);
 externalReader.beginWorldFrame(match);externalReader.endWorldFrame();
 fixture::currentPid=fixture::GamePid;CHECK(frameServer.wanted(fixture::HostPid,fixture::clock));
 auto key=fixture::key(++snap.frame);key.match=match;key.serial=++serial;key.tick=fixture::clock;
 key.connectionLow=uint32_t(frameServer.connection());key.connectionHigh=uint32_t(frameServer.connection()>>32);key.generationLow=77;
 worldframe::Snapshot frame;frame.reset(key);const uint32_t page=omitted&~4095u;
 auto read=[&](uint32_t at,void*out,size_t n){return (!page||at+n<=page||at>=page+4096)&&memory.read(at,out,n);};
 for(uint32_t at=0x10000;at<memory.bytes.size();at+=4096){if(at==page&&!failed)continue;frame.prefetch(read,at,4096);}
 CHECK(frame.seal());frameServer.observe(frame.key);frameServer.publish(frame);
 fixture::currentPid=fixture::HostPid;memory.calls.clear();fixture::rpmCalls=0;heldCalls=0;
}
static void reset(){
 CHECK(!externalReader.worldFrameActive());
 fixture::currentPid=fixture::GamePid;frameServer.retire();frameServer.disconnect();
 fixture::currentPid=fixture::HostPid;externalReader.close();memory.bytes.assign(memory.bytes.size(),0);memory.calls.clear();memory.beforeRead={};memory.denied.clear();memory.partialPages.clear();fixture::gameMemory=&memory;
 fixture::process=std::make_shared<fixture::ProcessRecord>(fixture::ProcessRecord{fixture::GamePid,12345u+serial,true});
 CHECK(externalReader.open(fixture::GamePid));fixture::clock+=20;++match;snap={};snap.frame=100;snap.chat=0;
 enabled=fresh=routeActive=displayFresh=true;routeHeld=false;duringHeld={};notifications=heldCalls=paintRevision=0;commandQueue.size=0;airway::jobs.clear();airway::cursor=0;
 buildreserve::requestedPaint.reset();buildreserve::paintedFrame.reset();buildreserve::requestedAt=buildreserve::paintRetryAt=0;buildreserve::choices.clear();for(auto&book:buildreserve::books)book={};for(auto&p:buildreserve::current)p=0;
 buildreserve::activeTab=2;buildreserve::house=fixture::House;buildreserve::geometryReady=true;buildreserve::paintReady=false;buildreserve::scroll=0;paintSubmissions=paintInvalidations=windowHides=0;
 memory.put(0xA83D4C,fixture::House);
}
constexpr uint32_t Aircraft=fixture::ActorA;
static void plane(uint32_t id=40,int x=1000,int y=1000){
 memory.put(Aircraft,0x7E22A4u);memory.put(Aircraft+0x10,id);memory.put(Aircraft+0x21C,fixture::House);memory.put(Aircraft+0x6C,100);
 memory.put(Aircraft+0x83,uint8_t(0));memory.put(Aircraft+0x6CA,uint8_t(0));memory.put(Aircraft+0x9C,x);memory.put(Aircraft+0xA0,y);memory.put(Aircraft+0xA4,500);memory.put(Aircraft+0xAC,5);
}
static void job(uint32_t id=40,int stage=0){
 airway::Job j;j.id=id;j.pointer=Aircraft;j.match=match;j.token=3;j.stage=stage;j.x=7;j.y=8;j.z=9;j.selected=false;j.sent=j.progress=j.firstIssued=fixture::clock;
 j.points.push_back({30020,500});airway::jobs.push_back(j);
}
static void serviceAir(){heldCalls=0;airway::service();CHECK(!externalReader.worldFrameActive());}
static void testAir(){
 reset();plane();job();seal();memory.put(Aircraft+0x9C,9000);memory.put(Aircraft+0xA0,8000);memory.put(Aircraft+0x83,uint8_t(1));routeHeld=true;serviceAir();
 CHECK(airway::jobs.size()==1&&airway::jobs[0].x==1000&&airway::jobs[0].y==1000&&airway::jobs[0].selected&&commandQueue.size==0);
 for(const auto&call:memory.calls)CHECK(call.address!=Aircraft+0x9C&&call.address!=Aircraft+0xA0&&call.address!=Aircraft+0xAC);
 reset();plane();job(40,2);seal(Aircraft);const auto cursor=airway::cursor;serviceAir();
 CHECK(airway::jobs.size()==1&&airway::jobs[0].stage==2&&airway::jobs[0].x==7&&airway::jobs[0].points.size()==1&&airway::cursor==cursor&&commandQueue.size==0&&fixture::rpmCalls==0);
 seal(Aircraft,true);serviceAir();CHECK(airway::jobs.size()==1&&airway::jobs[0].stage==2&&airway::jobs[0].x==7&&commandQueue.size==0&&fixture::rpmCalls==0);
 reset();plane(40,20*256+128,30*256+128);seal();plane(41,500,500);job(41,2);serviceAir();
 CHECK(airway::jobs.size()==1&&airway::jobs[0].id==41&&airway::jobs[0].points.size()==1&&airway::jobs[0].stage==2&&airway::jobs[0].x==7&&commandQueue.size==0);
 for(uint32_t unknown:{Aircraft+0x10,Aircraft+0x6C,Aircraft+0x83,0xA83D4Cu}){
  reset();plane();job(40,2);seal();memory.denied.push_back({unknown,1});serviceAir();
  CHECK(airway::jobs.size()==1&&airway::jobs[0].stage==2&&airway::jobs[0].x==7&&airway::jobs[0].points.size()==1&&commandQueue.size==0);
 }
 reset();plane();job();seal();duringHeld=[] {memory.put(Aircraft+0x10,41u);};serviceAir();CHECK(airway::jobs.empty()&&commandQueue.size==0);
 reset();plane();job();seal();memory.beforeRead=[](uint32_t at,size_t){if(at==Aircraft+0x6C)memory.put(Aircraft+0x10,41u);};serviceAir();CHECK(airway::jobs.empty()&&commandQueue.size==0);memory.beforeRead={};
 reset();plane();job();seal();duringHeld=[] {memory.denied.push_back({Aircraft+0x10,4});};serviceAir();CHECK(airway::jobs.size()==1&&airway::jobs[0].stage==0&&airway::jobs[0].points.size()==1&&commandQueue.size==0);
 reset();plane();job();seal();serviceAir();CHECK(commandQueue.size==1&&commandQueue.slots[0].arg==40&&commandQueue.slots[0].token==3&&airway::jobs[0].stage==1);
 reset();plane();job();seal();memory.put(Aircraft+0x6C,0);serviceAir();CHECK(airway::jobs.empty()&&commandQueue.size==0);
 reset();plane();job(40,2);seal();fixture::currentPid=fixture::GamePid;frameServer.retire();fixture::currentPid=fixture::HostPid;serviceAir();CHECK(airway::jobs.size()==1&&airway::jobs[0].x==7&&commandQueue.size==0);
 std::puts("PASS air-path sealed position/current UI, missing-data retention, sampled/live identities and final enqueue guard");
}
constexpr uint32_t BuildType=0x300100,InfType=0x310100,InfList=0x620000,Country=0x320100,Rules=0x330100;
static uint32_t sidebar(){return 0x87F7E8+0x1544+2*3988;}
static void constructionSetup(){
 reset();snap.typeCount=2;snap.types[0].kind=7;snap.types[0].index=0;std::memcpy(snap.types[0].id,"FACTORY",8);snap.types[1].kind=16;snap.types[1].index=0;std::memcpy(snap.types[1].id,"INFANTRY",9);
 memory.put(fixture::House+0x34,Country);memory.put(fixture::House+0x1D4,10);memory.put(0x8871E0,Rules);memory.put(Country+0xB8,0);memory.put(Country+0xB4,0);
 memory.vector(0xA83C68,0x610000,{BuildType});memory.vector(0xA8E348,InfList,{InfType});memory.vector(fixture::House+0x5550,0x630000,{1});memory.vector(fixture::House+0x5500,0x640000,{1});
 memory.put(BuildType+0xEB8,16);memory.put(InfType+0x6CC,1u);memory.put(InfType+0xDA0,UINT32_MAX);memory.put(InfType+0xDA4,UINT32_MAX);
 memory.put(sidebar()+84,1);memory.put(sidebar()+88,0);memory.put(sidebar()+92,16);
 CHECK(buildreserve::layout.set(1024,768,1024,768));buildreserve::layout.content(0,1);seal();
}
static void testConstruction(){
 constructionSetup();buildreserve::collectChoices();CHECK(buildreserve::choices.size()==1&&buildreserve::choices[0].type==InfType&&!buildreserve::choices[0].future);
 CHECK(memory.calls.size()==3&&memory.calls[0].address==sidebar()+84&&memory.calls[1].address==sidebar()+88&&memory.calls[2].address==sidebar()+84);
 auto original=buildreserve::choices[0];memory.put(sidebar()+84,0);memory.denied.push_back({sidebar()+84,4});buildreserve::collectChoices();
 CHECK(buildreserve::choices.size()==1&&buildreserve::choices[0].type==original.type&&!buildreserve::choices[0].future);memory.denied.clear();
 memory.put(sidebar()+84,1);seal(InfList);buildreserve::collectChoices();CHECK(buildreserve::choices.size()==1&&buildreserve::choices[0].type==original.type);
 memory.put(InfList,0x340100u);buildreserve::requestPaint();CHECK(!buildreserve::requestedPaint&&!paintSubmissions&&!windowHides);
 memory.put(InfList,InfType);seal();buildreserve::requestPaint();CHECK(buildreserve::requestedPaint&&paintSubmissions==1&&buildreserve::requestedPaint->pointers.at(16<<16)==InfType);
 auto request=buildreserve::requestedPaint;const auto stamp=buildreserve::requestedAt;seal(InfList,true);buildreserve::hoverChoice=2;buildreserve::requestPaint();
 CHECK(buildreserve::requestedPaint==request&&buildreserve::requestedAt==stamp&&paintSubmissions==1&&paintInvalidations==0&&windowHides==0);
 fixture::currentPid=fixture::GamePid;frameServer.retire();fixture::currentPid=fixture::HostPid;buildreserve::collectChoices();buildreserve::requestPaint();
 CHECK(buildreserve::choices.size()==1&&buildreserve::requestedPaint==request&&paintSubmissions==1);
 // A coherent, successfully read empty native strip remains a real update.
 seal();memory.put(sidebar()+84,0);buildreserve::collectChoices();CHECK(buildreserve::choices.empty());
 CHECK(!externalReader.worldFrameActive());std::puts("PASS construction shared catalogue/paint tokens, live sidebar and failed-choice/request retention");
}
int main(){testAir();testConstruction();std::printf("PASS %zu Host tactical checks through production ProcessReader/IPC (synthetic CPU fixture)\n",fixture::checks);}
