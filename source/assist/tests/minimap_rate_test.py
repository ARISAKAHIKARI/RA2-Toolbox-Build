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
static int samplerResets=0,sceneResets=0,diagramResets=0;static bool capacityFailure=false;
namespace external {
struct ReadSession {void close(){}bool open(DWORD){return true;}void begin(){}bool readFresh(uint32_t,void*,size_t){return true;}template<class T>T get(uint32_t,T){return 1;}};
struct SharedReadView{ReadSession*reader;};
namespace cameraassets {template<class Read>struct Context{explicit Context(Read&){}void identity(DWORD,uint32_t){}};}
namespace radar {
struct Geometry {bool same(const Geometry&)const{return true;}};
struct Terrain{Geometry geo;bool reveal=false;};
struct Image{int width=320,height=160;};
struct Frame {std::shared_ptr<Terrain>terrain=std::make_shared<Terrain>();std::shared_ptr<Image>map,scene;std::vector<int>markers,repairs;};
template<class Assets>struct SceneRenderer {bool capacityExceeded()const{return capacityFailure;}void reset(){++sceneResets;}template<class Read>auto render(const Frame&,int w,int h,Read,Assets&){if(capacityFailure)return std::shared_ptr<Image>{};return std::make_shared<Image>(Image{w,h});}};
struct DiagramRenderer {void reset(){++diagramResets;}auto render(std::shared_ptr<Terrain>,int w,int h){return std::make_shared<Image>(Image{w,h});}};
template<class Read>struct Sampler {explicit Sampler(Read){}void reset(){++samplerResets;}bool step(uint32_t,int,bool){return true;}unsigned revision(){return 1;}Frame snapshot(DWORD){return {};}};
template<class Read>int publicationContext(Read){return 1;}
inline bool sameWorld(const Frame&,int){return true;}inline bool freshMarkers(const Frame&,int){return true;}
}
}
namespace minimap {
'''
suffix = r'''
}
int main(){
 minimap::Worker worker;minimap::Request request{2,3,5,0,1,320,160};worker.set(request);
 auto generation=worker.generation;int waits=0;
 atWait=[&](uint64_t micros){
  assert(samplerResets==1&&sceneResets==1&&diagramResets==1);
  assert(worker.generation==generation);auto complete=worker.get();assert(complete&&complete->map);if(waits!=3)assert(complete->scene);
  if(waits++==0){
   assert(micros==200000);request.rate=144;worker.set(request);
   assert(worker.get()==complete&&worker.generation==generation);
  }else if(waits==2){
   assert(micros==6944);request.rate=240;worker.set(request);
   assert(worker.get()==complete&&worker.generation==generation);
  }else if(waits==3){
   assert(micros==4166);capacityFailure=true;request.rate=241;worker.set(request);
  }else if(waits==4){
   assert(micros==4149&&!complete->scene&&complete->map);capacityFailure=false;request.rate=242;worker.set(request);
  }else{
   assert(micros==4132&&complete->scene);worker.shutdown();
  }
 };
 worker.run();assert(waits==5);
 assert(samplerResets==1&&sceneResets==1&&diagramResets==1);
 std::cout<<"PASS production minimap worker: 5→144→240 Hz microsecond waits, rate-only wakeup, same published frame and warm world caches retained, capacity failure drops inherited real scene with diagram retained, recovery and shutdown\n";
}
'''
code = prefix.replace('@PERIOD@', 'inline uint64_t periodMicros(' + period) + worker + suffix
with tempfile.TemporaryDirectory(prefix='minimap-rate-') as temp:
    path = Path(temp)
    (path / 'test.cpp').write_text(code)
    subprocess.run(['g++', '-std=c++17', '-O2', '-pthread', str(path / 'test.cpp'), '-o', str(path / 'test')], check=True)
    subprocess.run([str(path / 'test')], check=True)

