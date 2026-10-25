"""Exercise the shipped minimap worker's rate-only update and wait predicate."""
from pathlib import Path
import subprocess, tempfile

root = Path(__file__).resolve().parents[2]
source = (root / 'assist/source/minimap_overlay.h').read_text()
worker = source.split('struct Request {', 1)[1].split('static assist::construction::PanelBuffer', 1)[0]
worker = 'struct Request {' + worker
# Only thread/CV boundaries are controlled. The production run/set/publication
# code executes unchanged; opening private state makes generation assertions
# possible without adding diagnostic API to the product.
worker = worker.replace('class Worker {', 'class Worker {\npublic:', 1)
worker = worker.replace('std::thread', 'TestThread').replace('std::condition_variable', 'TestCondition')
period = (root / 'shared/source/monitor_refresh.h').read_text().split('inline uint64_t periodMicros(', 1)[1].split('\n', 1)[0]
prefix = r'''
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <vector>
using DWORD=uint32_t;
static DWORD GetTickCount(){return 1000;}
namespace monitorrefresh {
constexpr int FallbackHz=60,MinHz=5,MaxHz=500;
@PERIOD@
}
struct TestThread {
 bool live=false;
 TestThread()=default;
 template<class F> explicit TestThread(F){live=true;}
 bool joinable()const{return live;}
 void join(){live=false;}
};
static std::function<void(uint64_t)> atWait;
struct TestCondition {
 void notify_one(){}
 template<class Lock,class Predicate> void wait(Lock&,Predicate ready){assert(ready());}
 template<class Lock,class Rep,class Period,class Predicate>
 bool wait_for(Lock& lock,std::chrono::duration<Rep,Period> time,Predicate changed){
  assert(!changed());
  lock.unlock();atWait(std::chrono::duration_cast<std::chrono::microseconds>(time).count());lock.lock();
  // A pure rate update must end the old wait without a world invalidation.
  assert(changed());return true;
 }
};
static int samplerResets=0,sceneResets=0,diagramResets=0;static bool capacityFailure=false,markerFresh=true,markerScanValid=true,contextReadable=true;static int actorCount=1;
namespace external {
struct ReadSession {void close(){}bool open(DWORD){return true;}void begin(){}bool readFresh(uint32_t,void*,size_t){return true;}template<class T>T get(uint32_t,T){return 1;}};
struct SharedReadView{ReadSession*reader;};
namespace cameraassets {template<class Read>struct Context{explicit Context(Read&){}void identity(DWORD,uint32_t){}};}
namespace radar {
struct Geometry {bool same(const Geometry&)const{return true;}};
struct Terrain{Geometry geo;bool reveal=false;};
struct Image{int width=320,height=160,units=0;};
struct Frame {std::shared_ptr<Terrain>terrain=std::make_shared<Terrain>();std::shared_ptr<Image>map,scene;std::vector<int>markers,repairs;bool markerScanValid=false;};
template<class Assets>struct SceneRenderer {bool capacityExceeded()const{return capacityFailure;}void reset(){++sceneResets;}template<class Read>auto render(const Frame&frame,int w,int h,Read,Assets&){if(capacityFailure)return std::shared_ptr<Image>{};return std::make_shared<Image>(Image{w,h,int(frame.markers.size())});}};
struct DiagramRenderer {void reset(){++diagramResets;}auto render(std::shared_ptr<Terrain>,int w,int h){return std::make_shared<Image>(Image{w,h});}};
template<class Read>struct Sampler {explicit Sampler(Read){}void reset(){++samplerResets;}bool step(uint32_t,int,bool){return true;}unsigned revision(){return 1;}Frame snapshot(DWORD){Frame frame;frame.markerScanValid=markerScanValid;if(markerScanValid&&actorCount)frame.markers.push_back(17);return frame;}};
struct PublicationContext{bool readable=true;};
template<class Read>PublicationContext publicationContext(Read){return {contextReadable};}
inline bool sameWorld(const Frame&,PublicationContext context){return context.readable;}inline bool freshMarkers(const Frame&,PublicationContext){return markerFresh;}
}
}
namespace minimap {
'''
suffix = r'''
}
int main(){
 minimap::Worker preview;minimap::Request previewRequest{2,3,60,0,1,320,160};preview.set(previewRequest);
 auto cached=std::make_shared<external::radar::Frame>();preview.latest=cached;
 auto priorGeneration=preview.generation;previewRequest.width=600;preview.set(previewRequest);
 assert(preview.generation==priorGeneration+1&&preview.get()==cached&&"resizing retains the last verified image until its replacement is ready");
 previewRequest.match=4;preview.set(previewRequest);
 assert(!preview.get()&&"new match discards the old image");preview.shutdown();
 minimap::Worker worker;minimap::Request request{2,3,5,0,1,320,160};worker.set(request);
 auto generation=worker.generation;int waits=0;std::shared_ptr<const external::radar::Frame> prior,empty;
 atWait=[&](uint64_t micros){
  assert(samplerResets==1&&sceneResets==1&&diagramResets==1);
  assert(worker.generation==generation);auto complete=worker.get();assert(complete&&complete->map);
  int step=waits++;
  if(step==0){
   assert(complete->scene&&complete->scene->units==1);
   assert(micros==200000);request.rate=144;worker.set(request);
   assert(worker.get()==complete&&worker.generation==generation);
  }else if(step==1){
   assert(micros==6944&&complete->scene&&complete->scene->units==1);prior=complete;
   markerFresh=false;request.rate=240;worker.set(request);
   assert(worker.get()==complete&&worker.generation==generation);
  }else if(step==2){
   assert(micros==4166&&complete==prior&&"late marker sample must retain the complete frame");
   markerFresh=true;markerScanValid=false;request.rate=241;worker.set(request);
  }else if(step==3){
   assert(micros==4149&&complete==prior&&"failed actor-vector read must retain the complete frame");
   markerScanValid=true;actorCount=0;request.rate=242;worker.set(request);
  }else if(step==4){
   assert(micros==4132&&complete!=prior&&complete->scene&&complete->scene->units==0&&"a verified empty vector removes dead actors");
   empty=complete;actorCount=1;capacityFailure=true;request.rate=243;worker.set(request);
  }else if(step==5){
   assert(micros==4115&&complete!=empty&&complete->scene==empty->scene&&"capacity retry retains the same-world image");
   capacityFailure=false;request.rate=244;worker.set(request);
  }else{
   assert(micros==4098&&complete->scene&&complete->scene!=empty->scene&&complete->scene->units==1);worker.shutdown();
  }
 };
 worker.run();assert(waits==7);
 assert(samplerResets==1&&sceneResets==1&&diagramResets==1);
 contextReadable=false;minimap::Worker transient;transient.set({2,3,60,0,1,320,160});transient.latest=cached;
 atWait=[&](uint64_t){assert(transient.get()==cached&&"unreadable publication context must not erase the last known world");transient.shutdown();};
 transient.run();contextReadable=true;
 std::cout<<"PASS production minimap worker: rate updates, stale/failed scans and unreadable context retain units, verified empty removes actors, capacity retry, recovery and shutdown\n";
}
'''
code = prefix.replace('@PERIOD@', 'inline uint64_t periodMicros(' + period) + worker + suffix
with tempfile.TemporaryDirectory(prefix='minimap-rate-') as temp:
    path = Path(temp)
    (path / 'test.cpp').write_text(code)
    subprocess.run(['g++', '-std=c++17', '-O2', '-pthread', str(path / 'test.cpp'), '-o', str(path / 'test')], check=True)
    subprocess.run([str(path / 'test')], check=True)

