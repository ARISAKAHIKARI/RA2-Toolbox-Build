#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../../external_reader/source/shared_scene.h"
#include "../source/watch_current_anchor.h"
#include <cassert>
#include <cstdio>
#include <thread>

namespace {
using namespace external;
constexpr uint32_t Cells=0x100000,Cell=0x200000,House=0x300000,Tile=0x400000;
constexpr uint32_t Body=0x600000,Type=0x610000,BodyList=0x620000;
constexpr size_t Edge=33*512+32;
std::vector<uint8_t> memory(0x1000000);
template<class T>void put(uint32_t p,const T&value){std::memcpy(memory.data()+p,&value,sizeof value);}
void setup(){
 std::fill(memory.begin(),memory.end(),0);
 radar::Geometry g{64,64,59,69,120,114,1.f,Cells,House,512*512,{64,64,2,2,60,56},120,114};
 put(radar::Map+0xF4,g.width);put(radar::Map+0xF8,g.height);put(radar::Map+0xFC,g.bounds.left);put(radar::Map+0x100,g.bounds.top);
 put(radar::Map+0x104,g.bounds.columns);put(radar::Map+0x108,g.bounds.rows);put(radar::Map+0x13C,g.cells);put(radar::Map+0x140,g.count);
 put(radar::Map+0x1488,g.factor);put(radar::Map+0x1490,g.offsetX);put(radar::Map+0x1498,g.offsetY);put(radar::Map+0x14A4,g.rw);put(radar::Map+0x14A8,g.rh);
 put(radar::Map+0x1240,g.rawWidth);put(radar::Map+0x1244,g.rawHeight);put(0xA83D4C,House);put(0xA8E9A0,uint8_t(1));put(0xA8E378,uint8_t(1));
 put(0xA8ED2C,0x500000u);put(0xA8ED30,1);put(0xA8ED38,1);put(0x500000,Tile);put(Tile+0x2F0,1);
 put(Cells+Edge*4,Cell);put(Cell+0x24,int16_t(32));put(Cell+0x26,int16_t(33));put(Cell+0x11B,int8_t(2));
 put(Cell+0xEC,2);put(Cell+0x44,-1);put(Cell+0x48,-1);put(Cell+0x12C,uint8_t(0x10));put(Cell+0x140,uint8_t(1));
 put(0xA8EC7C,BodyList);put(0xA8EC80,1);put(0xA8EC88,1);put(BodyList,Body);
 put(Body,0x7F5C70u);put(Body+0x10,7u);put(Body+0x6C,100);put(Body+0x74,uint8_t(1));put(Body+0x90,uint8_t(1));
 put(Body+0x6C4,Type);put(Body+0x21C,House);put(Body+0x9C,32*256);put(Body+0xA0,33*256);put(Body+0x94,2);
 put(House+22265,std::array<uint8_t,3>{{60,120,240}});
}
worldframe::Token token(uint32_t serial,int frame,uint32_t tick,uint32_t omittedPage=0,uint32_t failedPage=0){
 put(0xA8ED84,frame);auto out=std::make_shared<worldframe::Snapshot>();
 worldframe::Key key;key.pid=1;key.owner=2;key.match=3;key.serial=serial;key.frame=frame;key.tick=tick;
 key.scenario=0x700000;key.cells=Cells;key.house=House;key.cellCount=512*512;key.session=0;out->reset(key);
 auto raw=[&](uint32_t p,void*dest,size_t n){if((p&~4095u)==failedPage||uint64_t(p)+n>memory.size())return false;std::memcpy(dest,memory.data()+p,n);return true;};
 for(uint32_t p=0x10000;p<memory.size();p+=worldframe::PageSize)if(p!=omittedPage){const bool ok=out->prefetch(raw,p,worldframe::PageSize);assert(ok==(p!=failedPage));}
 assert(out->seal());return out;
}
struct Reader {
 worldframe::Token current,latest;bool complete=true;size_t reads=0,liveReads=0;uint32_t demand=worldframe::Base;
 explicit Reader(worldframe::Token sample):current(sample),latest(std::move(sample)){}
 bool read(uint32_t p,void*out,size_t n){++reads;if(!worldframe::range(p,n))return false;
  const bool ok=current&&current->read(p,out,n);if(!ok)complete=false;return ok;
 }
 bool readOptional(uint32_t p,void*out,size_t n){++reads;if(!worldframe::range(p,n))return false;
  const bool ok=current&&current->read(p,out,n);if(!ok&&(!current||!current->observed(p,n)))complete=false;return ok;
 }
 bool readFresh(uint32_t p,void*out,size_t n){return read(p,out,n);}
 bool readLive(uint32_t,void*,size_t){++liveReads;return false;}
 bool worldFrameComplete()const{return complete;}
 void markWorldIncomplete(){complete=false;}
 uint32_t worldDemand()const{return demand;}
 void setWorldDemand(uint32_t bits){demand=bits|worldframe::Base;}
 worldframe::Token worldFrameToken()const{return current;}
 bool beginWorldFrame(uint32_t match,worldframe::Token sample={}){current=sample?std::move(sample):latest;complete=true;return current&&current->key.match==match;}
 void endWorldFrame(){current.reset();}
 void prefetch(const std::vector<uint32_t>&,size_t){}
};
auto acquire(radar::SharedSceneProvider<Reader>&provider,Reader&r,radar::SceneConsumer consumer,uint32_t now,bool particles=false,bool lasers=false){
 return provider.acquire(r,1,3,1,now,consumer,true,particles,lasers);
}
}
int main(){
 using namespace external;setup();auto first=token(1,10,100);
 radar::SceneBank bank;radar::SharedSceneProvider<Reader>provider(bank);
 std::array<Reader,3>readers{{Reader(first),Reader(first),Reader(first)}};
 std::array<std::shared_ptr<const radar::Frame>,3>frames;
 std::array<std::thread,3>threads;
 for(unsigned i=0;i<3;++i)threads[i]=std::thread([&,i]{frames[i]=acquire(provider,readers[i],radar::SceneConsumer(i),100);});
 for(auto&t:threads)t.join();
 for(unsigned i=0;i<3;++i){assert(frames[i]&&frames[i]==frames[0]);assert(readers[i].liveReads==0);}
 assert(provider.statistics().attempts==1&&provider.statistics().captures==1&&provider.statistics().reused==2);
 const auto&frame=*frames[0];assert(frame.sourceFrame==first&&frame.tick==100&&frame.frame==10&&frame.posesPrepared);
 assert(frame.terrain->wholeWorld&&frame.terrain->geo.projectionAdjusted&&frame.terrain->art[Edge].cell==Cell&&frame.terrain->art[Edge].sourceColor);
 assert(frame.markers.size()==1&&frame.markers[0].object==Body&&frame.markers[0].x==32*256&&frame.markers[0].y==33*256);
 // Live movement after capture cannot leak into any view or its camera anchor.
 put(Body+0x9C,40*256);put(Body+0xA0,41*256);
 std::array<watch::Anchor,3>anchors{};anchors[0].located=true;anchors[0].target={Body,7};anchors[0].x=40*256;anchors[0].y=41*256;anchors[0].kind=2;
 assert(watchscene::sampledAnchors(frame,anchors)&&anchors[0].located&&anchors[0].x==frame.markers[0].x&&anchors[0].y==frame.markers[0].y);
 anchors[0].target.id=8;assert(watchscene::sampledAnchors(frame,anchors)&&!anchors[0].located);
 // Pause retains the acquisition timestamp; only the exact confirmed token
 // bypasses the ordinary age bound. UI projection may run at a higher cadence.
 assert(acquire(provider,readers[0],radar::SceneConsumer::Main,5100)==frames[0]);
 assert(frame.tick==100&&first->key.tick==100&&!bank.get(1,3,1,5100,true)&&bank.get(1,3,1,5100,true,first)==frames[0]);
 auto current=radar::publicationContext([&](uint32_t p,void*out,size_t n){return readers[0].read(p,out,n);});
 assert(radar::recentMarkers(frame,current,5100));++current.frame;assert(!radar::recentMarkers(frame,current,5100));
 // A late consumer can request more coverage, but must wait for the next
 // world publication rather than triggering a second scan of this one.
 assert(!acquire(provider,readers[1],radar::SceneConsumer::Minimap,5101,true,true));
 assert(provider.statistics().attempts==1&&provider.statistics().captures==1);
 put(Body+0x9C,32*256);put(Body+0xA0,33*256);auto next=token(2,11,5120);Reader nextReader(next);
 auto nextFrame=acquire(provider,nextReader,radar::SceneConsumer::Watch,5120);assert(nextFrame&&nextFrame->effectMask==3&&nextFrame->sourceFrame==next);
 assert(provider.statistics().attempts==2&&provider.statistics().captures==2&&nextReader.liveReads==0);
 // Old queued work cannot replace a newer serial or masquerade as a rewind.
 assert(!acquire(provider,readers[0],radar::SceneConsumer::Main,5130));assert(bank.get(1,3,1,5130,true,next)==nextFrame);
 auto late=std::make_shared<radar::Frame>(frame);late->observedMs=nextFrame->observedMs+1000;bank.publish(1,3,true,late);assert(bank.get(1,3,1,5130,true,next)==nextFrame);
 auto rewound=token(3,2,5140);Reader rewindReader(rewound);auto rewindFrame=acquire(provider,rewindReader,radar::SceneConsumer::Minimap,5140);assert(rewindFrame&&rewindFrame->frame==2);
 // Main disabled: Minimap is a complete producer and Watch reuses its bank.
 radar::SceneBank onlyBank;radar::SharedSceneProvider<Reader>only(onlyBank);Reader mini(first),watchReader(first);
 auto miniFrame=acquire(only,mini,radar::SceneConsumer::Minimap,100);assert(miniFrame&&acquire(only,watchReader,radar::SceneConsumer::Watch,100)==miniFrame);
 assert(only.statistics().captures==1&&only.statistics().attempts==1&&mini.liveReads+watchReader.liveReads==0);
 // Missing dynamic input is deferred without RPM fallback or a same-frame
 // retry storm. A later complete raw publication is the recovery boundary.
 radar::SceneBank failedBank;radar::SharedSceneProvider<Reader>failed(failedBank);auto missing=token(4,12,5200,Body&~4095u);Reader incomplete(missing),again(missing);
 assert(!acquire(failed,incomplete,radar::SceneConsumer::Main,5200)&&!incomplete.complete);
 assert(!acquire(failed,again,radar::SceneConsumer::Watch,5200)&&failed.statistics().attempts==1&&!failedBank.get(1,3,1,5200,true));
 auto recovered=token(5,13,5220);Reader complete(recovered);assert(acquire(failed,complete,radar::SceneConsumer::Minimap,5220));
 assert(failed.statistics().attempts==2&&failed.statistics().captures==1&&incomplete.liveReads+again.liveReads+complete.liveReads==0);
 // Known failed patch-signature probes retain the documented default policy;
 // known failed object bytes remain mandatory even though both were observed.
 radar::SceneBank optionalBank;radar::SharedSceneProvider<Reader>optional(optionalBank);auto probeFailed=token(6,14,5240,0,0x703000);Reader optionalReader(probeFailed);
 assert(acquire(optional,optionalReader,radar::SceneConsumer::Watch,5240)&&optionalReader.complete);
 auto bodyFailed=token(7,15,5260,0,Body);Reader requiredReader(bodyFailed);assert(!acquire(optional,requiredReader,radar::SceneConsumer::Watch,5260)&&!requiredReader.complete);
 auto probeMissing=token(8,16,5280,0x703000);Reader missingProbe(probeMissing);assert(!acquire(optional,missingProbe,radar::SceneConsumer::Watch,5280)&&!missingProbe.complete);
 assert(optionalReader.liveReads+requiredReader.liveReads+missingProbe.liveReads==0);
 // Reusing a PID and the same heap addresses does not reuse the old scene
 // after either Runtime incarnation or Host connection identity changes.
 auto restarted=std::make_shared<worldframe::Snapshot>();auto identity=first->key;identity.generationLow=99;identity.connectionLow=42;identity.tick=5300;
 assert(restarted->import(identity,first->counters,first->pages.data(),first->pages.size()));Reader restartedReader(restarted);
 auto restartedFrame=acquire(provider,restartedReader,radar::SceneConsumer::Watch,5300);assert(restartedFrame&&restartedFrame->sourceFrame==restarted&&restartedFrame!=frames[0]);
 Reader nested(first);nested.markWorldIncomplete();
 {radar::FrameScope inner(nested,3,next);assert(inner&&nested.current==next&&nested.complete&&nested.demand==(worldframe::Base|worldframe::Visual|worldframe::Passengers));}
 assert(nested.current==first&&!nested.complete&&nested.demand==worldframe::Base);
 {radar::FrameScope same(nested,3,first);assert(same&&nested.current==first&&!nested.complete);}
 assert(nested.current==first&&!nested.complete);
 Reader changedDemand(first);
 {radar::FrameScope inner(changedDemand,3,first,worldframe::Visual);assert(inner&&changedDemand.complete);changedDemand.markWorldIncomplete();}
 assert(changedDemand.current==first&&!changedDemand.complete&&changedDemand.demand==worldframe::Base);
 Reader independentFailure(first);
 {radar::FrameScope inner(independentFailure,3,next,worldframe::Visual);assert(inner&&independentFailure.complete);independentFailure.markWorldIncomplete();}
 assert(independentFailure.current==first&&independentFailure.complete&&independentFailure.demand==worldframe::Base);

 std::puts("PASS sealed shared scene: three concurrent consumers, one capture, no live fallback, whole-world north edge, common body/camera anchor, main-disabled producer, paused timestamp, late coverage, old serial/rewind, missing-input recovery and strict/optional failures");
}
