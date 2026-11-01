"""Exercise the shipped minimap worker's pacing, publication and retirement."""
from pathlib import Path
import argparse, json, os, subprocess, tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--zig', type=Path, help='Use this Zig C++ compiler (native g++ otherwise)')
parser.add_argument('--build-dir', type=Path, help='Keep generated production fixture, logs and result')
args = parser.parse_args()

root = Path(__file__).resolve().parents[2]
source = (root / 'assist/source/minimap_overlay.h').read_text(encoding='utf-8')
worker = source.split('struct Request {', 1)[1].split('static radarpanel::Buffer', 1)[0]
worker = 'struct Request {' + worker
# Only scheduling/time primitives are controlled. The production run/set/publication
# code executes unchanged; opening private state makes generation assertions
# possible without adding diagnostic API to the product.
worker = worker.replace('class Worker {', 'class Worker {\npublic:', 1)
worker = worker.replace('std::thread', 'TestThread').replace('std::condition_variable', 'TestCondition')
worker = worker.replace('std::mutex', 'TestMutex')
worker = worker.replace('std::chrono::steady_clock', 'TestClock')
period = (root / 'shared/source/monitor_refresh.h').read_text(encoding='utf-8').split('inline uint64_t periodMicros(', 1)[1].split('\n', 1)[0]
prefix = r'''
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <vector>
#include "@PROFILE@"
#include "@TIMING@"
using DWORD=uint32_t;
static void critical(const std::wstring&){}
static DWORD GetTickCount(){return 1000;}
static int64_t clockMicros=0,workMicros=0;
struct TestClock {
 using duration=std::chrono::microseconds;using time_point=std::chrono::time_point<TestClock,duration>;
 static time_point now(){return time_point(duration(clockMicros));}
};
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
struct TestMutex {
 bool locked=false;
 void lock(){assert(!locked&&"snapshot retirement re-entered a held publication lock");locked=true;}
 void unlock(){assert(locked);locked=false;}
};
static std::function<void(uint64_t)> atWait;
static std::function<void()> atRender;
static std::function<void()> onFrameRetire;
struct TestCondition {
 void notify_one(){}
 template<class Lock,class Predicate> void wait(Lock&,Predicate ready){assert(ready());}
 template<class Lock,class Clock,class Duration,class Predicate>
 bool wait_until(Lock& lock,std::chrono::time_point<Clock,Duration> deadline,Predicate changed){
  assert(!changed());
  auto time=deadline-Clock::now();if(time<Duration::zero())time=Duration::zero();
  lock.unlock();atWait(std::chrono::duration_cast<std::chrono::microseconds>(time).count());lock.lock();
  // A pure rate update must end the old wait without a world invalidation.
  assert(changed());return true;
 }
};
static int samplerResets=0,samplerSteps=0,sceneResets=0,diagramResets=0;static bool capacityFailure=false,markerFresh=true,markerScanValid=true,contextReadable=true,stepOk=true,worldSame=true,useSharedBank=false,readerOpen=true;static int actorCount=1;
namespace external {
namespace xbrzadapter{inline bool requested=false;inline void gpuPresentation(bool value){requested=value;}inline bool gpuUsed(){return false;}}
struct ReadSession {void close(){}bool open(DWORD){return readerOpen;}void begin(){}bool readFresh(uint32_t,void*,size_t){return true;}template<class T>T get(uint32_t,T){return 1;}};
struct SharedReadView{ReadSession*reader;};
namespace cameraassets {template<class Read>struct Context{explicit Context(Read&){}void identity(DWORD,uint32_t){}
 void presentationDetail(int){}void presentationGPU(bool){}void presentationResolution(int,int,int,int){}void presentationColorPrecision(bool){}void presentationPalette(pixelupgrade::PaletteRange){}void presentationLighting(bool,bool){}
};}
namespace radar {
struct Geometry {int rawWidth=64,rawHeight=64,offsetX=0,offsetY=0;bool same(const Geometry&)const{return true;}};
struct Terrain{Geometry geo;bool reveal=false;};
struct Image{int width=320,height=160,units=0;bool gpu=false,boltsRendered=false;};
struct EffectSnapshot{bool boltsValid=true;std::vector<int>bolts;};using Scene=Image;
struct Frame {uint32_t tick=1000;int frame=1;double observedMs=0;std::shared_ptr<const EffectSnapshot>effects;std::shared_ptr<Terrain>terrain=std::make_shared<Terrain>();std::shared_ptr<Image>map,scene;std::vector<int>markers,repairs;bool markerScanValid=false;
 Frame()=default;Frame(const Frame&)=default;Frame(Frame&&)=default;Frame&operator=(const Frame&)=default;Frame&operator=(Frame&&)=default;
 ~Frame(){if(onFrameRetire)onFrameRetire();}
};
inline bool reusableRealScene(const Frame&previous,const Frame&current,int width,int height){return previous.terrain&&current.terrain&&previous.scene&&previous.frame<=current.frame&&previous.terrain->reveal==current.terrain->reveal&&previous.terrain->geo.same(current.terrain->geo)&&previous.scene->width==width&&previous.scene->height==height;}
struct StageStats {bool reused=false;size_t actors=0;double terrainMs=0,palettesMs=0,decodeMs=0,voxelMs=0,objectsMs=0,resolveMs=0,effectsMs=0;};
struct NativeStats {int width=0,height=0;size_t pages=0,resident=0;};
template<class Assets>struct SceneRenderer {StageStats stats;bool capacityExceeded()const{return capacityFailure;}void directPresentation(bool){}void outputSized(bool){}const StageStats&stageStats(){return stats;}NativeStats nativeStats(){return {};}
 void features(const presentation::Profile&){}bool feedbackVisibility(const std::vector<uint8_t>*&mask){mask=nullptr;return true;}void reset(){++sceneResets;}
 template<class Read>auto render(const Frame&frame,int w,int h,Read,Assets&,std::initializer_list<int>,unsigned,bool,bool,bool,bool){if(atRender)atRender();if(capacityFailure)return std::shared_ptr<Image>{};return std::make_shared<Image>(Image{w,h,int(frame.markers.size())});}};
struct DiagramRenderer {void reset(){++diagramResets;}auto render(std::shared_ptr<Terrain>,int w,int h){return std::make_shared<Image>(Image{w,h});}};
template<class Read>struct Sampler {explicit Sampler(Read){}void effects(bool,bool){}void reset(){++samplerResets;}bool step(uint32_t,int,bool){++samplerSteps;clockMicros+=workMicros;return stepOk;}unsigned revision(){return 1;}Frame snapshot(DWORD,int){Frame frame;frame.markerScanValid=markerScanValid;if(markerScanValid&&actorCount)frame.markers.push_back(17);return frame;}};
template<class Read>void preparePoses(Frame&,Read){}inline bool supportsEffects(const Frame&,bool,bool){return true;}
struct PublicationContext{bool readable=true;};
template<class Read>PublicationContext publicationContext(Read){return {contextReadable};}
inline bool sameWorld(const Frame&,PublicationContext context){return context.readable&&worldSame;}inline bool freshMarkers(const Frame&,PublicationContext){return markerFresh;}
inline bool recentMarkers(const Frame&,PublicationContext context,DWORD){return context.readable&&markerFresh;}
struct SceneBank {std::shared_ptr<const Frame> get(DWORD,uint32_t,uint32_t,DWORD,bool){if(!useSharedBank)return {};auto frame=std::make_shared<Frame>();frame->markerScanValid=true;frame->markers.push_back(17);return frame;}
 void publish(DWORD,uint32_t,bool,std::shared_ptr<const Frame>){} };
inline SceneBank&sceneBank(){static SceneBank bank;return bank;}
}
namespace sceneextras {struct Layers{void begin(){}void reset(){}};
 template<class Read,class Assets>auto decorateRadar(std::shared_ptr<radar::Image>image,const radar::Frame&,Read,Assets&,Layers&){return image;}
}
}
namespace external {
template<class Read>double projectionZScale(Read){return .25;}
namespace materialmotion {inline double now(){return double(clockMicros)/1000.;}struct Timeline {radar::Frame current;void reset(){}void observe(const radar::Frame&frame,double){current=frame;}auto sampleLatest(double,bool,bool){return current;}};}
namespace nativefeedback {struct WorldView{int cameraX,cameraY,width,height,frame;};struct Status{bool complete=true;};template<class...A>Status effects(A&&...){return {};}}
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
 const int gpuSamplers=samplerResets,gpuScenes=sceneResets;minimap::Worker gpuWorker;minimap::Request gpuRequest{2,3,60,0,1,320,160};gpuWorker.set(gpuRequest);int gpuWaits=0;
 atWait=[&](uint64_t){auto image=gpuWorker.get();assert(image&&image->scene);if(gpuWaits++==0){assert(!external::xbrzadapter::requested);gpuRequest.gpu=true;gpuWorker.set(gpuRequest);assert(!gpuWorker.get());}
  else if(gpuWaits==2){assert(external::xbrzadapter::requested&&samplerResets==gpuSamplers+1&&sceneResets==gpuScenes+2);gpuRequest.gpu=false;gpuWorker.set(gpuRequest);}
  else{assert(!external::xbrzadapter::requested&&samplerResets==gpuSamplers+1&&sceneResets==gpuScenes+3);gpuWorker.shutdown();}};
 gpuWorker.run();assert(gpuWaits==3);
 contextReadable=false;minimap::Worker transient;transient.set({2,3,60,0,1,320,160});transient.latest=cached;
 atWait=[&](uint64_t){assert(transient.get()==cached&&"unreadable publication context must not erase the last known world");transient.shutdown();};
 transient.run();contextReadable=true;
 stepOk=false;worldSame=true;minimap::Worker lostStep;minimap::Request failedRequest{2,3,60,0,1,320,160};lostStep.set(failedRequest);lostStep.latest=cached;
 int failedWaits=0;atWait=[&](uint64_t){if(failedWaits++==0){assert(lostStep.get()==cached&&"transient terrain/read failure retains the verified image");worldSame=false;failedRequest.rate=61;lostStep.set(failedRequest);}
  else{assert(!lostStep.get()&&"confirmed world change clears the stale image");lostStep.shutdown();}};
 lostStep.run();assert(failedWaits==2);stepOk=true;worldSame=true;
 // Inject actual work into the extracted production worker with a controlled
 // steady clock. Rendering spends the requested interval instead of being
 // followed by another full interval. Slow frames do not accumulate debt.
 minimap::Worker paced;minimap::Request pacedRequest{2,3,144,0,1,320,160};paced.set(pacedRequest);int pacedWaits=0;
 workMicros=5000;
 atWait=[&](uint64_t micros){
  if(pacedWaits++==0){assert(micros==1944);workMicros=18000;pacedRequest.rate=145;paced.set(pacedRequest);}
  else if(pacedWaits==2){assert(micros==0);workMicros=2000;pacedRequest.rate=144;paced.set(pacedRequest);}
  else{assert(micros==4944);paced.shutdown();}
 };
 paced.run();assert(pacedWaits==3);workMicros=0;
 // A fresh shared world bypasses the separate terrain traversal while still
 // publishing the sampled markers to the minimap renderer.
 useSharedBank=true;minimap::Worker shared;shared.set({2,3,60,0,1,320,160});int beforeSteps=samplerSteps;
 atWait=[&](uint64_t){auto result=shared.get();assert(result&&result->scene&&result->scene->units==1&&samplerSteps==beforeSteps);shared.shutdown();};
 shared.run();useSharedBank=false;
 // Observe the public snapshot at the real render boundary. Initial loading
 // publishes a diagram; later renders leave the previous COMPLETE pointer
 // unchanged until the final publication, rather than a second provisional.
 minimap::Worker publication;minimap::Request publicationRequest{2,3,60,0,1,320,160};publication.set(publicationRequest);
 int renderChecks=0,publicationWaits=0;std::shared_ptr<const external::radar::Frame> publicationPrior;
 atRender=[&]{auto visible=publication.get();if(renderChecks++==0)assert(visible&&visible->map&&!visible->scene&&"initial loading publishes its diagram before rendering");
  else assert(visible==publicationPrior&&visible->scene&&"completed source remains published without an intermediate replacement");};
 atWait=[&](uint64_t){auto visible=publication.get();assert(visible&&visible->scene);if(publicationWaits++==0){publicationPrior=visible;publicationRequest.rate=61;publication.set(publicationRequest);}
  else{assert(visible!=publicationPrior);publication.shutdown();}};
 publication.run();assert(renderChecks==2&&publicationWaits==2);atRender={};publicationPrior.reset();
 // Destruction is instrumented at the stand-in Frame boundary, while all
 // production retirement branches execute unchanged. Re-enter get() to prove
 // every retirement is outside the actual Worker lock, including rejected
 // incoming frames after a confirmed world change during rendering.
 unsigned retired=0;
 auto audit=[&](minimap::Worker&w){onFrameRetire=[&]{assert(!w.mutex.locked);(void)w.get();++retired;};};
 auto seed=[](minimap::Worker&w){auto frame=std::make_shared<external::radar::Frame>();frame->markerScanValid=true;frame->map=std::make_shared<external::radar::Image>();frame->scene=std::make_shared<external::radar::Image>();w.latest=frame;};
 {minimap::Worker target;minimap::Request r{2,3,60,0,1,320,160};target.set(r);audit(target);seed(target);r.match=4;target.set(r);assert(!target.get());seed(target);target.shutdown();assert(!target.get());onFrameRetire={};}
 for(int branch=0;branch<5;++branch){
  minimap::Worker target;target.set({2,3,60,0,1,320,160});audit(target);seed(target);
  if(branch==0)worldSame=false;
  if(branch==1){stepOk=false;worldSame=false;}
  if(branch==2)readerOpen=false;
  if(branch==3)atRender=[] {worldSame=false;}; // changes only after initial validation
  atWait=[&](uint64_t){if(branch<4)assert(!target.get());else assert(target.get()&&target.get()->scene);target.shutdown();};
  target.run();assert(!target.get());onFrameRetire={};atRender={};worldSame=stepOk=readerOpen=true;
 }
 assert(retired>=9);
 std::cout<<"PASS production minimap worker: rate updates, transient sampling/context failure retains units, verified world/empty change clears, capacity retry and recovery\n";
 std::cout<<"PASS production minimap deadlines: 5/18/2ms controlled work consumes each period; slow work adds no full wait or catch-up debt\n";
 std::cout<<"PASS production minimap publication: initial diagram, one final replacement for completed scenes; set/shutdown/read/world/final-swap retire outside lock ("<<retired<<" retirements)\n";
}
'''
code = prefix.replace('@PERIOD@', 'inline uint64_t periodMicros(' + period).replace('@PROFILE@',(root/'external_reader/source/render_profile.h').as_posix()).replace('@TIMING@',(root/'shared/source/pipeline_timing.h').as_posix()) + worker + suffix

def execute(path):
    path.mkdir(parents=True, exist_ok=True)
    source_path = path / 'test.cpp'
    source_path.write_text(code, encoding='utf-8')
    binary = path / ('test.exe' if os.name == 'nt' else 'test')
    compiler = [str(args.zig.resolve()), 'c++'] if args.zig else ['g++']
    if args.zig and os.name == 'nt':
        compiler += ['-target', 'x86_64-windows-gnu']
    command = compiler + ['-std=c++17', '-O2', '-pthread', str(source_path), '-o', str(binary)]
    build = subprocess.run(command, capture_output=True, text=True)
    (path / 'build.log').write_text(build.stdout + build.stderr, encoding='utf-8')
    result = dict(build_command=command, build_exit=build.returncode, scope='Extracted production Worker; controlled scheduling and renderer/read stand-ins; no game process')
    if build.returncode:
        print(build.stdout + build.stderr)
        (path / 'result.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
        raise SystemExit(build.returncode)
    run = subprocess.run([str(binary)], capture_output=True, text=True, timeout=30)
    (path / 'test.log').write_text(run.stdout + run.stderr, encoding='utf-8')
    result.update(test_exit=run.returncode, stdout=run.stdout, stderr=run.stderr)
    (path / 'result.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    print(run.stdout + run.stderr)
    if run.returncode:
        raise SystemExit(run.returncode)

if args.build_dir:
    execute(args.build_dir.resolve())
else:
    with tempfile.TemporaryDirectory(prefix='minimap-rate-') as temp:
        execute(Path(temp))

