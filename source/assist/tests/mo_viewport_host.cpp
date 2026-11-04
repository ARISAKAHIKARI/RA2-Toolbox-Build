#ifdef NDEBUG
#undef NDEBUG
#endif
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <memory>
#include <vector>
#include "../source/native_viewport_host.h"
#include "../source/render_presentation.h"
#include "../source/label_motion.h"
#include "../source/passenger_motion.h"
#include "../source/superweapon_sidebar_model.h"
#include "../../external_reader/source/tactical_sampler.h"
#include "../../external_reader/source/feedback_selection.h"
#include BASELINE_VIEWPORT
#include BASELINE_SAMPLER
using namespace assist;namespace nv=nativeviewportprojection;
struct SharedView {int32_t viewportSerial=0;uint32_t viewportActive=0,viewportGeneration=0,viewportHostCapabilities=0;int32_t viewportPhysical[4]{},viewportLogical[4]{};int32_t viewZoomBusy=0;};
static SharedView sh;static int checks=0;static size_t reads=0;static uint32_t failAt=0;
static std::function<void(uint32_t)>onRead;static std::vector<uint8_t>memory(32*1024*1024);
template<class T>static void put(uint32_t at,const T&v){assert(uint64_t(at)+sizeof v<=memory.size());memcpy(memory.data()+at,&v,sizeof v);}
static void vectorAt(uint32_t at,uint32_t items,const std::vector<uint32_t>&values){put(at,std::array<uint32_t,6>{{0,items,uint32_t(values.size()),0x101,uint32_t(values.size()),10}});for(size_t i=0;i<values.size();++i)put(items+uint32_t(i)*4,values[i]);}
struct Read {bool operator()(uint32_t at,void*out,size_t n)const{++reads;if(onRead)onRead(at);if(uint64_t(at)+n>memory.size()||(failAt>=at&&uint64_t(failAt)<uint64_t(at)+n))return false;memcpy(out,memory.data()+at,n);return true;}bool fresh(uint32_t at,void*out,size_t n)const{return (*this)(at,out,n);}};
static void check(bool yes){assert(yes);++checks;}
static void publish(nv::Transform t){++sh.viewportSerial;sh.viewportActive=t.active;sh.viewportGeneration=t.generation;for(int i=0;i<4;++i){sh.viewportPhysical[i]=(&t.physical.x)[i];sh.viewportLogical[i]=(&t.logical.x)[i];}++sh.viewportSerial;}
static void setup(){
 std::fill(memory.begin(),memory.end(),0);sh={};reads=0;failAt=0;onRead={};
 put(0xA8B230,0x30000u);put(0xA83D4C,0x40000u);put(0x887324,0x50000u);put(0xA8ED84,int32_t(400));put(0xA8E9A0,uint8_t(1));put(0xA8E378,uint8_t(1));put(0x8A00A4,int32_t(800));put(0x8A00A8,int32_t(600));put(0x500B0,int32_t(-900));put(0x500B4,int32_t(400));
 put(0x886FA0,std::array<int32_t,4>{{0,0,632,568}});put(0xB0CE30,int32_t(632));put(0xB0CE34,int32_t(568));
 vectorAt(0xA80228,0x60000,{0x40000,0x42000});vectorAt(0xA8EC78,0x60040,{0x70000});vectorAt(0xA8ECB8,0x60080,{});
 put(0x40030,int32_t(0));put(0x40034,0x41000u);put(0x42030,int32_t(1));put(0x42034,0x43000u);put(0x45788,uint32_t(1));
 put(0x70000,0x7F5C70u);put(0x70010,88u);put(0x7021C,0x42000u);put(0x706C4,0x80000u);put(0x7006C,int32_t(100));put(0x70074,uint8_t(1));put(0x70090,uint8_t(1));put(0x7009C,int32_t(5120));put(0x700A0,int32_t(5120));put(0x700A4,int32_t(0));put(0x80024,std::array<char,8>{{'T','A','N','K',0,0,0,0}});
}
struct Image {int width=0,height=0;};
struct Request {uint64_t generation=1;bool materialRefresh=true;int detail=0,colorPrecision=0;uint32_t pid=1,match=1;int width=632,height=568;uint32_t reveal=0;int paletteRange=0,pixelScaleMode=1;external::mainview::View view;overlaycomposition::Context context{1,1,1,1,800,600};bool gpu=false;};
struct Frame {Request request;std::shared_ptr<Image>image;uint32_t sampledTick=100,completedTick=110;};
int main(int argc,char**argv){
 if(argc>1&&!strcmp(argv[1],"--identity-child"))return 0;
 setup();external::mainview::View legacy;external::baselineview::View oldLegacy;check(external::mainview::sample(Read{},legacy)&&external::baselineview::sample(Read{},oldLegacy));check(legacy.x==oldLegacy.x&&legacy.width==oldLegacy.width&&legacy.cameraX==oldLegacy.cameraX&&nativeviewhost::key(legacy.transform)==0);check(sh.viewportHostCapabilities==0);
 int cases=0,oldFailures=0;for(int left:{0,168})for(int scale:{50,75,100,125,150}){
  nv::Transform t{{left,16,632,568},{0,0,632*scale/100,568*scale/100},uint32_t(++cases),true};publish(t);put(0x886FA0,std::array<int32_t,4>{{0,0,t.logical.w,t.logical.h}});external::mainview::View v;
  check(nativeviewhost::sample(&sh,Read{},v)&&v.valid()&&v.physical()==t.physical&&v.width==t.logical.w&&v.height==t.logical.h);
  external::baselineview::View old;bool oldOK=external::baselineview::sample(Read{},old);if(!oldOK)++oldFailures;check(!oldOK||old.x!=t.physical.x||old.y!=t.physical.y||old.width!=t.physical.w||old.height!=t.physical.h);
  auto crop=external::mainview::crop(v,13,27);check(crop.x==v.cameraX+390&&crop.y==v.cameraY-405&&crop.width==t.logical.w&&crop.height==t.logical.h);
  const auto ui=external::mainview::physicalView(v);check(ui.valid()&&ui.x==left&&ui.y==16&&ui.width==632&&ui.height==568&&!ui.transform.active);
  auto cursor=external::nativefeedback::cursorRect(100,100,0,0,16,16,0,0,632./ui.width,568./ui.height);check(cursor.x==100&&cursor.y==100&&cursor.w==16&&cursor.h==16);
  auto band=external::nativefeedback::bandStrips({true,{{t.logical.w/4,t.logical.h/4,t.logical.w/2,t.logical.h/2}}},{0,0,632,568},632./v.width,568./v.height);check(band.size()==4&&band[0].x>=157&&band[0].x<=158&&band[0].y>=141&&band[0].y<=142&&band[0].h==std::max(1,int(std::lround(568./v.height))));
  for(double fraction:{-.125,0.,.125,.5,.875,1.,1.125}){double x=t.logical.w*fraction,y=t.logical.h*fraction;check(nativeviewhost::localPoint(t,x,y));check(std::abs(x-(left+632*fraction))<1e-8&&std::abs(y-(16+568*fraction))<1e-8);}
  Request a;a.view=v;a.context.projection=nativeviewhost::key(t);Request b=a;check(renderpresentation::compatible(a,b));b.view.transform.generation++;b.context.projection=nativeviewhost::key(b.view.transform);check(!renderpresentation::compatible(a,b)&&!renderpresentation::sameProjection(a.view,b.view));
  renderpresentation::ViewLease lease;lease.observe(1,1,1,800,600,v,100);check(lease.retain(1,1,1,800,600,110,t));check(!lease.retain(1,1,1,800,600,110,b.view.transform));
  Frame f;f.request=a;f.image=std::make_shared<Image>(Image{632,568});check(renderpresentation::completeUsable(f,a,120)&&!renderpresentation::completeUsable(f,b,120));
  // Actual CPU compositor pixels: the map crop occupies physical A, world
  // anchors land in A once, and old-generation surfaces are discarded.
  overlaycomposition::Context context=a.context;auto map=std::make_shared<std::vector<uint32_t>>(632*568,0xFF143C68u);double x=t.logical.w*.25,y=t.logical.h*.5;check(nativeviewhost::localPoint(t,x,y));
  auto marker=std::make_shared<std::vector<uint32_t>>(9,0xFFFF3300u);std::vector<overlaycomposition::Surface>layers={{1,1,context,{left,16,632,568},false,255,map},{2,2,context,{int(x),int(y),3,3},false,255,marker}};std::vector<uint32_t>pixels;
  check(overlaycomposition::compose(context,layers,pixels));check(pixels[size_t(16)*800+left]==0xFF143C68u&&pixels[size_t(int(y))*800+int(x)]==0xFFFF3300u&&pixels[0]==0);context.projection=b.context.projection;check(!overlaycomposition::valid(layers[0],context));check(overlaycomposition::compose(context,layers,pixels)&&std::all_of(pixels.begin(),pixels.end(),[](uint32_t p){return p==0;}));
 }
 check(cases==10&&oldFailures>=4);
 {nv::Transform uneven{{168,17,631,567},{0,0,947,851},41,true};publish(uneven);put(0x886FA0,std::array<int32_t,4>{{0,0,947,851}});external::mainview::View view;check(nativeviewhost::sample(&sh,Read{},view)&&view.valid());double x=473.5,y=425.5;check(nativeviewhost::localPoint(uneven,x,y)&&std::abs(x-483.5)<1e-8&&std::abs(y-300.5)<1e-8);}
 // The detached superweapon panel uses physical A right, while native
 // ViewBounds and B0CE30 remain the enlarged raster R.
 {setup();nv::Transform side{{0,16,632,568},{0,0,948,852},20,true};put(0x886FA0,int32_t(0));put(0xB0CE30,int32_t(948));std::vector<swsidebar::Entry>entries;int w=0,h=0,right=0;swsidebar::Read read=Read{};check(swsidebar::sample(read,entries,w,h,right,side)&&right==632);check(swsidebar::sample(read,entries,w,h,right)&&right==948);side.physical.x=168;check(swsidebar::sample(read,entries,w,h,right,side)&&right==800);}
 nv::Transform t{{168,16,632,568},{0,0,948,852},25,true};publish(t);put(0x886FA0,std::array<int32_t,4>{{0,0,948,852}});external::mainview::View v,sentinel;sentinel.frame=-19;
 check(nativeviewhost::sample(&sh,Read{},v));sh.viewportSerial|=1;const auto before=reads;check(!nativeviewhost::sample(&sh,Read{},sentinel)&&reads==before&&sentinel.frame==-19);++sh.viewportSerial;
 auto newer=t;++newer.generation;publish(newer);const auto separateReads=reads;check(!nativeviewhost::sample(&sh,Read{},sentinel,t)&&reads==separateReads&&sentinel.frame==-19);publish(t);
 bool changed=false;onRead=[&](uint32_t at){if(!changed&&at==0x886FA0){changed=true;auto altered=t;altered.generation++;publish(altered);}};check(!nativeviewhost::sample(&sh,Read{},sentinel)&&sentinel.frame==-19);onRead={};publish(t);
 failAt=0x886FA0;check(!nativeviewhost::sample(&sh,Read{},sentinel)&&sentinel.frame==-19);failAt=0;
 // Current mapping lease loss is explicit: an immediate remap need never
 // expose a null pointer to paint. A missing/unstable departure is possible R.
 {nativeviewhost::Lifecycle life;nv::Transform out;life.bind({101,1001});check(life.probeAbsent({101,1001}));publish(t);check(life.sample(&sh,out));life.lost(&sh);check(life.unknown()&&!life.sample(&sh,out));life.bind({0,0});life.bind({101,1001});check(!life.sample(static_cast<SharedView*>(nullptr),out));life.bind({101,0});check(!life.sample(&sh,out));auto off=t;off.active=false;++off.generation;publish(off);check(life.sample(&sh,out)&&!life.unknown());check(life.sample(static_cast<SharedView*>(nullptr),out)&&!out.active);
  publish(t);check(life.sample(&sh,out));life.lost(&sh);life.bind({101,1002});check(!life.sample(static_cast<SharedView*>(nullptr),out)&&life.probeAbsent({101,1002}));check(life.sample(&sh,out));life.lost(&sh);life.bind({202,2001});check(!life.sample(static_cast<SharedView*>(nullptr),out)&&life.probeAbsent({202,2001})&&life.sample(static_cast<SharedView*>(nullptr),out));
  nativeviewhost::Lifecycle first;first.bind({303,3001});publish(t);first.lost(&sh);check(first.unknown()&&!first.sample(&sh,out));publish(off);sh.viewportSerial|=1;nativeviewhost::Lifecycle unstable;unstable.bind({404,4001});unstable.lost(&sh);check(unstable.unknown()&&!unstable.sample(static_cast<SharedView*>(nullptr),out));++sh.viewportSerial;check(unstable.sample(&sh,out)&&!unstable.unknown());nativeviewhost::Lifecycle requested;requested.bind({505,5001});sh.viewportHostCapabilities=3;requested.lost(&sh);check(requested.unknown()&&!requested.sample(static_cast<SharedView*>(nullptr),out));sh.viewportHostCapabilities=0;check(requested.sample(&sh,out));}
 // Native capability handshake is an independent production helper; this
 // fixture is not the final UI capability publisher.
 {nativeviewhost::bindProcess(9001,9001);nativeviewhost::bindProcess(9002,9002);constexpr uint32_t marker=nv::HostHandshake,ready=marker|nv::RequiredCapabilities;sh={};sh.viewportHostCapabilities=marker;check(nativeviewhost::capabilityRequest(&sh)==marker);nv::Transform off=t;off.active=false;publish(off);check(nativeviewhost::capabilityRequest(&sh)==ready&&sh.viewportHostCapabilities==marker);
  sh.viewportHostCapabilities=ready;sh.viewZoomBusy=1;check(nativeviewhost::capabilityRequest(&sh)==ready);sh.viewportSerial|=1;nv::Transform out;check(nativeviewhost::capabilityRequest(&sh)==ready&&!nativeviewhost::sampleTransform(&sh,out));++sh.viewportSerial;check(nativeviewhost::capabilityRequest(&sh)==ready);sh.viewZoomBusy=0;publish(t);check(nativeviewhost::capabilityRequest(&sh)==ready);
  // An odd state with busy0 is a fault, not an in-progress authorization.
  sh.viewportSerial|=1;check(nativeviewhost::capabilityRequest(&sh)==marker);++sh.viewportSerial;check(nativeviewhost::capabilityRequest(&sh)==marker);publish(off);check(nativeviewhost::capabilityRequest(&sh)==ready);sh.viewportHostCapabilities=marker;sh.viewZoomBusy=1;check(nativeviewhost::capabilityRequest(&sh)==marker);sh.viewZoomBusy=0;check(nativeviewhost::capabilityRequest(&sh,false)==marker);
  // Pre-existing active permission cannot acquire a new lease, even at the
  // same pointer. Only an actual inactive publication can first grant it.
  publish(t);sh.viewportHostCapabilities=ready;check(nativeviewhost::capabilityRequest(&sh)==marker);publish(off);check(nativeviewhost::capabilityRequest(&sh)==ready);publish(t);check(nativeviewhost::capabilityRequest(&sh)==ready);nativeviewhost::mappingLost(&sh);check(nativeviewhost::capabilityRequest(&sh)==marker);sh={};sh.viewportHostCapabilities=marker;check(!nativeviewhost::sampleTransform(&sh,out)&&nativeviewhost::capabilityRequest(&sh)==marker);publish(off);check(nativeviewhost::sampleTransform(&sh,out)&&nativeviewhost::capabilityRequest(&sh)==ready);sh.viewportGeneration=0;check(nativeviewhost::capabilityRequest(&sh)==marker);
  // Departing marker|0 + fresh zero is not proof that private R retired.
  sh={};sh.viewportHostCapabilities=marker;nativeviewhost::mappingLost(&sh);check(nativeviewhost::lifecycle().unknown()&&!nativeviewhost::sampleTransform(&sh,out));publish(off);check(nativeviewhost::sampleTransform(&sh,out));}
 // Probe real binding before null mapping is interpreted as legacy. All
 // callbacks are tied to PID+creation and expire after a bounded interval.
 {nativeviewhost::Lifecycle life;nv::Transform out;const nativeviewhost::ProcessIdentity id{6001,60001};life.bind(id);check(life.probeDue(100)&&!life.sample(static_cast<SharedView*>(nullptr),out,100));check(!life.probeAbsent({6001,60002},100)&&life.probeDue(100));check(life.probeAbsent(id,100)&&!life.probeDue(349)&&life.sample(static_cast<SharedView*>(nullptr),out,349));check(life.probeDue(350)&&!life.sample(static_cast<SharedView*>(nullptr),out,350));check(life.probeUnknown(id,350)&&life.unknown()&&!life.probeDue(599));check(life.probeAbsent(id,600)&&!life.sample(static_cast<SharedView*>(nullptr),out,600));
  sh={};check(life.probeExisting(id,&sh,650)&&life.unknown());auto off=t;off.active=false;publish(off);sh.viewZoomBusy=1;check(life.probeExisting(id,&sh,700)&&life.unknown());sh.viewZoomBusy=0;sh.viewportSerial|=1;check(life.probeExisting(id,&sh,750)&&life.unknown());++sh.viewportSerial;check(life.probeExisting(id,&sh,800)&&!life.unknown()&&life.sample(static_cast<SharedView*>(nullptr),out,800));publish(t);check(life.probeExisting(id,&sh,850)&&life.unknown());check(life.probeAbsent(id,900)&&!life.sample(static_cast<SharedView*>(nullptr),out,900));publish(off);check(life.probeExisting(id,&sh,950)&&life.sample(static_cast<SharedView*>(nullptr),out,950));life.lost(&sh);check(life.probeDue(950)&&!life.sample(static_cast<SharedView*>(nullptr),out,950));check(life.probeAbsent(id,1000)&&life.sample(static_cast<SharedView*>(nullptr),out,1000));life.bind({6002,60002});check(!life.probeUnknown(id,1050)&&life.probeDue(1050));publish(off);check(life.sample(&sh,out,1050)&&!life.probeDue(1050));
  // First active publish may precede any paint or probe: still fail closed.
  nativeviewhost::Lifecycle unprobed;unprobed.bind(id);publish(t);check(!unprobed.sample(&sh,out,1100)&&unprobed.unknown());publish(off);check(unprobed.sample(&sh,out,1150)&&!unprobed.unknown());}
 {nativeviewhost::Lifecycle departing;departing.bind({6101,61001});sh={};departing.lost(&sh);check(departing.unknown());auto off=t;off.active=false;publish(off);nv::Transform out;check(departing.sample(&sh,out));sh.viewZoomBusy=1;departing.lost(&sh);check(departing.unknown());sh.viewZoomBusy=0;check(departing.sample(&sh,out));}
 // Exercise the actual identity API on this fixture and a hidden copy of
 // itself. No game is started, inspected or attached by this Win32 case.
 {FILETIME created{},exited{},kernel{},user{};check(GetProcessTimes(GetCurrentProcess(),&created,&exited,&kernel,&user)!=0);const uint64_t ownTime=(uint64_t(created.dwHighDateTime)<<32)|created.dwLowDateTime;const auto ownPID=GetCurrentProcessId();wchar_t path[32768]{};check(GetModuleFileNameW(nullptr,path,32768)>0);std::wstring command=L"\""+std::wstring(path)+L"\" --identity-child";STARTUPINFOW startup{};startup.cb=sizeof startup;PROCESS_INFORMATION child{};check(CreateProcessW(path,command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&startup,&child)!=0);check(GetProcessTimes(child.hProcess,&created,&exited,&kernel,&user)!=0);const uint64_t childTime=(uint64_t(created.dwHighDateTime)<<32)|created.dwLowDateTime;nativeviewhost::Lifecycle actual;actual.bind({ownPID,ownTime});check(actual.probeAbsent({ownPID,ownTime}));publish(t);nv::Transform out;check(actual.sample(&sh,out));actual.lost(&sh);actual.bind({ownPID,ownTime});check(!actual.sample(static_cast<SharedView*>(nullptr),out));actual.bind({child.dwProcessId,childTime});check(!actual.sample(static_cast<SharedView*>(nullptr),out)&&actual.probeAbsent({child.dwProcessId,childTime})&&actual.sample(static_cast<SharedView*>(nullptr),out));check(WaitForSingleObject(child.hProcess,5000)==WAIT_OBJECT_0);CloseHandle(child.hThread);CloseHandle(child.hProcess);}
 // 900 R-local px is outside the old 800px screen sampler but is visible in
 // R=948. The physical marker belongs at 768 inside A=[168,800).
 setup();publish(t);put(0x886FA0,std::array<int32_t,4>{{0,0,948,852}});put(0xB0CE30,int32_t(948));put(0xB0CE34,int32_t(852));auto sampler=std::make_unique<external::TacticalSampler<Read>>(Read{});auto oldSampler=std::make_unique<external::BaselineTacticalSampler<Read>>(Read{});sampler->viewport(t);const auto&s=sampler->sample(100,0,true);const auto&old=oldSampler->sample(100,0,true);check(s.status==1&&old.status==1&&s.width==800&&s.height==600);check(sampler->enemyMarkers().count==1&&oldSampler->enemyMarkers().count==0&&sampler->enemyMarkers().width==948);check(sampler->sampledViewport()==t);sampler->viewport(t,false);check(sampler->sample(110,0,true).renderStatus==0&&sampler->enemyMarkers().count==0);
 // Interpolation stays in R and resets at a projection generation. Fractional
 // anchors remain fractional after A/R mapping; fonts need not be resampled.
 static Snapshot sample;sample.labels=1;sample.match=1;sample.frame=1;sample.sampledTick=100;sample.label[0]={11,100,200};static visual::Motion motion;visual::LabelView labels[1];motion.update(sample);motion.render(100,labels);check(labels[0].x==100);sample.frame=2;sample.sampledTick=200;sample.label[0].x=104;motion.update(sample);motion.render(200,labels);check(labels[0].x==100);motion.projection(3);motion.update(sample);motion.render(200,labels);check(labels[0].x==104);float x=100.125f,y=200.125f;check(nativeviewhost::localPoint(t,x,y)&&std::abs(x-(168+100.125f*632/948))<.0001f);
 printf("{\"passed\":true,\"checks\":%d,\"views\":%d,\"baseline_rejected_views\":%d,\"scope\":\"actual View/sample/crop/leases/compositor/motion and sampler source; baseline source counterexamples; synthetic read-only memory\"}\n",checks,cases,oldFailures);
}
