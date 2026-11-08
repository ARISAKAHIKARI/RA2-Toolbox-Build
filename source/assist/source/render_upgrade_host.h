#pragma once
#include "../../shared/source/latest_worker.h"
#include "../../shared/source/recurring_worker.h"
#include "../../shared/source/frame_signal_client.h"
#include "../../shared/source/overlay_compositor.h"
#include "../../shared/source/physical_window.h"
#include "../../external_reader/source/main_viewport.h"
#include "../../external_reader/source/feedback_scene.h"
#include "../../external_reader/source/feedback_orders.h"
#include "../../external_reader/source/feedback_cursor.h"
#include "../../external_reader/source/native_effect_stencil.h"
#include "../../external_reader/source/native_beam_effects.h"
#include "../../external_reader/source/native_unit_ui_stencil.h"
#include "../../external_reader/source/local_ui_task.h"
#include "../../external_reader/source/material_motion.h"
#include "../../external_reader/source/shared_scene.h"
#include "../../external_reader/source/scene_poses.h"
#include <chrono>
#include "render_presentation.h"
#include "native_viewport_host.h"
#include "render_observation_probe.h"
#include "../../shared/source/pipeline_timing.h"
#include "sidebar_indicator_host.h"
// External read/render; the sampler observes native frame-number changes.
// No engine drawing or feature command is dispatched by this pipeline.
namespace renderupgrade {
static double presentationNow(){return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();}
struct Request {uint64_t generation=0;bool materialRefresh=true;int detail=1,colorPrecision=1;uint32_t pid=0,match=0,tick=0;int width=0,height=0;uint32_t reveal=0;int paletteRange=1,pixelScaleMode=0;external::mainview::View view;overlaycompositor::Context context;bool gpu=false;double decodeBudgetMs=4.;int displayRate=60;external::presentation::Profile profile;};
struct Frame {
 double sampledAtMs=0,completedAtMs=0,sampleToDrawMs=0;
 int frameSignalState=0;double signalDelayMs=0;
 double sampleMs=0,sceneMs=0,finishMs=0,gpuPrepareMs=0,gpuWaitMs=0,gpuCopyMs=0;
 double terrainMs=0,palettesMs=0,decodeStageMs=0,voxelMs=0,objectsMs=0,resolveMs=0,effectsMs=0;
 std::array<double,4>compositionMs{};
 uint32_t renderReadCalls=0,renderPageHits=0,drawnObjects=0,requestedObjects=0;uint64_t paletteSamples=0,paletteHits=0;
 std::string gpuDevice,gpuReason;bool gpuSoftware=false,gpuReadbackCached=false;
 uint32_t readCalls=0,pageHits=0,prefetchedPages=0,decodeAttempts=0;double decodeMs=0;
 Request request;std::shared_ptr<const external::radar::Scene> image;
 std::shared_ptr<const external::worldscene::Snapshot> world;
 std::vector<external::nativefeedback::Object> objects;
 bool gpuAttempted=false,gpuUsed=false,sceneReused=false;
 uint64_t assetRevision=0;uint32_t sampledTick=0,completedTick=0;bool hasCompletionGap=false;int paddingX=0,paddingY=0;double renderMs=0,observationMs=0,stateIntervalMs=0,completionGapMs=0;int logicFrame=-1;double yellow=.5,red=.25;external::nativefeedback::EffectStatus effects;
};
static external::ReadSession reader;
static external::cameraassets::Context<external::ReadSession> assets(reader);
static external::radar::SceneRenderer<decltype(assets)> renderer;
// Reading owns a separate page epoch and never waits for asset decoding, GPU
// completion or a layered-window upload. Only immutable banks cross threads.
struct Observation {
 Request request;external::mainview::View view;int frameSignalState=0;double signalDelayMs=0;
 std::shared_ptr<const external::radar::Frame> current;
 double observedMs=0,readMs=0,intervalMs=0,stateObservedMs=0,stateIntervalMs=0;uint32_t tick=0,readCalls=0,pageHits=0,prefetchedPages=0;

};
static external::ReadSession sampleReader;
static external::materialmotion::Timeline preparationMotion;static uint32_t process=0,epoch=0;static bool active=false;static uint64_t imageToken=0;
static constexpr UINT ReadyMessage=WM_APP+20;
static std::atomic<bool> readyPending{false};
static void notifyReady(){if(!readyPending.exchange(true)&&!PostMessageW(ui,ReadyMessage,0,0))readyPending=false;}
static std::atomic<int> progress{0};
static uint64_t generation=1,workerGeneration=0;
static double handedOffFrameMs=0,completionToHandoffMs=0;
static renderpresentation::ViewLease viewLease;
static renderpresentation::GroupLease<Request,Frame> groupLease;
static int verifiedScaleSetting=-1;static bool verifiedPhysicalSetting=false;
static renderpresentation::StatusCadence statusCadence;
static double lastFrameCompleteMs=0;static bool hasFrameCompleteTime=false;
static std::wstring lastStatusText;
static void statusText(const std::wstring&text){
 if(lastStatusText==text)return;lastStatusText=text;SetWindowTextW(GetDlgItem(ui,9303),text.c_str());
}
static asyncwork::Recurring<Request,Observation> samplingWorker([](const Request&r)->std::shared_ptr<const Observation>{
 static pipelineperf::Window timings(L"world-observation",{L"attempt",L"duplicate",L"capture"});pipelineperf::Scope attempt(timings,0,critical);
 static std::shared_ptr<const Observation> last;
 static uint64_t sampleGeneration=0;static uint32_t sampleProcess=0,sampleMatch=0,sampleReveal=~0u;
 if(sampleGeneration!=r.generation||sampleProcess!=r.pid||sampleMatch!=r.match||sampleReveal!=r.reveal){
  sampleReader.close();last.reset();sampleGeneration=r.generation;sampleProcess=r.pid;sampleMatch=r.match;sampleReveal=r.reveal;
 }
 if(!sampleReader.open(r.pid))return {};sampleReader.begin();
 external::radar::FrameScope scope(sampleReader,r.match);if(!scope)return {};
 const auto token=sampleReader.worldFrameToken();
 if(last&&last->current->sourceFrame&&token&&last->current->sourceFrame->key.serial==token->key.serial&&
    last->current->sourceFrame->key.sameFrame(token->key)&&renderpresentation::sameProjection(last->view,r.view)){timings.add(1,0);return {};}
 const double captureStarted=presentationNow();
 auto live=[](uint32_t p,void*out,size_t n){return sampleReader.readLive(p,out,n);};
 external::mainview::View before;
 if(!external::mainview::sample(live,before,r.view.transform)||!renderpresentation::sameProjection(before,r.view)||before.frame<r.view.frame)return {};
 auto bank=external::radar::sharedScene(sampleReader,r.pid,r.match,r.reveal,GetTickCount(),external::radar::SceneConsumer::Main,true,r.profile.particles,r.profile.lasers);
 if(!bank||!external::radar::recentMarkers(*bank,external::radar::publicationContext(live),GetTickCount()))return {};
 if(last&&last->current==bank&&renderpresentation::sameProjection(last->view,before)){timings.add(1,0);return {};}
 pipelineperf::Scope capture(timings,2);
 auto out=std::make_shared<Observation>();out->request=r;out->view=before;out->frameSignalState=0;
 out->tick=bank->tick;out->observedMs=bank->observedMs;out->readMs=presentationNow()-captureStarted;
 out->intervalMs=last?out->observedMs-last->observedMs:0;
 out->readCalls=sampleReader.reads;out->pageHits=sampleReader.pageHits;out->prefetchedPages=sampleReader.prefetchedPages;
 out->current=std::move(bank);out->stateObservedMs=out->observedMs;
 if(last&&out->current->frame>=last->current->frame&&out->current->terrain->geo.same(last->current->terrain->geo)){
  if(out->current->frame>last->current->frame)out->stateIntervalMs=out->observedMs-last->stateObservedMs;
  else{out->stateObservedMs=last->stateObservedMs;out->stateIntervalMs=last->stateIntervalMs;}
 }
 last=out;return out;
});
static external::worldscene::Pipeline worldPipeline;
static asyncwork::Recurring<Request,Frame> worker([](const Request&r)->std::shared_ptr<const Frame>{
 static pipelineperf::Window timings(L"world-render",{L"attempt",L"prepare",L"compose",L"completed"});pipelineperf::Scope attempt(timings,0,critical);
 const double started=presentationNow();
 external::xbrzadapter::gpuPresentation(r.gpu);
 // Failed attempts do not replace a completed image. The UI independently
 // enforces sample/completion age limits and exact world/view/config identity.
 auto unavailable=[]()->std::shared_ptr<const Frame>{return {};};
 if(process!=r.pid||epoch!=r.match){reader.close();renderer.reset();worldPipeline.reset();process=r.pid;epoch=r.match;hasFrameCompleteTime=false;}
 if(workerGeneration!=r.generation){workerGeneration=r.generation;hasFrameCompleteTime=false;preparationMotion.reset();worldPipeline.reset();}
 if(!reader.open(r.pid)){progress=-1;return unavailable();}
 reader.begin(); // Explicit fresh page epoch for every presentation request.
 auto fresh=[](uint32_t p,void*out,size_t n){return reader.readLive(p,out,n);};
 external::mainview::View before;
 if(!external::mainview::sample(fresh,before,r.view.transform)||!renderpresentation::sameProjection(before,r.view)||before.frame<r.view.frame){progress=-1;return unavailable();}
 // Requests can wait behind a CPU render. Sample the current camera rather
 // than discarding every queued job while the player continuously scrolls.
 Request sampledRequest=r;sampledRequest.view=before;
 sampledRequest.context.cameraX=before.cameraX;sampledRequest.context.cameraY=before.cameraY;
 progress=1;
 const auto observed=samplingWorker.get();
 if(!observed||observed->request.generation!=r.generation||observed->request.pid!=r.pid||
    observed->request.match!=r.match||observed->request.reveal!=r.reveal||
    !renderpresentation::sameProjection(observed->view,before)||
    !external::radar::recentMarkers(*observed->current,external::radar::publicationContext(fresh),GetTickCount()))return unavailable();
 external::radar::FrameScope worldScope(reader,r.match,observed->current->sourceFrame);if(!worldScope)return unavailable();
 assets.identity(r.pid,r.match);
 const double sampledMs=presentationNow();
 // Sampling, world drawing and presentation have independent clocks. Reuse
 // the latest immutable state freely; the native logic frame is an identity/
 // freshness guard, never a render permit. Motion uses a bounded latest-state
 // presentation estimate, without buffering a complete observation interval.
 preparationMotion.observe(*observed->current,presentationNow());
 auto data=preparationMotion.sampleLatest(presentationNow(),r.profile.motion,r.detail!=0);
 if(!data.terrain)return unavailable();
 const uint32_t sampledTick=observed->tick;
 data.tick=GetTickCount(); // presentation retry deadlines keep running while paused
 renderer.features(r.profile);assets.presentationLighting(r.profile.voxelLighting,r.profile.spriteLighting);
 assets.presentationDetail(r.detail);
 const auto&gpuBackend=external::xbrzadapter::gpuState().resolve;
 const bool gpuUnavailable=gpuBackend&&!gpuBackend->diagnostics().available&&!gpuBackend->diagnostics().reason.empty();
 assets.presentationGPU(r.gpu&&!gpuUnavailable&&!external::vulkancompute::Resolve::driverQuarantined());
 assets.presentationResolution(r.view.width,r.view.height,r.width,r.height);
 assets.presentationColorPrecision(r.colorPrecision!=0);
 assets.presentationPalette(r.paletteRange==1?external::pixelupgrade::PaletteRange::FullRGB:external::pixelupgrade::PaletteRange::OriginalDAC);
 const uint64_t assetRevision=assets.revision();
 // Lighting, visibility and materials are decoded from the same immutable
 // publication as these poses. Display-rate camera changes only reproject it;
 // new world facts arrive with the next Runtime publication.

 // Every draw uses CURRENT observed membership and visibility. Removed or
 // hidden actors are never resurrected from a prior interpolation endpoint.

 // Vera-style retained world preparation: a camera never rebuilds or clips
 // the world cache. The same render owner resolves the final viewport.
 const auto renderView=before;
 auto crop=external::mainview::crop(renderView,data.terrain->geo.offsetX,data.terrain->geo.offsetY);
 renderer.guideEnabled(false,r.generation);renderer.directPresentation(false);renderer.prepareWorld(true);
 pipelineperf::Scope prepare(timings,1);
 auto scene=renderer.render(data,r.width,r.height,external::SharedReadView{&reader},assets,{crop.x,crop.y,crop.width,crop.height,true},unsigned(r.pixelScaleMode),r.detail!=0,true,r.colorPrecision!=0,true,r.decodeBudgetMs);prepare.finish();
 if(!scene||!reader.worldFrameComplete())return unavailable();
 const double sceneDoneMs=presentationNow();
 auto out=std::make_shared<Frame>();out->world=renderer.preparedWorld();if(!out->world)return unavailable();out->request=sampledRequest;out->paddingX=out->paddingY=0;out->assetRevision=assetRevision;out->logicFrame=data.frame;out->observationMs=observed->intervalMs;out->stateIntervalMs=observed->stateIntervalMs;out->frameSignalState=observed->frameSignalState;out->signalDelayMs=observed->signalDelayMs;
 out->gpuAttempted=external::xbrzadapter::gpuAttempted();out->gpuUsed=external::xbrzadapter::gpuUsed();
 const auto coverage=renderer.objectCoverage();out->drawnObjects=coverage.first;out->requestedObjects=coverage.second;
 out->readCalls=observed->readCalls;out->pageHits=observed->pageHits;out->prefetchedPages=observed->prefetchedPages;
 out->decodeAttempts=renderer.decodeStats().attempts;out->decodeMs=renderer.decodeStats().milliseconds;
 out->sampleMs=observed->readMs;out->sceneMs=sceneDoneMs-sampledMs;
 out->sampledAtMs=observed->observedMs;out->sampleToDrawMs=std::max(0.,sampledMs-observed->observedMs);
 const auto&stages=renderer.stageStats();out->sceneReused=stages.reused;out->terrainMs=stages.terrainMs;out->palettesMs=stages.palettesMs;out->decodeStageMs=stages.decodeMs;out->voxelMs=stages.voxelMs;out->objectsMs=stages.objectsMs;out->resolveMs=stages.resolveMs;out->effectsMs=stages.effectsMs;
 out->renderReadCalls=reader.reads;out->renderPageHits=reader.pageHits;out->paletteSamples=assets.paletteSamples();out->paletteHits=assets.paletteSampleHits();
 if(gpuBackend){const auto&d=gpuBackend->diagnostics();out->gpuDevice=d.device;out->gpuReason=d.reason;out->gpuSoftware=d.softwareDevice;out->gpuReadbackCached=d.readbackCached;
  if(out->gpuUsed&&!out->sceneReused){out->gpuPrepareMs=d.prepareMilliseconds;out->gpuWaitMs=d.waitMilliseconds;out->gpuCopyMs=d.readbackMilliseconds;}}
 out->yellow=assets.conditionYellow();out->red=assets.conditionRed();
 out->effects.complete=!r.profile.lasers;
 out->effects.independentLasers=r.profile.lasers&&data.effects&&data.effects->world&&data.effects->world->lasersValid;
 external::mainview::View after;
 if(!external::mainview::sample(fresh,after,r.view.transform)||!renderpresentation::sameProjection(before,after)||after.frame<before.frame||
   !external::radar::sameWorld(data,external::radar::publicationContext(fresh)))return unavailable();
 // One render owner prepares AND resolves the whole world. Restore the
 // original completed-frame handoff instead of passing GPU/material owners
 // between separate preparation and presentation workers.
 const double compositionStarted=presentationNow();
 pipelineperf::Scope compose(timings,2);auto image=worldPipeline.draw(out->world,after,r.width,r.height,external::SharedReadView{&reader},nullptr,GetTickCount());compose.finish();
 if(!image||!reader.worldFrameComplete())return unavailable();
 external::mainview::View committed;
 if(!external::mainview::sample(fresh,committed,r.view.transform)||!renderpresentation::sameProjection(after,committed)||committed.frame<after.frame||
    !external::radar::sameWorld(data,external::radar::publicationContext(fresh)))return unavailable();
 out->request.view=after;out->request.context.cameraX=after.cameraX;out->request.context.cameraY=after.cameraY;
 out->objects=external::nativefeedback::objectGeometry(*out->world,worldPipeline.presentedState(),after,r.width,r.height,out->world->zscale);
 out->effects.complete=!r.profile.lasers||image->boltsRendered;
 out->gpuAttempted=external::xbrzadapter::gpuAttempted();out->gpuUsed=external::xbrzadapter::gpuUsed();
 out->sceneReused=worldPipeline.reused();out->compositionMs=worldPipeline.timings();
 if(const auto&backend=external::xbrzadapter::gpuState().resolve){const auto&d=backend->diagnostics();
  out->gpuDevice=d.device;out->gpuReason=d.reason;out->gpuSoftware=d.softwareDevice;out->gpuReadbackCached=d.readbackCached;
  out->gpuPrepareMs=d.prepareMilliseconds;out->gpuWaitMs=d.waitMilliseconds;out->gpuCopyMs=d.readbackMilliseconds;
 }
 out->image=std::move(image);out->sampledTick=sampledTick;out->completedTick=GetTickCount();const double completedMs=presentationNow();out->completedAtMs=completedMs;out->renderMs=completedMs-started;out->finishMs=completedMs-compositionStarted;
 out->hasCompletionGap=hasFrameCompleteTime;out->completionGapMs=hasFrameCompleteTime?completedMs-lastFrameCompleteMs:0;
 lastFrameCompleteMs=completedMs;hasFrameCompleteTime=true;progress=2;timings.add(3,out->renderMs);return out;
},notifyReady);
#include "render_feedback_host.h"
static void suspendScene(){groupLease.reset();if(active){active=false;++generation;worker.invalidate();samplingWorker.invalidate();}feedback::hide();imageToken=0;overlaycompositor::host().hide(overlaycompositor::RenderUpgrade);}
static void suspend(){viewLease.reset();suspendScene();sidebarhud::stop();}
static void stop(){suspend();overlaycompositor::host().suspend();}
static void shutdown(){stop();worker.shutdown();samplingWorker.shutdown();feedback::shutdown();sidebarhud::shutdown();overlaycompositor::host().shutdown();}
static bool syncRadarExclusion(const RECT&client){
 int width=0,height=0;const int status=shared?shared->sidebarStatus:0;const uint32_t mode=shared?shared->sidebarMode:0;
 if(status==3&&(!externalReader.readFresh(0x8A00A4,&width,4)||!externalReader.readFresh(0x8A00A8,&height,4)))return false;
 nativeviewportprojection::Rect radar{};
 if(!nativeviewhost::radarOcclusion(shared,[](uint32_t at,void*out,size_t n){return externalReader.readFresh(at,out,n);},width,height,radar))return false;
 overlaycomposition::Rect excluded{};
 if(radar.w>0&&radar.h>0){const int left=int(int64_t(radar.x)*client.right/width),top=int(int64_t(radar.y)*client.bottom/height);
  const int right=int((int64_t(radar.x+radar.w)*client.right+width-1)/width),bottom=int((int64_t(radar.y+radar.h)*client.bottom+height-1)/height);excluded={left,top,right-left,bottom-top};}
 int afterW=0,afterH=0;
 if(status==3&&(!externalReader.readFresh(0x8A00A4,&afterW,4)||!externalReader.readFresh(0x8A00A8,&afterH,4)||afterW!=width||afterH!=height))return false;
 if(shared&&(shared->sidebarStatus!=status||shared->sidebarMode!=mode||shared->viewZoomBusy))return false;
 overlaycompositor::host().setMapExclusion(excluded);return true;
}
static void update(POINT origin,const RECT&client,const external::display::State&displayState){
 static const bool diagnostics=[](){overlaycompositor::host().setDiagnosticsSink(critical);return true;}();(void)diagnostics;
 if(!overlayvisibility::visible()){stop();return;}
 const auto selected=external::presentation::settings().get();static uint64_t profileRevision=~uint64_t(0);
 if(profileRevision!=selected.revision){profileRevision=selected.revision;++generation;worker.invalidate();samplingWorker.invalidate();groupLease.reset();}

 // The whole tool remains system-DPI aware. A physical-pixel option must
 // explicitly validate monitor geometry instead of assuming a 4K monitor
 // makes a 2K client window magically 4K.
 const bool requested=renderUpgradeEnabled||unifiedOverlayEnabled||harvesterCounterEnabled||powerDeltaEnabled;
 const bool visible=requested&&enabled&&foregroundGame()&&!snap.chat&&pid&&game&&
  (!displayState.readable||displayState.visible());
 const uint32_t viewNow=GetTickCount(),match=displayMatch();
 if(!renderUpgradeEnabled)suspendScene();
 if(!visible){
  overlaycompositor::host().configure(uintptr_t(game),pid,match,client.right,client.bottom,0,0,false,false,informationFPS());
  suspend();progress=0;return;
 }
 // Refresh native radar holes before both fresh and retained-frame branches.
 if(!syncRadarExclusion(client)){suspend();overlaycompositor::host().suspend();progress=0;return;}
 nativeviewportprojection::Transform transform;
 if(!nativeviewhost::sampleTransform(shared,transform)){
  // A torn A/R publication is never an identity viewport or a grace lease.
  viewLease.reset();overlaycompositor::host().configure(uintptr_t(game),pid,match,client.right,client.bottom,0,0,false,false,informationFPS());suspend();progress=0;return;
 }
 external::mainview::View view;
 if(!displayState.readable||!nativeviewhost::sample(shared,[](uint32_t p,void*out,size_t n){return externalReader.readFresh(p,out,n);},view,transform)){
  // Unknown is not known-hidden. A short torn read must not toggle the HWND
  // or invalidate an in-flight job; known menus/focus/world changes do so above.
  if(nativeviewhost::stable(shared,transform)&&viewLease.retain(uintptr_t(game),pid,match,client.right,client.bottom,viewNow,transform)){
   const auto&prior=viewLease.view();
   overlaycompositor::host().configure(uintptr_t(game),pid,match,client.right,client.bottom,
    prior.cameraX,prior.cameraY,true,false,informationFPS(),renderUpgradeEnabled,nativeviewhost::key(transform));
   if(renderUpgradeEnabled){
    auto held=groupLease.frame();
    if(held){
     Request current=held->request;current.generation=generation;current.pid=pid;current.match=match;current.view=prior;
     current.materialRefresh=effectiveRenderInterpolation();current.detail=effectiveRenderDetail();current.colorPrecision=effectiveRenderPrecision();
     current.paletteRange=effectiveRenderPalette();current.reveal=uint32_t(extra.renderReveal||assist::shroudpolicy::fullPreview(extra.mapStrategy));
     current.gpu=renderVulkanEnabled;
     // Scale-mode changes require a fresh verified request, even during a read gap.
     if(verifiedScaleSetting!=effectiveRenderScale()||verifiedPhysicalSetting!=renderPhysicalOutput)groupLease.reset();
     if(groupLease.retainWorld(current,viewNow)){
      overlaycompositor::host().replaceGroupAt(groupLease.context(),{overlaycompositor::RenderUpgrade,feedback::SelectionLayer,feedback::CursorLayer,feedback::MessageLayer,feedback::TimerLayer,feedback::StatusLayer,feedback::RallyLayer,feedback::PlanningHighlightLayer},
       renderpresentation::worldOnly(groupLease.layers(),overlaycompositor::RenderUpgrade));
     }else{groupLease.reset();feedback::hide();imageToken=0;}
    }
   }
   if(statusCadence.due((generation<<4)|8,viewNow))statusText(L"渲染升级：等待完整视口采样");return;
  }
  overlaycompositor::host().configure(uintptr_t(game),pid,match,client.right,client.bottom,0,0,false,false,informationFPS());
  suspend();progress=0;return;
 }
 viewLease.observe(uintptr_t(game),pid,match,client.right,client.bottom,view,viewNow);
 physicalpresentation::Result physical{};
 static physicalpresentation::Result cachedPhysical{};
 static HWND cachedGame=nullptr;
 static int cachedClientW=0,cachedClientH=0,cachedSourceW=0,cachedSourceH=0;
 static POINT cachedOrigin{};
 static DWORD physicalCheckedAt=0;
 const DWORD checkNow=GetTickCount();
 if(visible&&renderPhysicalOutput){
  if(game!=cachedGame||client.right!=cachedClientW||client.bottom!=cachedClientH||
     view.screenW!=cachedSourceW||view.screenH!=cachedSourceH||
     origin.x!=cachedOrigin.x||origin.y!=cachedOrigin.y||
     DWORD(checkNow-physicalCheckedAt)>=250){
   cachedPhysical=physicalpresentation::inspect(game,view.screenW,view.screenH,origin,client);
   cachedGame=game;cachedClientW=client.right;cachedClientH=client.bottom;
   cachedSourceW=view.screenW;cachedSourceH=view.screenH;
   cachedOrigin=origin;physicalCheckedAt=checkNow;
  }
  physical=cachedPhysical;
 }
 // Physical enhancement eligibility is separate from window visibility.
 // A classic exclusive 2560x1440 game can still show our established 2K
 // overlay. Never disable that overlay merely because the monitor switched
 // out of its configured 4K mode.
 const auto renderPath=physicalpresentation::renderPath(physical,renderPhysicalOutput);
 auto context=overlaycompositor::host().configure(uintptr_t(game),pid,displayMatch(),client.right,client.bottom,
  visible?view.cameraX:0,visible?view.cameraY:0,visible,false,informationFPS(),renderUpgradeEnabled,nativeviewhost::key(view.transform));
 sidebarhud::update(pid,displayMatch(),view,context,harvesterCounterEnabled,powerDeltaEnabled);
 if(!renderUpgradeEnabled){progress=0;return;}
 active=true;
 const auto area=view.physical();
 const int left=int((int64_t(area.x)*client.right)/view.screenW),top=int((int64_t(area.y)*client.bottom)/view.screenH);
 const int right=int((int64_t(area.x+area.w)*client.right+view.screenW-1)/view.screenW);
 const int bottom=int((int64_t(area.y+area.h)*client.bottom+view.screenH-1)/view.screenH);
 // Scaling is explicit. Physical output never silently enables xBRZ.
 const int effectiveScale=effectiveRenderScale();
 verifiedScaleSetting=effectiveScale;verifiedPhysicalSetting=renderPhysicalOutput;
 Request request{generation,effectiveRenderInterpolation(),effectiveRenderDetail(),effectiveRenderPrecision(),pid,displayMatch(),GetTickCount(),right-left,bottom-top,
  uint32_t(extra.renderReveal||assist::shroudpolicy::fullPreview(extra.mapStrategy)),
  effectiveRenderPalette(),effectiveScale,view,context};
 request.displayRate=informationFPS();request.profile=selected.profile;request.gpu=selected.profile.gpu;request.decodeBudgetMs=std::clamp(500./informationFPS(),1.,6.);
 const Request liveRequest=request;
 auto result=worker.get();
 // Publish the complete frame under the camera used to draw ALL its passes.
 // Camera lag is bounded by completion age. Never move, stretch or pad a
 // completed image to masquerade as the next camera's newly exposed pixels.
 const bool fresh=result&&renderpresentation::completeUsable(*result,request,GetTickCount());
 if(fresh){request.view=result->request.view;request.context.cameraX=request.view.cameraX;request.context.cameraY=request.view.cameraY;}
 std::vector<overlaycompositor::Surface> layers;
 const bool feedbackReady=fresh&&feedback::update(request,result,left,top,layers);
 auto publishContext=request.context;
 std::shared_ptr<const Frame> presentedFrame=result;bool displaying=false;
 if(fresh&&!feedback::unsafeWorld){
  if(!feedbackReady){layers.clear();feedback::nativeCutouts.reset();}
  auto pixels=std::shared_ptr<const std::vector<uint32_t>>(result->image,&result->image->pixels);
  layers.push_back({overlaycompositor::RenderUpgrade,5,request.context,
   {left,top,result->image->width,result->image->height},true,255,std::move(pixels),
   {left,top,request.width,request.height},feedback::nativeCutouts,result->image->gpu});imageToken=context.token;
  for(auto&layer:layers)layer.clip={left,top,request.width,request.height};
  groupLease.publish(request,result,layers,GetTickCount());displaying=true;
 }else if(!feedback::unsafeWorld&&groupLease.retainWorld(liveRequest,GetTickCount())){
  // Preserve only the completed world across a pending/failed draw.
  // Failure never refreshes this lease's deadline or any native hole.
  layers=groupLease.layers();presentedFrame=groupLease.frame();publishContext=groupLease.context();
  // A complete image keeps its own camera. Never stretch or translate it,
  // and never expose native pixels or stale interaction layers from that view.
  layers=renderpresentation::worldOnly(std::move(layers),overlaycompositor::RenderUpgrade);displaying=true;
 }else{layers.clear();groupLease.reset();imageToken=0;if(!fresh)feedback::hide();}
 overlaycompositor::host().replaceGroupAt(publishContext,{overlaycompositor::RenderUpgrade,feedback::SelectionLayer,feedback::CursorLayer,feedback::MessageLayer,feedback::TimerLayer,feedback::StatusLayer,feedback::RallyLayer,feedback::PlanningHighlightLayer},std::move(layers));
 if(displaying&&presentedFrame&&presentedFrame->completedAtMs!=handedOffFrameMs){handedOffFrameMs=presentedFrame->completedAtMs;completionToHandoffMs=std::max(0.,presentationNow()-handedOffFrameMs);}
 samplingWorker.submit(liveRequest,renderpresentation::ObservationRate);
 worker.submit(liveRequest,informationFPS());

 const uint32_t now=GetTickCount();
 // Scene publication, current cursor feedback and worker submission run at
 // their own cadence. A hidden settings-page label needs only 4 updates/sec.
 const unsigned statusKind=(displaying?1:0)|(fresh?2:0)|(feedbackReady?4:0);
 if(!statusCadence.due((generation<<4)|statusKind,now))return;
 std::wstring status;
 if(!displaying&&!fresh)status=L"渲染升级：采样/素材或视角变化，暂用原画面";
 else if(!displaying&&!feedbackReady)status=L"渲染升级：等待有效光标或选择状态，暂用原画面";
 else{
  status=request.paletteRange==1?L"渲染升级：RGB888 全范围色盘":L"渲染升级：原生 DAC 色盘";
  if(renderPhysicalOutput){
   if(physical.status==physicalpresentation::Status::Ready)
    status+=L" · 物理设备 "+std::to_wstring(client.right)+L"×"+std::to_wstring(client.bottom)+
            L"（游戏 "+std::to_wstring(view.screenW)+L"×"+std::to_wstring(view.screenH)+L"）";
   else if(physical.status==physicalpresentation::Status::NativeResolution)
    status+=L" · 已是物理原生分辨率";
   else if(physical.status==physicalpresentation::Status::WindowOnly)
    status+=L" · 非全屏窗口：按客户区 "+std::to_wstring(client.right)+L"×"+
            std::to_wstring(client.bottom)+L" 像素绘制，未启用 4K 增强";
   else if(physical.status==physicalpresentation::Status::SuspiciousModeSwitch)
    status+=L" · 疑似独占/显示模式切换：覆盖层保持 "+
            std::to_wstring(client.right)+L"×"+std::to_wstring(client.bottom)+
            L"，仅停用 4K 增强";
   else if(physical.status==physicalpresentation::Status::AmbiguousDpi)
    status+=L" · DPI 坐标未对应：覆盖层仍按 "+
            std::to_wstring(client.right)+L"×"+std::to_wstring(client.bottom)+L" 绘制";
   else
    status+=L" · 4K 增强条件未满足：覆盖层保持原客户区 "+
            std::to_wstring(client.right)+L"×"+std::to_wstring(client.bottom);
  }
  if(request.pixelScaleMode==3)status+=L" · 自适应 xBRZ／实际输出比例";
  else if(request.pixelScaleMode==0)status+=L" · 原有面积采样";
  else if(request.pixelScaleMode==1)status+=L" · 按实际比例最近邻";
  else if(request.pixelScaleMode==2)status+=L" · xBRZ／实际输出比例";
  status+=L" · 游戏视口 "+std::to_wstring(request.view.width)+L"×"+std::to_wstring(request.view.height)+
          L" → 输出 "+std::to_wstring(request.width)+L"×"+std::to_wstring(request.height);

  status+=request.colorPrecision?L" · 连续 RGB 光照":L" · 兼容光照色阶";
  if(request.detail)status+=L" · 连续地形＋目标像素对象＋VXL 自适应几何/256 向";
  status+=L" · 原始素材绘制（画质增强暂停）";
  status+=L" · 独立整帧绘制目标 "+std::to_wstring(informationFPS())+L" Hz";
  status+=request.profile.motion?L" · 最新观测＋有界移动补帧":L" · 最新内存状态";
  if(request.gpu)status+=presentedFrame->gpuUsed?(presentedFrame->image&&presentedFrame->image->gpu?L" · GPU 场景直出":L" · Vulkan GPU 场景"):L" · Vulkan 未启用或已回退 CPU";
  status+=L" · 共享场景已接入激光/辐射束/波纹/粒子；原生像素一致性待实机验证";
 }
 if(result&&result->hasCompletionGap)status+=L" · 世界新帧间隔 "+std::to_wstring(int(std::lround(result->completionGapMs)))+L" ms";
 const auto uploads=overlaycompositor::host().uploadStatistics();
 if(uploads.count)status+=L" · 覆盖层提交间隔 "+std::to_wstring(uploads.intervalMs)+L" ms（连续最大 "+std::to_wstring(uploads.maxGapMs)+L" ms，非显示 FPS）";
 // The native status control is only 48 design pixels high. Put current
 // measurements first; the verbose configuration summary must not hide them.
 if(displaying&&presentedFrame){
  auto ms=[](double value){wchar_t text[32]{};swprintf(text,32,L"%.1f",value);return std::wstring(text);};
  std::wstring timing=L"独立采样 "+ms(presentedFrame->sampleMs)+L" · 场景 "+ms(presentedFrame->sceneMs)+L" · 后处理 "+ms(presentedFrame->finishMs)+L" ms";
  timing+=L"\r\n新帧 "+(presentedFrame->hasCompletionGap?ms(presentedFrame->completionGapMs):std::wstring(L"待测"))+L" · 采样间隔 "+ms(presentedFrame->observationMs)+L" ms";
  if(presentedFrame->gpuUsed)timing+=L" · GPU准备 "+ms(presentedFrame->gpuPrepareMs)+L"／等待 "+ms(presentedFrame->gpuWaitMs)+L"／回读整理 "+ms(presentedFrame->gpuCopyMs)+L" ms";
  if(presentedFrame->sceneReused)timing+=L" · 画面输入相同，复用完成画面";
  timing+=L"\r\n后处理细分：取景/遮罩 "+ms(presentedFrame->compositionMs[0])+L" · 命令 "+ms(presentedFrame->compositionMs[1])+L" · 合成 "+ms(presentedFrame->compositionMs[2])+L" · 特效 "+ms(presentedFrame->compositionMs[3])+L" ms";
  timing+=L"\r\n对象 "+std::to_wstring(presentedFrame->drawnObjects)+L"/"+std::to_wstring(presentedFrame->requestedObjects)+L" · 读取 "+std::to_wstring(presentedFrame->readCalls)+L" 次 · 批量 "+std::to_wstring(presentedFrame->prefetchedPages)+L" 页 · 素材解码 "+std::to_wstring(presentedFrame->decodeAttempts)+L" 项／"+ms(presentedFrame->decodeMs)+L" ms";
  timing+=L"\r\n场景细分：地形 "+ms(presentedFrame->terrainMs)+L" · 色盘 "+ms(presentedFrame->palettesMs)+L" · 解码 "+ms(presentedFrame->decodeStageMs)+L" · VXL "+ms(presentedFrame->voxelMs)+L" · 对象合成 "+ms(presentedFrame->objectsMs)+L" · 输出 "+ms(presentedFrame->resolveMs)+L" · 特效 "+ms(presentedFrame->effectsMs)+L" ms";
  timing+=L"\r\n场景读取 "+std::to_wstring(presentedFrame->renderReadCalls)+L" 次／缓存 "+std::to_wstring(presentedFrame->renderPageHits)+L" · 色盘采样 "+std::to_wstring(presentedFrame->paletteSamples)+L"／复用 "+std::to_wstring(presentedFrame->paletteHits);
  timing+=L"\r\n每次逻辑帧变化采样 · 逻辑帧变化间隔 "+ms(presentedFrame->stateIntervalMs)+L" ms";
  timing+=L"\r\n采样触发：帧号变化检测";
  timing+=L"\r\n链路：采样→开画 "+ms(presentedFrame->sampleToDrawMs)+L" · 完成→交付 "+ms(completionToHandoffMs)+L" · 当前画面数据年龄 "+ms(std::max(0.,presentationNow()-presentedFrame->sampledAtMs))+L" ms";
  if(presentedFrame->gpuUsed)timing+=presentedFrame->image&&presentedFrame->image->gpu?L" · 回读：0（GPU 直出）":presentedFrame->gpuReadbackCached?L" · 回读：CPU 缓存内存":L" · 回读：非缓存内存";
  if(request.gpu){auto wide=[](const std::string&s){return std::wstring(s.begin(),s.end());};
   timing+=L"\r\nGPU："+(presentedFrame->gpuUsed?std::wstring(presentedFrame->gpuSoftware?L"软件设备 ":L"硬件设备 "):std::wstring(L"CPU 回退 "))+wide(presentedFrame->gpuDevice);
   if(!presentedFrame->gpuReason.empty())timing+=L" · "+wide(presentedFrame->gpuReason);
  }
  status=std::move(timing);
 }
 statusText(status);
}
static void presentCompleted(){
 if(!overlayvisibility::visible()){stop();return;}
 if(!renderUpgradeEnabled||!enabled)return;
 // Completion needs only the current viewport/input feedback. Do not repeat
 // the entire tactical information scan before exposing an already drawn frame.
 externalReader.begin();
 auto state=external::display::inspect([](uint32_t p,void*out,size_t n){return externalReader.readFresh(p,out,n);});
 RECT client{};POINT origin{};if(game){GetClientRect(game,&client);ClientToScreen(game,&origin);}
 update(origin,client,state);
}
}
