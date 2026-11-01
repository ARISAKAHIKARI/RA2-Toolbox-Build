#ifdef NDEBUG
#undef NDEBUG
#endif
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "../../shared/source/overlay_compositor.h"
#include <cassert>
#include <cstdio>
using namespace overlaycomposition;
struct MockDIB {
 int w=0,h=0;bool dc=false,fail=false;void*bits=nullptr;std::vector<uint32_t>pixels;unsigned allocations=0;
 bool resize(int width,int height){if(fail)return false;if(dc&&w==width&&h==height)return true;pixels.assign(size_t(width)*height,0);bits=pixels.data();w=width;h=height;dc=true;++allocations;return true;}
};
struct Fixture {
 overlaypublication::Submission submission;overlaypublication::CpuStage cpu;MockDIB dib;
 uint64_t presented=~uint64_t(0),token=0,preparedVersion=~uint64_t(0),preparedToken=0;bool visible=false;
 unsigned composes=0,uploads=0,positions=0,validations=0;size_t copied=0;
 bool update(uint64_t now,uint64_t version,const Context&context,std::vector<Surface>layers,bool uploadOK=true,bool active=true){
  submission.observe(version);++validations;if(!active){visible=false;presented=version;return true;}
  bool changed=version!=presented||token!=context.token;
  if(submission.needs(now,changed,visible,cpu.lostContent())){
   if(preparedVersion!=version||preparedToken!=context.token){++composes;if(!cpu.compose(context,std::move(layers),dib)){submission.failed();return false;}preparedVersion=version;preparedToken=context.token;}
   const bool upload=cpu.needsUpload(submission.repair(now)||submission.pending()||!visible||token!=context.token);
   if(cpu.needsCopy()){assert(cpu.copy(dib));copied+=cpu.copiedPixels();}
   if(upload){++uploads;submission.completed(now,uploadOK);if(!uploadOK){cpu.lost();return false;}}
   presented=version;token=context.token;
  }
  ++positions;visible=true;return true;
 }
};
int main(){
 // Compile the actual Win32 Host loop/API as well as exercising its portable
 // CPU stages. No test window, game process or graphics driver is required.
 overlaycompositor::Host unused;unused.setDiagnosticsSink(nullptr);
 Context context{1,101,77,8,4,4,0,0};auto pixels=std::make_shared<const std::vector<uint32_t>>(16,0xff123456u);
 auto layer=[&](const Context&c){return Surface{3,0,c,{0,0,c.width,c.height},true,255,pixels};};
 Fixture f;assert(f.update(1,1,context,{layer(context)}));assert(f.composes==1&&f.uploads==1&&f.copied==16&&f.dib.pixels==*pixels);
 for(uint64_t now=9;now<1001;now+=8)assert(f.update(now,1,context,{layer(context)}));
 assert(f.composes==1&&f.uploads==1&&f.copied==16&&f.positions==f.validations);assert(!f.submission.changed(1)&&f.submission.changed(2));
 assert(f.update(1001,1,context,{layer(context)}));assert(f.uploads==2&&f.composes==1&&f.copied==16); // repair reuses DIB
 assert(f.update(1002,2,context,{layer(context)}));assert(f.composes==2&&f.uploads==2); // metadata-only revision consumes no upload
 auto next=std::make_shared<const std::vector<uint32_t>>(16,0xffabcdefu);auto changed=layer(context);changed.pixels=next;
 assert(!f.update(1003,3,context,{changed},false));assert(f.presented==2&&f.submission.pending()&&!f.submission.changed(3));
 const auto failedCopies=f.copied;assert(f.update(1011,3,context,{changed}));assert(f.presented==3&&f.copied==failedCopies+16&&!f.submission.pending()&&f.dib.pixels==*next);
 const auto before=f.uploads;assert(f.update(1012,3,context,{changed},true,false));assert(!f.visible&&f.uploads==before);assert(f.update(1020,3,context,{changed})&&f.uploads==before+1); // foreground recovery
 auto resized=context;resized.token=2;resized.width=8;auto wide=std::make_shared<const std::vector<uint32_t>>(32,0xff112233u);Surface wideLayer{3,0,resized,{0,0,8,4},true,255,wide};
 f.dib.fail=true;assert(!f.update(1021,4,resized,{wideLayer}));f.dib.fail=false;assert(f.update(1029,4,resized,{wideLayer})&&f.dib.pixels==*wide&&f.dib.allocations==2);
 // Actual incremental compositor handles changed pixels and removal; the
 // mock upload boundary never grants input or modifies a game allocation.
 auto tiny=std::make_shared<const std::vector<uint32_t>>(1,0xffffffffu);Surface hud{25,1,resized,{1,1,1,1},false,255,tiny};
 assert(f.update(1030,5,resized,{wideLayer,hud}));assert(f.dib.pixels[9]==0xffffffffu);const auto dirtyBefore=f.copied;
 assert(f.update(1031,6,resized,{wideLayer}));assert(f.dib.pixels[9]==0xff112233u&&f.copied-dirtyBefore==1);
 using Clock=std::chrono::steady_clock;const auto time=Clock::time_point{};using namespace std::chrono;
 assert(overlaypublication::pollDelay(time,time+milliseconds(200),true)==milliseconds(8));
 assert(overlaypublication::pollDelay(time,time+milliseconds(2),true)==milliseconds(2));
 assert(overlaypublication::pollDelay(time,time-milliseconds(1),true)==Clock::duration::zero());
 overlaypresentrate::Cadence cadence;cadence.submitted(time);assert(cadence.deadline(time,500)==time+milliseconds(2));
 // Idle does not call submitted(): a new frame long after the last upload is
 // immediately eligible, while a deferred frame retains the exact deadline.
 assert(cadence.deadline(time+milliseconds(50),500)==time+milliseconds(50));
 overlaypublication::Submission gpu;gpu.observe(7);gpu.completed(100,true);gpu.failed();assert(gpu.needs(101,false,true,false)&&!gpu.changed(7));gpu.completed(108,false);assert(gpu.repair(1100));gpu.completed(1108,true);assert(!gpu.needs(1109,false,true,false));
 std::puts("PASS actual CPU publication: identical frames perform zero compose/copy/upload; 1s repair reuses DIB; dirty metadata consumed; failed upload/resize retry; hide/resume; incremental layer removal; observed revision avoids pending spin; <=8ms poll and exact 500Hz deadline; GPU pending does not renew success.");
}
