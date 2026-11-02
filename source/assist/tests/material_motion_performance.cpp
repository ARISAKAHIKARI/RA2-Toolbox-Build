#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../../external_reader/source/material_motion.h"
#include RA2_REFERENCE_MOTION
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <limits>
static bool trackAllocations=false;static size_t allocationBytes=0;
void*operator new(size_t n){if(trackAllocations)allocationBytes+=n;void*p=std::malloc(n?n:1);if(!p)throw std::bad_alloc();return p;}
void operator delete(void*p)noexcept{std::free(p);}
void operator delete(void*p,size_t)noexcept{std::free(p);}
using namespace external;
static double clockMs(){return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();}
static void equal(const radar::Frame&a,const radar::Frame&b){
 assert(a.terrain==b.terrain&&a.frame==b.frame&&a.tick==b.tick&&a.observedMs==b.observedMs&&a.markerScanValid==b.markerScanValid&&a.markers.size()==b.markers.size());
 assert(a.scene==b.scene&&a.map==b.map&&a.extras==b.extras&&a.effects==b.effects&&a.effectMask==b.effectMask&&a.posesPrepared==b.posesPrepared&&a.repairs.size()==b.repairs.size());
 assert(materialmotion::presentationKey(a)==materialmotion::presentationKey(b));
 for(size_t n=0;n<a.markers.size();++n){const auto&x=a.markers[n];const auto&y=b.markers[n];
  assert(x.u==y.u&&x.v==y.v&&x.color==y.color&&x.building==y.building&&x.parent==y.parent&&x.parentId==y.parentId&&x.drawZ==y.drawZ&&x.sort==y.sort&&x.layer==y.layer&&x.normalLight==y.normalLight&&x.onBridge==y.onBridge&&x.disguised==y.disguised&&x.tetherHidden==y.tetherHidden&&x.tetherRedraw==y.tetherRedraw&&x.rank==y.rank&&x.displayOrder==y.displayOrder);
  assert(x.pose.rawBody==y.pose.rawBody&&x.pose.rawTurret==y.pose.rawTurret&&x.pose.rawFacing==y.pose.rawFacing&&x.pose.hvaBody==y.pose.hvaBody&&x.pose.hvaTurret==y.pose.hvaTurret&&x.pose.variant==y.pose.variant&&x.pose.buildingFlags==y.pose.buildingFlags&&x.pose.doorFrame==y.pose.doorFrame&&x.pose.turretFrame==y.pose.turretFrame&&x.pose.sequence==y.pose.sequence&&visual::postureSignature(x.pose)==visual::postureSignature(y.pose));
 }
}
static radar::Frame fixture(unsigned count){
 radar::Frame frame;auto terrain=std::make_shared<radar::Terrain>();terrain->geo.width=terrain->geo.height=128;terrain->geo.rawWidth=terrain->geo.rawHeight=256;terrain->geo.rw=terrain->geo.rh=256;terrain->geo.factor=1;terrain->geo.cells=0x100000;terrain->geo.house=0x400000;terrain->geo.count=512*512;terrain->reveal=true;frame.terrain=terrain;frame.frame=1;frame.tick=10;frame.observedMs=1000;frame.markerScanValid=true;
 for(unsigned n=0;n<count;++n){radar::Marker m;m.object=0x10000+n*0x800;m.id=n+1;m.type=0x600000+n%17*0x1000;m.owner=0x400000+n%4*0x10000;m.kind=n%9;m.x=1000+n%100*50;m.y=2000+n/100*50;m.z=n%4*10;m.color=0x4477aa;m.rank=n%3;m.pose.voxel=n%2;m.pose.rawFacing=true;m.pose.rawBody=uint16_t(n*499);m.pose.rawTurret=uint16_t(n*71);m.pose.frame=n%4;m.pose.sequence=1;m.pose.frameFrom=2;m.pose.frameWeight=100;terrain->geo.project(m.x/256.,m.y/256.,m.u,m.v);if(n%13==0)m.tetherRedraw.push_back({m.object,m.id});frame.markers.push_back(m);}
 return frame;
}
int main(){
 materialmotion_before::Timeline before;materialmotion::Timeline after;auto frame=fixture(512);unsigned cases=0;
 for(int pass=0;pass<48;++pass){
  if(pass%11==0){before.reset();after.reset();}
  if(pass%13==0)frame.frame=1;else if(pass%7)++frame.frame;
  frame.observedMs+=pass%9?16:350;frame.tick+=16;
  if(pass%10==0){auto t=std::make_shared<radar::Terrain>(*frame.terrain);t->reveal=!t->reveal;frame.terrain=t;}
  for(size_t n=0;n<frame.markers.size();++n){auto&m=frame.markers[n];m.x+=n%23==0?700:3;m.y+=n%5;m.z+=n%7==0?300:0;m.pose.rawBody+=uint16_t(n%2?400:20000);m.pose.rawTurret+=200;m.pose.frame=(m.pose.frame+1)%5;m.parent=pass%4?0:0x90000;m.onBridge=(pass+n)%5==0;if(n%13==0)m.owner=0x400000+pass%3*0x10000;}
  // Duplicate identities retain the last predecessor, exactly as map::operator[].
  if(frame.markers.size()>2){frame.markers.back()=frame.markers[1];frame.markers.back().x+=8;}
  assert(before.observe(frame,frame.observedMs)==after.observe(frame,frame.observedMs));assert(before.ready()==after.ready()&&before.interval()==after.interval());
  for(double delta:{-1.,0.,1.,8.,25.,50.,100.,500.,std::numeric_limits<double>::quiet_NaN()})for(bool enabled:{false,true})for(bool fine:{false,true}){
   equal(before.sampleLatest(frame.observedMs+delta,enabled,fine),after.sampleLatest(frame.observedMs+delta,enabled,fine));equal(before.sample(frame.observedMs+delta,enabled,fine),after.sample(frame.observedMs+delta,enabled,fine));cases+=2;
  }
  // Same frame replacement changes current membership without advancing time.
  auto replacement=frame;replacement.markers.erase(replacement.markers.begin());replacement.markers[1].id+=999;
  assert(before.observe(replacement,frame.observedMs+2)==after.observe(replacement,frame.observedMs+2));equal(before.sampleLatest(frame.observedMs+3,true,true),after.sampleLatest(frame.observedMs+3,true,true));++cases;
 }
 auto large=fixture(4096);before.reset();after.reset();before.observe(large,1000);after.observe(large,1000);
 double oldObserve=0,newObserve=0,oldMotion=0,newMotion=0,oldStill=0,newStill=0;size_t oldBytes=0,newBytes=0;
 for(int pass=0;pass<80;++pass){++large.frame;large.observedMs+=16;for(auto&m:large.markers){m.x+=2;m.pose.rawBody+=110;}
  auto run=[&](auto&timeline,double&observe,double&motion,double&still,size_t&bytes){allocationBytes=0;trackAllocations=true;auto t=clockMs();timeline.observe(large,large.observedMs);observe+=clockMs()-t;trackAllocations=false;bytes+=allocationBytes;
   t=clockMs();auto moving=timeline.sampleLatest(large.observedMs+8,true,true);motion+=clockMs()-t;t=clockMs();auto stationary=timeline.sampleLatest(large.observedMs+8,false,true);still+=clockMs()-t;assert(moving.markers.size()==4096&&stationary.markers.size()==4096);
  };
  if(pass&1){run(before,oldObserve,oldMotion,oldStill,oldBytes);run(after,newObserve,newMotion,newStill,newBytes);}else{run(after,newObserve,newMotion,newStill,newBytes);run(before,oldObserve,oldMotion,oldStill,oldBytes);}
  equal(before.sampleLatest(large.observedMs+8,true,true),after.sampleLatest(large.observedMs+8,true,true));
 }
 assert(newBytes<oldBytes);
 std::printf("PASS actual Timeline baseline parity %u cases: moving/still, fine/native, discontinuity/reset, duplicate identity, membership/owner/parent/bridge changes, invalid time and expired horizon.\n",cases);
 std::printf("4096 actors x 80 fresh observations: allocated bytes %llu -> %llu; mean ms observe %.6f -> %.6f, moving sample %.6f -> %.6f, disabled-motion sample %.6f -> %.6f. No game FPS claim.\n",(unsigned long long)(oldBytes/80),(unsigned long long)(newBytes/80),oldObserve/80,newObserve/80,oldMotion/80,newMotion/80,oldStill/80,newStill/80);
}
