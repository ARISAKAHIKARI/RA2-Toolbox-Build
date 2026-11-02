#include "radar_layout.h"
#include "secondary_radar_layout.h"
#include "../../external_reader/source/radar_navigation.h"
#include "construction_buffer.h"
#include "../../external_reader/source/radar_map.h"
#include "../../external_reader/source/camera_assets.h"
#include "../../external_reader/source/radar_scene.h"
#include "../../external_reader/source/radar_publication.h"
#include "../../external_reader/source/scene_bank.h"
#include "../../external_reader/source/scene_poses.h"
#include "../../external_reader/source/render_profile.h"
#include "../../external_reader/source/feedback_scene.h"
#include "../../external_reader/source/material_motion.h"
#include "../../shared/source/monitor_refresh.h"
#include "../../shared/source/secondary_display.h"
#include "../../shared/source/pipeline_timing.h"
#include "radar_paint_gate.h"
#include "radar_panel.h"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
namespace minimap {
struct Request {DWORD pid=0;uint32_t match=0;int rate=60;uint32_t reveal=0;int mode=0,width=320,height=160;bool gpu=false;external::presentation::Selection selection;};
// Separate task cache, shared central read-only connection. Terrain traversal
// never blocks the UI or the game thread. Publish complete immutable frames.
class Worker {
 std::mutex mutex;std::condition_variable cv;std::thread thread;bool stop=false,paused=false;Request request{};uint64_t generation=0;
 std::shared_ptr<const external::radar::Frame> latest;
 void run(){
  external::ReadSession reader;external::cameraassets::Context<external::ReadSession> assets(reader);external::radar::SceneRenderer<decltype(assets)> renderer;external::radar::DiagramRenderer diagram;external::sceneextras::Layers radarLayers;external::materialmotion::Timeline motion;external::radar::Sampler<external::SharedReadView> sampler(external::SharedReadView{&reader});uint64_t active=~uint64_t(0);int previousFrame=-1;unsigned previousRevision=0;Request activeRequest{};
  DWORD diagnosticAt=0;pipelineperf::Window timings(L"minimap-worker",{L"total",L"sample",L"snapshot",L"diagram",L"render",L"publish"});
  for(;;){Request r;uint64_t token;{std::unique_lock<std::mutex> lock(mutex);cv.wait(lock,[&]{return stop||(!paused&&request.pid);});if(stop)return;r=request;token=generation;}
   const auto iterationStarted=std::chrono::steady_clock::now();
   pipelineperf::Scope total(timings,0,critical);
   if(token!=active){if(r.pid!=activeRequest.pid||r.match!=activeRequest.match){reader.close();sampler.reset();renderer.reset();diagram.reset();radarLayers.reset();motion.reset();}else {if(r.mode!=activeRequest.mode)sampler.reset();if(r.gpu!=activeRequest.gpu||r.selection.revision!=activeRequest.selection.revision)renderer.reset();}activeRequest=r;active=token;previousFrame=-1;previousRevision=0;}
   external::xbrzadapter::gpuPresentation(r.gpu); // this worker owns its GPU state; native assets stay CPU-prepared
   std::shared_ptr<const external::radar::Frame> next,retired;
   if(reader.open(r.pid)){reader.begin();
    pipelineperf::Scope sample(timings,1);
    auto fresh=[&](uint32_t p,void*out,size_t n){return reader.readFresh(p,out,n);};
    auto shared=external::radar::sceneBank().get(r.pid,r.match,r.reveal,GetTickCount(),true);
    if(shared&&(!external::radar::recentMarkers(*shared,external::radar::publicationContext(fresh),GetTickCount())||!external::radar::supportsEffects(*shared,r.selection.profile.particles,r.selection.profile.lasers)))shared.reset();
    // Shared world banks retain the engine's allocated RGB backing. A
    // scenario trigger can change the radar span without reallocating it;
    // such a bank cannot be used as this panel's current projection.
    if(shared&&!sampler.radarProjectionCompatible(*shared))shared.reset();
    sampler.effects(r.selection.profile.particles,r.selection.profile.lasers);bool ok=shared||sampler.step(r.reveal,4096,r.mode==1,true,true);
    const auto sampleReads=reader.reads,samplePages=reader.prefetchedPages;
    reader.begin();int frame=reader.get<int>(0xA8ED84,-1);sample.finish();
    if(ok&&(r.mode==1||frame!=previousFrame||sampler.revision()!=previousRevision)){
     pipelineperf::Scope snapshot(timings,2);auto data=shared?*shared:sampler.snapshot(GetTickCount(),frame);if(!shared){data.observedMs=external::materialmotion::now();external::radar::preparePoses(data,external::SharedReadView{&reader});}assets.identity(r.pid,r.match);snapshot.finish();
     bool reusedDiagram=false;
     if(r.mode==1){std::lock_guard<std::mutex> lock(mutex);if(token==generation&&latest&&external::radar::reusableRealScene(*latest,data,r.width,r.height)){data.map=latest->map;data.scene=latest->scene;reusedDiagram=true;}}
     if(!reusedDiagram){pipelineperf::Scope diagramTime(timings,3);radarLayers.begin();data.map=external::sceneextras::decorateRadar(diagram.render(data.terrain,r.width,r.height),data,external::SharedReadView{&reader},assets,radarLayers);}
     auto sampled=external::radar::publicationContext(fresh);
     const bool same=external::radar::sameWorld(data,sampled);
     // Sampling may span several game ticks. Keep the last complete image
     // until this world's markers are fresh instead of publishing a frame in
     // which every unit vanishes for one paint.
     if(same&&data.markerScanValid&&external::radar::recentMarkers(data,sampled,GetTickCount())){
      if(!shared&&r.mode==1&&!data.terrain->geo.projectionAdjusted)external::radar::sceneBank().publish(r.pid,r.match,true,std::make_shared<const external::radar::Frame>(data));
      // Publish terrain immediately; retain the last completed scene while the
      // next one loads. Initial MIX scans must not keep the whole window hidden.
      bool initial=false;{std::lock_guard<std::mutex> lock(mutex);if(token==generation){if(r.mode==1&&latest&&external::radar::reusableRealScene(*latest,data,r.width,r.height))data.scene=latest->scene;initial=r.mode==1&&(!latest||!latest->scene);}}
      // Only initial loading needs a provisional diagram. An existing complete
      // scene stays published while rendering, without copying the whole actor
      // snapshot twice or destroying the retired snapshot under the UI lock.
      if(initial){std::shared_ptr<const external::radar::Frame> provisional=std::make_shared<external::radar::Frame>(data);{std::lock_guard<std::mutex> lock(mutex);if(token==generation)latest.swap(provisional);}}
      if(r.mode==1){
       const auto&profile=r.selection.profile;external::presentation::apply(profile,assets,data.terrain->geo.rawWidth*30,data.terrain->geo.rawHeight*15,r.width,r.height);assets.identity(r.pid,r.match);renderer.features(profile);renderer.directPresentation(true);
       motion.observe(data,external::materialmotion::now());auto animated=motion.sampleLatest(external::materialmotion::now(),profile.motion,profile.detail!=0);animated.tick=GetTickCount();
       renderer.outputSized(true);
       pipelineperf::Scope renderTime(timings,4);const auto renderStarted=std::chrono::steady_clock::now();const auto renderReadsAt=reader.reads,renderPagesAt=reader.prefetchedPages;
       auto scene=renderer.render(animated,r.width,r.height,external::SharedReadView{&reader},assets,{},unsigned(profile.scale),profile.detail!=0,true,profile.precision!=0,true);
       const DWORD diagnosticNow=GetTickCount();if(!diagnosticAt||DWORD(diagnosticNow-diagnosticAt)>=5000){diagnosticAt=diagnosticNow;const auto&s=renderer.stageStats();const auto n=renderer.nativeStats();wchar_t line[768];
        swprintf(line,768,L"minimap render ok=%d gpu=%d reused=%d diagram-reused=%d raster=%dx%d ground=%dx%d pages=%llu resident=%llu actors=%llu ms terrain=%.2f palette=%.2f decode=%.2f voxel=%.2f objects=%.2f resolve=%.2f effects=%.2f total=%.2f",bool(scene),external::xbrzadapter::gpuUsed(),s.reused,reusedDiagram,r.width,r.height,n.width,n.height,(unsigned long long)n.pages,(unsigned long long)n.resident,(unsigned long long)s.actors,s.terrainMs,s.palettesMs,s.decodeMs,s.voxelMs,s.objectsMs,s.resolveMs,s.effectsMs,std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-renderStarted).count());critical(line);
        wchar_t io[256];swprintf(io,256,L"minimap reads shared=%d sample-rpm=%u sample-prefetched-pages=%u render-rpm=%u render-prefetched-pages=%u",bool(shared),sampleReads,samplePages,reader.reads-renderReadsAt,reader.prefetchedPages-renderPagesAt);critical(io);
        wchar_t visibility[256];swprintf(visibility,256,L"minimap visibility sampled=%d ms=%.2f rpm=%llu pages=%llu cells=%llu cpu-terrain-reused=%d",s.visibilityAttempted,s.visibilityMs,(unsigned long long)s.visibilityCalls,(unsigned long long)s.visibilityPages,(unsigned long long)s.visibilityCells,s.cpuTerrainReused);critical(visibility);
        wchar_t cache[256];swprintf(cache,256,L"minimap vxl-geometry-cache cumulative builds=%llu hits=%llu face-checks=%llu evictions=%llu charged-bytes=%llu",(unsigned long long)assets.voxelGeometryBuilds(),(unsigned long long)assets.voxelGeometryHits(),(unsigned long long)assets.voxelGeometryFaceChecks(),(unsigned long long)assets.voxelGeometryEvictions(),(unsigned long long)assets.voxelGeometryCacheBytes());critical(cache);
        if(!scene){std::wstring reason;for(unsigned char c:s.assetFailure){if(reason.size()==160)break;reason.push_back(c>=32&&c<127?wchar_t(c):L'_');}
         wchar_t failed[1280];swprintf(failed,1280,L"minimap render-failure reason=%hs object=%08X type=%08X id=%u kind=%d candidates=%llu rasterScale=%d vpl=%d lightChanged=%d gunPose=%d posture=%d shadow=%d hva=%d/%d faces=%llu/%llu pixels=%llu old=%llu new=%llu limit=%llu geometry=%llu/%llu decodeAttempts=%u detail=%ls",
          external::radar::renderFailureName(s.failure),s.failureObject,s.failureType,s.failureID,s.failureKind,(unsigned long long)s.candidates,s.rasterScale,s.voxelLightAvailable,s.voxelLightChanged,s.gunPoseValid,s.postureAvailable,s.shadowAvailable,s.hvaBody,s.hvaTurret,(unsigned long long)s.liveFaces,(unsigned long long)external::rendermemory::LiveFaces,(unsigned long long)s.posePixels,(unsigned long long)s.replacementPixels,(unsigned long long)s.newPixels,(unsigned long long)external::rendermemory::PosePixels,(unsigned long long)s.geometryBytes,(unsigned long long)external::rendermemory::Geometry,renderer.decodeStats().attempts,reason.empty()?L"none":reason.c_str());critical(failed);
        }
       }
       if(scene&&!scene->gpu&&!scene->boltsRendered&&profile.lasers&&data.effects&&data.effects->boltsValid){const std::vector<uint8_t>*mask=nullptr;if(renderer.feedbackVisibility(mask)){
        const auto&geo=data.terrain->geo;external::nativefeedback::WorldView projection{-geo.offsetX*30,geo.offsetY*15,geo.rawWidth*30,geo.rawHeight*15,data.frame};
        auto decorated=std::make_shared<external::radar::Scene>(*scene);auto status=external::nativefeedback::effects(data.effects->bolts,*decorated,projection,mask,external::projectionZScale(fresh));if(status.complete)scene=std::move(decorated);
       }}
       if(scene)data.scene=std::move(scene);
      }
      next=std::make_shared<external::radar::Frame>(std::move(data));
     }else if(!same&&sampled.readable){std::lock_guard<std::mutex> lock(mutex);if(token==generation&&latest&&!external::radar::sameWorld(*latest,sampled))retired=std::move(latest);}
     previousFrame=frame;previousRevision=sampler.revision();
    }
    if(!ok){auto fresh=[&](uint32_t p,void*out,size_t n){return reader.readFresh(p,out,n);};auto observed=external::radar::publicationContext(fresh);
     if(observed.readable){std::lock_guard<std::mutex> lock(mutex);if(token==generation&&latest&&!external::radar::sameWorld(*latest,observed))retired=std::move(latest);}}
   }else{std::lock_guard<std::mutex> lock(mutex);if(token==generation)retired=std::move(latest);}
   auto fresh=[&](uint32_t p,void*out,size_t n){return reader.readFresh(p,out,n);};auto completed=external::radar::publicationContext(fresh);
   {pipelineperf::Scope publish(timings,5);std::lock_guard<std::mutex> lock(mutex);if(token==generation&&next){if(external::radar::sameWorld(*next,completed))latest.swap(next);else if(completed.readable&&latest&&!external::radar::sameWorld(*latest,completed))retired=std::move(latest);}}
   next.reset();retired.reset();total.finish();
   {std::unique_lock<std::mutex> lock(mutex);cv.wait_until(lock,iterationStarted+std::chrono::microseconds(monitorrefresh::periodMicros(r.rate)),[&]{return stop||paused||generation!=token||request.rate!=r.rate;});if(stop)return;}
  }
 }
public:
 ~Worker(){shutdown();}
 void set(Request r){std::shared_ptr<const external::radar::Frame> retired;std::lock_guard<std::mutex> lock(mutex);bool changed=r.pid!=request.pid||r.match!=request.match||r.reveal!=request.reveal||r.mode!=request.mode||r.width!=request.width||r.height!=request.height||r.gpu!=request.gpu||r.selection.revision!=request.selection.revision;
  if(changed){++generation;
   // A size edit may present the last verified world while the one shared
   // renderer produces the requested dimensions. Identity/mode changes may not.
   if(r.pid!=request.pid||r.match!=request.match||r.reveal!=request.reveal||r.mode!=request.mode||r.gpu!=request.gpu||r.selection.revision!=request.selection.revision)retired=std::move(latest);
  }
  request=r;paused=false;if(r.pid&&!thread.joinable()){stop=false;thread=std::thread([this]{run();});}cv.notify_one();}
 void pause(){std::lock_guard<std::mutex> lock(mutex);paused=true;cv.notify_one();}
 std::shared_ptr<const external::radar::Frame> get(){std::lock_guard<std::mutex> lock(mutex);return latest;}
 void shutdown(){std::shared_ptr<const external::radar::Frame> retired;{std::lock_guard<std::mutex> lock(mutex);stop=true;request={};retired=std::move(latest);++generation;cv.notify_one();}if(thread.joinable())thread.join();}
};
static radarpanel::Buffer paintBuffer,secondaryPaintBuffer;
static radarpanel::Gpu gpuPanel,secondaryGpuPanel;
static Worker worker;static HWND window,secondaryWindow;static std::shared_ptr<const external::radar::Frame> frame;static overlayui::Recovery recovery,secondaryRecovery;
static radargate::PaintGate<external::radar::Frame> primaryPaintGate,secondaryPaintGate;
struct PresentedWorld {
 uintptr_t game=0;DWORD pid=0;uint32_t match=0;int mode=0;bool reveal=false,valid=false;external::radar::Geometry geometry;
 bool same(uintptr_t window,DWORD process,uint32_t epoch,int selectedMode,const external::radar::Frame&value)const{
  return valid&&value.terrain&&game==window&&pid==process&&match==epoch&&mode==selectedMode&&reveal==value.terrain->reveal&&geometry.same(value.terrain->geo);
 }
 void commit(uintptr_t window,DWORD process,uint32_t epoch,int selectedMode,const external::radar::Frame&value){game=window;pid=process;match=epoch;mode=selectedMode;reveal=value.terrain->reveal;geometry=value.terrain->geo;valid=true;}
};
static PresentedWorld primaryWorld,secondaryWorld;
static secondarydisplay::Monitor secondaryMonitor;
static external::radar::Geometry lastGeometry;static bool dragging=false;static uint32_t dragMatch=0;static HWND dragWindow;
enum class SecondaryEdit {None,Move,Resize,ResizeUpperLeft};
static SecondaryEdit secondaryEdit=SecondaryEdit::None;
static POINT secondaryStartCursor{};static RECT secondaryStartRect{};
static int secondaryRequestedWidth=320;
static double secondaryAspect(){
 double rw=lastGeometry.valid()?lastGeometry.rawWidth:1,rh=lastGeometry.valid()?lastGeometry.rawHeight:1;
 if(minimapMode==1)rw*=2;return rw/rh;
}
static void finishSecondaryEdit(){
 if(secondaryEdit==SecondaryEdit::None)return;
 secondaryEdit=SecondaryEdit::None;
 if(GetCapture()==secondaryWindow)ReleaseCapture();
 if(!secondaryWindow)return;
 RECT r{};if(!GetWindowRect(secondaryWindow,&r))return;
 auto monitor=secondaryMonitor.get(game,ui,GetTickCount());if(!monitor.available())return;
 auto saved=secondaryradar::capture(monitor.work.left,monitor.work.top,monitor.work.right,monitor.work.bottom,
                                    {r.left,r.top,r.right-r.left,r.bottom-r.top},secondaryRequestedWidth);
 if(saved.x==minimapSecondaryX&&saved.y==minimapSecondaryY&&saved.width==minimapSecondaryWidth)return;
 minimapSecondaryX=saved.x;minimapSecondaryY=saved.y;minimapSecondaryWidth=saved.width;
 secondaryPaintGate.invalidate();syncSecondaryLayoutControls();changedSetting();
}
static void moveSecondaryEdit(){
 if(secondaryEdit==SecondaryEdit::None||!secondaryWindow)return;
 POINT current{};RECT r{};
 auto monitor=secondaryMonitor.get(game,ui,GetTickCount());
 if(!monitor.available()||!GetCursorPos(&current)||!GetWindowRect(secondaryWindow,&r))return;
 auto work=monitor.work;
 const int64_t workW=int64_t(work.right)-work.left,workH=int64_t(work.bottom)-work.top;
 if(workW<3||workH<3)return;
 const int margin=int(std::min<int64_t>(12,std::min((workW-3)/2,(workH-3)/2)));
 if(secondaryEdit==SecondaryEdit::Move){
  const int width=secondaryStartRect.right-secondaryStartRect.left,height=secondaryStartRect.bottom-secondaryStartRect.top;
  const int64_t leftBound=int64_t(work.left)+margin,topBound=int64_t(work.top)+margin;
  int left=int(std::clamp(int64_t(secondaryStartRect.left)+current.x-secondaryStartCursor.x,
                          leftBound,std::max(leftBound,int64_t(work.right)-margin-width)));
  int top=int(std::clamp(int64_t(secondaryStartRect.top)+current.y-secondaryStartCursor.y,
                         topBound,std::max(topBound,int64_t(work.bottom)-margin-height)));
  SetWindowPos(secondaryWindow,nullptr,left,top,0,0,SWP_NOACTIVATE|SWP_NOZORDER|SWP_NOSIZE);
 }else{
  bool upperLeft=secondaryEdit==SecondaryEdit::ResizeUpperLeft;
  int64_t delta=upperLeft?int64_t(secondaryStartCursor.x)-current.x:int64_t(current.x)-secondaryStartCursor.x;
  int requested=int(std::clamp(int64_t(secondaryStartRect.right-secondaryStartRect.left)+delta,
                                int64_t(160),int64_t(4096)));
  auto b=secondaryradar::place(work.left,work.top,work.right,work.bottom,0,0,requested,secondaryAspect());
  if(b.w<3||b.h<3)return;
  int64_t requestedX=upperLeft?int64_t(secondaryStartRect.right)-b.w:secondaryStartRect.left;
  int64_t requestedY=upperLeft?int64_t(secondaryStartRect.bottom)-b.h:secondaryStartRect.top;
  int x=int(std::clamp(requestedX,int64_t(work.left)+margin,int64_t(work.right)-margin-b.w));
  int y=int(std::clamp(requestedY,int64_t(work.top)+margin,int64_t(work.bottom)-margin-b.h));
  secondaryRequestedWidth=requested;
  SetWindowPos(secondaryWindow,nullptr,x,y,b.w,b.h,SWP_NOACTIVATE|SWP_NOZORDER);
 }
}
static void cancelDrag(){dragging=false;dragMatch=0;dragWindow=nullptr;auto captured=GetCapture();if((window&&captured==window)||(secondaryWindow&&captured==secondaryWindow))ReleaseCapture();}
static void hidePrimary(){if(dragWindow==window)cancelDrag();recovery.suspend();primaryPaintGate.invalidate();if(window)ShowWindow(window,SW_HIDE);}
static void hideSecondary(){if(secondaryEdit!=SecondaryEdit::None)finishSecondaryEdit();if(dragWindow==secondaryWindow)cancelDrag();secondaryRecovery.suspend();secondaryPaintGate.invalidate();if(secondaryWindow)ShowWindow(secondaryWindow,SW_HIDE);}
static void hide(){hidePrimary();hideSecondary();worker.pause();frame.reset();}
static void invalidateDisplays(){secondaryMonitor.invalidate();primaryPaintGate.invalidate();secondaryPaintGate.invalidate();}
static void navigate(HWND w,LPARAM lp){
 DWORD now=GetTickCount();bool requested=w==secondaryWindow?minimapSecondary:minimapEnabled;
 if(!shared||!enabled||!requested||!foregroundGame()||snap.chat||!commandFresh(now)||!frame||!frame->terrain||DWORD(now-frame->tick)>500||dragMatch!=commandMatch()){cancelDrag();return;}
 RECT r{};GetClientRect(w,&r);int x=0,y=0;if(!external::radar::navigate(frame->terrain->geo,short(LOWORD(lp)),short(HIWORD(lp)),r.right,r.bottom,std::max(1,snap.width-160),snap.height,x,y))return;
 InterlockedIncrement(&shared->cameraSeq);shared->cameraMatch=commandMatch();shared->cameraTick=sampleClock(shared);shared->cameraTargetX=x;shared->cameraTargetY=y;MemoryBarrier();InterlockedIncrement(&shared->cameraSeq);wakeGame();
}
static SecondaryEdit secondaryHit(HWND w,int x,int y){
 if(w!=secondaryWindow)return SecondaryEdit::None;
 RECT r{};GetClientRect(w,&r);
 if(x>=r.right-18&&y>=r.bottom-18)return SecondaryEdit::Resize;
 if(x>=0&&x<18&&y>=0&&y<18)return SecondaryEdit::ResizeUpperLeft;
 if(y>=0&&y<18)return SecondaryEdit::Move;
 return SecondaryEdit::None;
}
static void clear(){hide();worker.set({});lastGeometry={};primaryWorld={};secondaryWorld={};paintBuffer.clear();secondaryPaintBuffer.clear();if(window)gpuPanel.cpu(window);if(secondaryWindow)secondaryGpuPanel.cpu(secondaryWindow);}
static void destroy(){clear();worker.shutdown();if(window)DestroyWindow(window);if(secondaryWindow)DestroyWindow(secondaryWindow);window=secondaryWindow=nullptr;}
static bool presentation(int width,int height,std::shared_ptr<const external::radar::Scene>&image,radargate::Visual&visual){
 if(!frame||!frame->terrain||!frame->terrain->geo.valid()||width<3||height<3)return false;
 bool real=minimapMode==1&&bool(frame->scene);image=real?frame->scene:frame->map;if(!image)return false;
 const int mw=width-2,mh=height-2;visual={};visual.image=image;visual.width=width;visual.height=height;visual.real=real;
 // Diagram markers and radar feedback are already ordered by the worker.
 const auto&g=frame->terrain->geo;double left=(snap.cameraX/30.0+g.offsetX)/g.rawWidth,top=(snap.cameraY/15.0-g.offsetY)/g.rawHeight;
 double right=left+std::max(0,snap.width-160)/30.0/g.rawWidth,bottom=top+snap.height/15.0/g.rawHeight;
 visual.view={1+int(std::clamp(left,0.,1.)*(mw-1)),1+int(std::clamp(top,0.,1.)*(mh-1)),1+int(std::clamp(right,0.,1.)*(mw-1)),1+int(std::clamp(bottom,0.,1.)*(mh-1)),0};return true;
}
static LRESULT CALLBACK proc(HWND w,UINT msg,WPARAM wp,LPARAM lp){
 if(msg==WM_NCHITTEST)return HTCLIENT;
 if(msg==WM_SETCURSOR){
  if(w==secondaryWindow){POINT pointer{};GetCursorPos(&pointer);ScreenToClient(w,&pointer);
   auto hit=secondaryHit(w,pointer.x,pointer.y);
   SetCursor(LoadCursorW(nullptr,(hit==SecondaryEdit::Resize||hit==SecondaryEdit::ResizeUpperLeft)?IDC_SIZENWSE:hit==SecondaryEdit::Move?IDC_SIZEALL:IDC_ARROW));return TRUE;}
#ifdef RA2_UNIFIED
  if(!suite::hardwareCursorActive())SetCursor(nullptr);
#else
  SetCursor(nullptr);
#endif
  return TRUE;
 }
 if(msg==WM_LBUTTONDOWN){
  auto hit=secondaryHit(w,short(LOWORD(lp)),short(HIWORD(lp)));
  if(hit!=SecondaryEdit::None&&GetCursorPos(&secondaryStartCursor)&&GetWindowRect(w,&secondaryStartRect)){
   secondaryEdit=hit;secondaryRequestedWidth=minimapSecondaryWidth;SetCapture(w);return 0;
  }
  dragging=true;dragWindow=w;dragMatch=commandMatch();SetCapture(w);navigate(w,lp);return 0;
 }
 if(msg==WM_MOUSEMOVE){
  if(w==secondaryWindow&&secondaryEdit!=SecondaryEdit::None){if(wp&MK_LBUTTON)moveSecondaryEdit();else finishSecondaryEdit();return 0;}
  if(dragging&&dragWindow==w){if(!(wp&MK_LBUTTON)){cancelDrag();return 0;}navigate(w,lp);return 0;}
 }
 if(msg==WM_LBUTTONUP){if(w==secondaryWindow&&secondaryEdit!=SecondaryEdit::None){finishSecondaryEdit();return 0;}if(dragging&&dragWindow==w)navigate(w,lp);cancelDrag();return 0;}
 if(msg==WM_CAPTURECHANGED||msg==WM_CANCELMODE){if(w==secondaryWindow)finishSecondaryEdit();cancelDrag();return 0;}
 if(msg==WM_SIZE&&w==secondaryWindow&&secondaryEdit!=SecondaryEdit::None){secondaryPaintGate.invalidate();InvalidateRect(w,nullptr,FALSE);return 0;}
 if(msg==WM_RBUTTONDOWN||msg==WM_RBUTTONUP||msg==WM_LBUTTONDBLCLK)return 0;if(msg==WM_MOUSEACTIVATE)return MA_NOACTIVATE;if(msg==WM_ERASEBKGND)return 1;
 if(msg==WM_PAINT){PAINTSTRUCT ps{};HDC dc=BeginPaint(w,&ps);RECT r{};GetClientRect(w,&r);int width=r.right,height=r.bottom;
  auto&gpu=w==secondaryWindow?secondaryGpuPanel:gpuPanel;
  if(gpu.active){if(!gpu.ready(width,height)){std::shared_ptr<const external::radar::Scene>image;radargate::Visual visual;if(presentation(width,height,image,visual)&&image->gpu)gpu.present(w,*image,visual);}EndPaint(w,&ps);return 0;}
  auto&buffer=w==secondaryWindow?secondaryPaintBuffer:paintBuffer;
  // User resizing can arrive before the next regular update. Stage a whole
  // replacement first; ordinary exposure only copies the completed front.
  if(!buffer.ready(width,height)){std::shared_ptr<const external::radar::Scene>image;radargate::Visual visual;
   if(presentation(width,height,image,visual)&&buffer.stage(dc,*image,visual))buffer.promote();}
  buffer.present(dc,width,height);EndPaint(w,&ps);return 0;
 }return DefWindowProcW(w,msg,wp,lp);
}
static void placePanel(bool second,int x,int y,int width,int height){
 if(second&&secondaryEdit!=SecondaryEdit::None){RECT current{};if(!GetWindowRect(secondaryWindow,&current))return;x=current.left;y=current.top;width=current.right-current.left;height=current.bottom-current.top;}
 auto&panel=second?secondaryWindow:window;auto&keep=second?secondaryRecovery:recovery;
 auto&buffer=second?secondaryPaintBuffer:paintBuffer;auto&gate=second?secondaryPaintGate:primaryPaintGate;
 auto&gpu=second?secondaryGpuPanel:gpuPanel;
 auto&world=second?secondaryWorld:primaryWorld;
 if(!panel){WNDCLASSW cls{};cls.lpfnWndProc=proc;cls.hInstance=GetModuleHandleW(nullptr);cls.lpszClassName=L"RA2DataRadar127";cls.hCursor=nullptr;RegisterClassW(&cls);panel=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE|WS_EX_TOPMOST|WS_EX_LAYERED,cls.lpszClassName,second?L"第二显示器小地图":L"战术小地图",WS_POPUP,0,0,1,1,nullptr,nullptr,cls.hInstance,nullptr);if(panel)SetLayeredWindowAttributes(panel,0,255,LWA_ALPHA);}
 if(!panel)return;RECT old{};GetWindowRect(panel,&old);
 bool compatible=frame&&world.same(uintptr_t(game),pid,displayMatch(),minimapMode,*frame);
 bool visible=IsWindowVisible(panel)!=FALSE;
 // Retention is only for this exact displayed world/reveal policy. A new
 // worker can publish before the UI observes its intermediate empty state.
 if(!compatible){if(dragWindow==panel)cancelDrag();if(visible)ShowWindow(panel,SW_HIDE);visible=false;keep.suspend();gate.invalidate();}
 bool repair=keep.due(GetTickCount(),true,{uintptr_t(game),pid,displayMatch(),width,height},visible);
 bool moved=old.left!=x||old.top!=y||old.right-old.left!=width||old.bottom-old.top!=height;
 std::shared_ptr<const external::radar::Scene>image;radargate::Visual visual;if(!presentation(width,height,image,visual))return;
 if(image->gpu){
  // Main/secondary panels retain the same completed shared texture. A busy
  // presenter retries next cadence without publishing half a panel or GDI.
  const bool changed=gate.needsVisual(visual)||!gpu.ready(width,height)||gpu.pending;
  if((changed||repair||!visible)&&!gpu.present(panel,*image,visual)){keep.submitted(false);return;}
  if(repair||moved||!visible){bool placed=overlaystack::place(panel,overlaystack::Layer::Minimap,x,y,width,height,SWP_NOACTIVATE|SWP_NOREDRAW|SWP_SHOWWINDOW)!=FALSE;keep.submitted(placed);if(!placed)return;}
  gate.committedVisual(visual);world.commit(uintptr_t(game),pid,displayMatch(),minimapMode,*frame);return;
 }
 bool changed=gate.needsVisual(visual)||!buffer.ready(width,height);
 if(changed){HDC dc=GetDC(panel);bool ready=dc&&buffer.stage(dc,*image,visual);if(dc)ReleaseDC(panel,dc);if(!ready){keep.submitted(false);return;}}
 if(changed)buffer.promote(); // any synchronous position callback sees the new complete pixels
 gpu.cpu(panel);
 // A complete backing exists before the first SHOWWINDOW or resize. Suppress
 // positioning-triggered redraw/erasure; WM_PAINT copies the completed front.
 if(repair||moved||!visible){bool placed=overlaystack::place(panel,overlaystack::Layer::Minimap,x,y,width,height,SWP_NOACTIVATE|SWP_NOREDRAW|SWP_SHOWWINDOW)!=FALSE;keep.submitted(placed);if(!placed)return;}
 if(changed||repair||moved||!visible||!buffer.painted){buffer.painted=false;
  if(InvalidateRect(panel,nullptr,FALSE)){UpdateWindow(panel);if(buffer.painted){gate.committedVisual(visual);world.commit(uintptr_t(game),pid,displayMatch(),minimapMode,*frame);}else{keep.submitted(false);if(!compatible)ShowWindow(panel,SW_HIDE);}}
  else keep.submitted(false);
 }
}
static void update(POINT origin,const RECT&client,bool verifiedBattlefield){
 bool valid=verifiedBattlefield&&pid&&snap.renderStatus==1&&snap.width>=320&&snap.height>=200;
 bool primary=valid&&enabled&&minimapEnabled&&foregroundGame()&&!snap.chat&&client.right>=320&&client.bottom>=200;
 auto target=minimapSecondary?secondaryMonitor.get(game,ui,GetTickCount()):secondarydisplay::Target{};
 bool secondary=valid&&minimapSecondary&&target.available();
 if(!primary)hidePrimary();if(!secondary)hideSecondary();if(!primary&&!secondary){worker.pause();frame.reset();return;}
 auto latest=worker.get();if(latest&&latest->terrain)lastGeometry=latest->terrain->geo;
 double rw=lastGeometry.valid()?lastGeometry.rawWidth:1,rh=lastGeometry.valid()?lastGeometry.rawHeight:1;if(minimapMode==1)rw*=2;double aspect=rw/rh;
 auto native=minimapMode==1?lastGeometry.nativeViewport():external::mapbounds::NativeViewport{};
 auto layout=primary?radarlayout::place(snap.width,snap.height,client.right,client.bottom,minimapScale,minimapPosition,minimapX,minimapY,aircraftButtonBox,aspect,minimapAlignX,minimapAlignY,native.width,native.height):radarlayout::Layout{};
 if(primary&&!layout.valid){primary=false;hidePrimary();}
 radarlayout::Box secondaryBox;
 if(secondary){
  secondaryBox=secondaryradar::place(target.work.left,target.work.top,target.work.right,target.work.bottom,
                                     minimapSecondaryX,minimapSecondaryY,minimapSecondaryWidth,aspect);
  if(secondaryBox.w<3||secondaryBox.h<3){secondary=false;hideSecondary();}
 }
 if(!primary&&!secondary){worker.pause();frame.reset();return;}
 // Render once at the largest visible panel size, then present that completed
 // image to each window. Enlarging the desktop panel does not stretch the
 // smaller game-panel raster into a blurry preview.
 int width=std::max(primary?layout.destination.w:0,secondary?secondaryBox.w:0);
 int height=std::max(primary?layout.destination.h:0,secondary?secondaryBox.h:0);
 worker.set({pid,displayMatch(),informationFPS(),uint32_t(extra.renderReveal||assist::shroudpolicy::fullPreview(extra.mapStrategy)),minimapMode,std::min(2048,width-2),std::min(2048,height-2),renderVulkanEnabled,external::presentation::settings().get()});
 frame=worker.get();if(!frame||!frame->terrain){cancelDrag();hidePrimary();hideSecondary();return;}if(dragging&&dragMatch!=commandMatch())cancelDrag();
 if(primary){auto d=layout.destination;placePanel(false,origin.x+d.x,origin.y+d.y,d.w,d.h);}
 if(secondary)placePanel(true,secondaryBox.x,secondaryBox.y,secondaryBox.w,secondaryBox.h);
}
}
