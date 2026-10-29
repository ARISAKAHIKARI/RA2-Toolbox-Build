#pragma once
#include "../../shared/source/latest_worker.h"
#include "../../shared/source/recurring_worker.h"
#include "../../shared/source/overlay_compositor.h"
#include "../../shared/source/physical_window.h"
#include "../../external_reader/source/main_viewport.h"
#include "../../external_reader/source/feedback_scene.h"
#include "../../external_reader/source/feedback_orders.h"
#include "../../external_reader/source/feedback_cursor.h"
#include "../../external_reader/source/native_effect_stencil.h"
#include "../../external_reader/source/native_beam_effects.h"
#include "../../external_reader/source/native_unit_ui_stencil.h"
#include "../../external_reader/source/material_motion.h"
#include "../../external_reader/source/scene_bank.h"
#include <chrono>
#include "render_presentation.h"
#include "neural_renderer_client.h"
#include "neural_history.h"
// External read/render only. No Runtime requests or engine draw calls.
namespace renderupgrade {
static double presentationNow(){return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();}
struct Request {uint64_t generation=0;bool materialRefresh=true;int detail=1,colorPrecision=1;uint32_t pid=0,match=0,tick=0;int width=0,height=0;uint32_t reveal=0;int paletteRange=1,pixelScaleMode=0;external::mainview::View view;overlaycompositor::Context context;bool gpu=false,neural=false;uint64_t neuralToken=0;renderpresentation::CameraPrefetch cameraMotion;};
struct Frame {
 double sampleMs=0,sceneMs=0,finishMs=0,gpuWaitMs=0,gpuCopyMs=0;
 Request request;std::shared_ptr<const external::radar::Scene> image;
 std::vector<external::nativefeedback::Object> objects;
 bool gpuAttempted=false,gpuUsed=false;
 neuralclient::Diagnostics neural;
 uint64_t assetRevision=0;uint32_t drawnObjects=0,requestedObjects=0;uint32_t sampledTick=0,completedTick=0;bool hasCompletionGap=false;int paddingX=0,paddingY=0;double renderMs=0,observationMs=0,completionGapMs=0;int logicFrame=-1;double yellow=.5,red=.25;external::nativefeedback::EffectStatus effects;
};
static external::ReadSession reader;
static external::cameraassets::Context<external::ReadSession> assets(reader);
static external::radar::SceneRenderer<decltype(assets)> renderer;
// Reading owns a separate page epoch and never waits for asset decoding, GPU
// completion or a layered-window upload. Only immutable banks cross threads.
struct Observation {
 Request request;external::mainview::View view;
 std::shared_ptr<const external::radar::Frame> current,previous;
 double observedMs=0,previousMs=0,readMs=0;uint32_t tick=0;
};
static external::ReadSession sampleReader;
static external::radar::Sampler<external::SharedReadView> sampler(external::SharedReadView{&sampleReader});
// Full-map paging runs on a third independent thread, never on actor/render threads.
static external::ReadSession terrainReader;
static external::radar::Sampler<external::SharedReadView> terrainSampler(external::SharedReadView{&terrainReader});
static uint32_t process=0,epoch=0;static bool active=false;static uint64_t imageToken=0;
static constexpr UINT ReadyMessage=WM_APP+20;
static std::atomic<bool> readyPending{false};
static void notifyReady(){if(!readyPending.exchange(true)&&!PostMessageW(ui,ReadyMessage,0,0))readyPending=false;}
static std::atomic<int> progress{0};
static uint64_t generation=1,workerGeneration=0;
static external::materialmotion::Timeline timeline;
static int sampledLogicFrame=-1;static bool sampleValid=false;
static renderpresentation::ViewLease viewLease;
static renderpresentation::GroupLease<Request,Frame> groupLease;
static int verifiedScaleSetting=-1;static bool verifiedPhysicalSetting=false;
static renderpresentation::StatusCadence statusCadence;
static renderpresentation::CameraPrefetch cameraPrefetch;
static double lastCompletedRenderMs=0;
static double lastFrameCompleteMs=0;static bool hasFrameCompleteTime=false;
static neuralclient::Client neuralRenderer;
static uint64_t neuralFrame=0;
static neuralhistory::Gate neuralHistory;
#ifdef _WIN64
static constexpr bool neuralHost=true;
#else
static constexpr bool neuralHost=false;
#endif
static std::wstring lastStatusText;
static void statusText(const std::wstring&text){
 if(lastStatusText==text)return;lastStatusText=text;SetWindowTextW(GetDlgItem(ui,9303),text.c_str());
}
static asyncwork::Recurring<Request,external::radar::Frame> terrainWorker([](const Request&r)->std::shared_ptr<const external::radar::Frame>{
 static uint64_t lastGen=0;static uint32_t lastPid=0,lastMatch=0,lastReveal=~0u;
 if(lastGen!=r.generation||lastPid!=r.pid||lastMatch!=r.match||lastReveal!=r.reveal){
  terrainReader.close();terrainSampler.reset();lastGen=r.generation;lastPid=r.pid;lastMatch=r.match;lastReveal=r.reveal;
 }
 if(!terrainReader.open(r.pid))return {};
 terrainReader.begin();
 if(!terrainSampler.step(r.reveal,8192,true)||!terrainSampler.hasTerrain())return {};
 auto result=std::make_shared<external::radar::Frame>(terrainSampler.terrainSnapshot(GetTickCount()));
 auto fresh=[](uint32_t p,void*out,size_t n){return terrainReader.readFresh(p,out,n);};
 if(!external::radar::sameWorld(*result,external::radar::publicationContext(fresh)))return {};
 return result;
});
static asyncwork::Recurring<Request,Observation> samplingWorker([](const Request&r)->std::shared_ptr<const Observation>{
 static std::shared_ptr<const Observation> last;
 static uint64_t sampleGeneration=0;static uint32_t sampleProcess=0,sampleMatch=0,sampleReveal=~0u;
 const double started=presentationNow();
 if(sampleGeneration!=r.generation||sampleProcess!=r.pid||sampleMatch!=r.match||sampleReveal!=r.reveal){
  sampleReader.close();sampler.reset();last.reset();sampleGeneration=r.generation;
  sampleProcess=r.pid;sampleMatch=r.match;sampleReveal=r.reveal;
 }
 if(!sampleReader.open(r.pid))return {};
 sampleReader.begin();
 auto fresh=[](uint32_t p,void*out,size_t n){return sampleReader.readFresh(p,out,n);};
 external::mainview::View before;
 if(!external::mainview::sample(fresh,before)||!renderpresentation::sameProjection(before,r.view)||before.frame<r.view.frame)return {};
 // Coalesce very fast UI requests without retaining an old observation for
 // a whole render. Paused games still resample and continue terrain paging.
 if(last&&last->current->frame==before.frame&&DWORD(GetTickCount()-last->tick)<8)return last;
 // Reuse completed immutable terrain, never scan its 262k cells on the actor clock.
 auto source=terrainWorker.get();
 if(!source||!source->terrain||!external::radar::sameWorld(*source,external::radar::publicationContext(fresh)))
  source=external::radar::sceneBank().get(r.pid,r.match,r.reveal,GetTickCount());
 if(!source||!source->terrain||!external::radar::sameWorld(*source,external::radar::publicationContext(fresh)))return {};
 sampler.seedTerrain(*source);
 sampleReader.begin();external::mainview::View actorView;
 if(!external::mainview::sample(fresh,actorView)||!renderpresentation::sameProjection(before,actorView)||actorView.frame<before.frame)return {};
 auto raw=sampler.snapshot(GetTickCount(),actorView.frame);
 if(!raw.terrain||!raw.markerScanValid||!external::radar::sameWorld(raw,external::radar::publicationContext(fresh)))return {};
 external::SharedReadView cached{&sampleReader};external::GameView game(cached);
 const uint32_t rules=game.get<uint32_t>(0x8871E0);
 double yellow=rules>=0x10000?game.get<double>(rules+0x1700,.5):.5;
 double red=rules>=0x10000?game.get<double>(rules+0x1708,.25):.25;
 if(!std::isfinite(yellow)||yellow<0||yellow>1)yellow=.5;
 if(!std::isfinite(red)||red<0||red>1)red=.25;
 for(auto&m:raw.markers)if(m.kind==3){
  m.pose.frame=external::buildings::frame(cached,m.object,m.type,yellow,red);
  const auto state=external::buildings::state(cached,m.object,m.type,yellow);
  m.pose.buildingFlags=state.flags;m.pose.doorFrame=state.doorFrame;
  if(external::buildings::buildup(game,m.object))m.pose.variant=-2;else if(m.pose.variant==-2)m.pose.variant=-1;
 }
 raw.posesPrepared=true;
 external::mainview::View captured;
 if(!external::mainview::sample(fresh,captured)||!renderpresentation::sameProjection(before,captured)||
    !renderpresentation::sampleAdvance(actorView.frame,raw.frame,captured.frame,uint32_t(std::clamp(presentationNow()-started,0.,10000.)))||
    !external::radar::sameWorld(raw,external::radar::publicationContext(fresh)))return {};
 auto out=std::make_shared<Observation>();out->request=r;out->view=captured;
 out->tick=GetTickCount();out->observedMs=presentationNow();out->readMs=out->observedMs-started;
 out->current=std::make_shared<const external::radar::Frame>(std::move(raw));
 external::radar::sceneBank().publish(r.pid,r.match,true,out->current);
 if(last&&out->current->terrain->geo.same(last->current->terrain->geo)){
  if(out->current->frame>last->current->frame){out->previous=last->current;out->previousMs=last->observedMs;}
  else if(out->current->frame==last->current->frame){out->previous=last->previous;out->previousMs=last->previousMs;}
 }
 last=out;return out;
});
static asyncwork::Recurring<Request,Frame> worker([](const Request&r)->std::shared_ptr<const Frame>{
 const double started=presentationNow();
 static bool neuralPreviouslyRequested=false;
 external::xbrzadapter::gpuPresentation(r.gpu);
 // Failed attempts do not replace a completed image. The UI independently
 // enforces sample/completion age limits and exact world/view/config identity.
 auto unavailable=[]()->std::shared_ptr<const Frame>{return {};};
 if(process!=r.pid||epoch!=r.match){neuralHistory.invalidate();neuralRenderer.reset();reader.close();renderer.reset();process=r.pid;epoch=r.match;sampleValid=false;timeline.reset();lastCompletedRenderMs=0;hasFrameCompleteTime=false;}
 if(workerGeneration!=r.generation){neuralHistory.invalidate();neuralRenderer.reset();workerGeneration=r.generation;sampleValid=false;sampledLogicFrame=-1;timeline.reset();lastCompletedRenderMs=0;hasFrameCompleteTime=false;}
 if(!r.neural&&neuralPreviouslyRequested){neuralHistory.invalidate();neuralRenderer.reset();}
 neuralPreviouslyRequested=r.neural;
 if(!reader.open(r.pid)){progress=-1;return unavailable();}
 reader.begin(); // Explicit fresh page epoch for every presentation request.
 assets.identity(r.pid,r.match);
 auto fresh=[](uint32_t p,void*out,size_t n){return reader.readFresh(p,out,n);};
 external::mainview::View before;
 if(!external::mainview::sample(fresh,before)||!renderpresentation::sameProjection(before,r.view)||before.frame<r.view.frame){progress=-1;return unavailable();}
 // Requests can wait behind a CPU render. Sample the current camera rather
 // than discarding every queued job while the player continuously scrolls.
 Request sampledRequest=r;sampledRequest.view=before;
 sampledRequest.context.cameraX=before.cameraX;sampledRequest.context.cameraY=before.cameraY;
 progress=1;
 const auto observed=samplingWorker.get();
 if(!observed||observed->request.generation!=r.generation||observed->request.pid!=r.pid||
    observed->request.match!=r.match||observed->request.reveal!=r.reveal||
    !renderpresentation::sameProjection(observed->view,before)||
    !external::radar::sameWorld(*observed->current,external::radar::publicationContext(fresh))||
    uint32_t(GetTickCount()-observed->tick)>250)return unavailable();
 static uint32_t timelineReveal=~0u;
 if(!sampleValid||timelineReveal!=r.reveal||observed->current->frame<sampledLogicFrame){
  timeline.reset();sampledLogicFrame=-1;timelineReveal=r.reveal;
 }
 if(observed->current->frame!=sampledLogicFrame){
  // A slow render may miss several readings. Use the sampler's latest TWO
  // observations, so interpolation latency follows reading, not rendering.
  if(observed->previous&&observed->previous->frame>sampledLogicFrame)
   timeline.observe(*observed->previous,observed->previousMs);
  timeline.observe(*observed->current,observed->observedMs);
  sampledLogicFrame=observed->current->frame;sampleValid=true;
 }
 const double sampledMs=presentationNow();
 auto data=timeline.sample(presentationNow(),r.materialRefresh,r.detail!=0);
 if(!data.terrain)return unavailable();
 const uint32_t sampledTick=observed->tick;
 data.tick=GetTickCount(); // presentation retry deadlines keep running while paused
 assets.presentationDetail(r.detail);
 const auto&gpuBackend=external::xbrzadapter::gpuState().resolve;
 const bool gpuUnavailable=gpuBackend&&!gpuBackend->diagnostics().available&&!gpuBackend->diagnostics().reason.empty();
 // GPU and image quality are separate choices: native nearest/1x is a
 // supported GPU scene. Never enable fine VXL, xBRZ or RGB expansion merely
 // because the Vulkan backend was requested.
 const bool useGpu=r.gpu&&!gpuUnavailable;
 assets.presentationGPU(useGpu);
 assets.presentationResolution(r.view.width,r.view.height,r.width,r.height);
 assets.presentationColorPrecision(r.colorPrecision!=0);
 assets.presentationPalette(r.paletteRange==1?external::pixelupgrade::PaletteRange::FullRGB:external::pixelupgrade::PaletteRange::OriginalDAC);
 const uint64_t assetRevision=assets.revision();
 // Scene reuse is intentionally limited to geometry/decoded art. Even an
 // unchanged pose can receive new cell light, converter tint, palette bytes,
 // animation brightness or effects within this same simulation frame.
 // Always run the live-light composition pass; no opaque-frame shortcut.

 // In-between poses still use CURRENT visibility and an immutable current
 // object list. Removed/hidden actors are never resurrected by the timeline.

 const auto border=renderpresentation::window(r.width,r.height,before.width,before.height);
 if(!border.width||!border.height)return unavailable();
 const double paddingX=double(border.paddingX)*before.width/r.width,paddingY=double(border.paddingY)*before.height/r.height;
 // Use the SAME pixel budget, biased toward actually observed camera motion.
 // This changes only the source crop, never the game camera or object state.
 const auto renderView=r.cameraMotion.centerAt(before,GetTickCount(),lastCompletedRenderMs,paddingX,paddingY);
 sampledRequest.view=renderView;sampledRequest.context.cameraX=renderView.cameraX;sampledRequest.context.cameraY=renderView.cameraY;
 auto crop=external::mainview::crop(renderView,data.terrain->geo.offsetX,data.terrain->geo.offsetY);
 renderer.guideEnabled(neuralHost&&r.neural&&r.gpu&&r.detail!=0&&!neuralRenderer.blocked(),r.generation);
 auto scene=renderer.render(data,border.width,border.height,external::SharedReadView{&reader},assets,{crop.x-paddingX,crop.y-paddingY,crop.width+2*paddingX,crop.height+2*paddingY,true},unsigned(r.pixelScaleMode),r.detail!=0||useGpu,true,r.colorPrecision!=0,true);
 if(!scene)return unavailable();
 const double sceneDoneMs=presentationNow();
 auto out=std::make_shared<Frame>();out->request=sampledRequest;out->paddingX=border.paddingX;out->paddingY=border.paddingY;out->assetRevision=assetRevision;out->logicFrame=data.frame;out->observationMs=timeline.interval();
 out->gpuAttempted=external::xbrzadapter::gpuAttempted();out->gpuUsed=external::xbrzadapter::gpuUsed();
 const auto coverage=renderer.objectCoverage();out->drawnObjects=coverage.first;out->requestedObjects=coverage.second;
 out->sampleMs=observed->readMs;out->sceneMs=sceneDoneMs-sampledMs;
 if(out->gpuUsed&&gpuBackend){out->gpuWaitMs=gpuBackend->diagnostics().waitMilliseconds;out->gpuCopyMs=gpuBackend->diagnostics().readbackMilliseconds;}
 // Neural rendering sees only the completed world. Native feedback, command
 // lines and the independently refreshed cursor are composed afterwards.
 if(r.neural){
#ifdef _WIN64
  if(!r.gpu||!r.detail){neuralHistory.invalidate();out->neural.status=neuralframe::Status::Unsupported;out->neural.reason="Enable Vulkan and the detailed scene mode first";}
  else if(neuralRenderer.blocked()){neuralHistory.invalidate();out->neural=neuralRenderer.diagnostics();}
  else if(!out->gpuUsed||!scene->guides){neuralHistory.invalidate();out->neural.status=neuralframe::Status::Unsupported;out->neural.reason="Waiting for a complete GPU world frame and motion guides";}
  else{
   const auto&guides=*scene->guides;std::vector<uint32_t> enhanced;
   if(neuralRenderer.process(scene->pixels,guides.motion,guides.depth,guides.confidence,uint32_t(scene->width),uint32_t(scene->height),r.generation,++neuralFrame,neuralHistory.reset(r.generation,guides.sequence,guides.resetHistory),enhanced,r.neuralToken)){
    neuralHistory.accepted(r.generation,guides.sequence);
    auto result=std::make_shared<external::radar::Scene>();result->width=scene->width;result->height=scene->height;
    result->pixels=std::move(enhanced);scene=std::move(result); // guides were consumed; do not retain them in the displayed frame
   }else neuralHistory.invalidate();
   out->neural=neuralRenderer.diagnostics();
  }
#else
  out->neural.status=neuralframe::Status::Unsupported;out->neural.reason="DLSS 5 requires the 64-bit toolbox";
#endif
 }
 const double zscale=external::projectionZScale(fresh);
 out->objects=external::nativefeedback::objectGeometry(renderer,data,renderView,r.width,r.height,zscale,border.paddingX,border.paddingY);
 out->yellow=assets.conditionYellow();out->red=assets.conditionRed();
 std::vector<external::nativefeedback::Bolt> bolts;const std::vector<uint8_t>*visibility=nullptr;
 if(external::nativefeedback::bolts(fresh,bolts)&&renderer.feedbackVisibility(visibility)){
  out->effects.complete=true;
  if(!bolts.empty()){
   auto decorated=std::make_shared<external::radar::Scene>(*scene);
   out->effects=external::nativefeedback::effects(bolts,*decorated,renderView,visibility,zscale,r.width,r.height,border.paddingX,border.paddingY);
   if(out->effects.complete)scene=std::move(decorated);
  }
 }
 external::mainview::View after;
 if(!external::mainview::sample(fresh,after)||!renderpresentation::sameProjection(before,after)||after.frame<before.frame||
   !external::radar::sameWorld(data,external::radar::publicationContext(fresh)))return unavailable();
 out->image=std::move(scene);out->sampledTick=sampledTick;out->completedTick=GetTickCount();const double completedMs=presentationNow();out->renderMs=completedMs-started;out->finishMs=completedMs-sceneDoneMs;lastCompletedRenderMs=out->renderMs;
 // Written exactly once per successful world-frame completion. Reusing or
 // touching the same frame at a faster UI cadence cannot inflate this rate.
 out->hasCompletionGap=hasFrameCompleteTime;out->completionGapMs=hasFrameCompleteTime?completedMs-lastFrameCompleteMs:0;
 lastFrameCompleteMs=completedMs;hasFrameCompleteTime=true;progress=2;return out;
},notifyReady);
#include "render_feedback_host.h"
static void suspendScene(){neuralRenderer.cancel();cameraPrefetch.reset();groupLease.reset();if(active){active=false;++generation;worker.invalidate();samplingWorker.invalidate();terrainWorker.invalidate();}feedback::hide();imageToken=0;overlaycompositor::host().hide(overlaycompositor::RenderUpgrade);}
static void suspend(){viewLease.reset();suspendScene();}
static void stop(){suspend();overlaycompositor::host().suspend();}
static void shutdown(){stop();worker.shutdown();samplingWorker.shutdown();terrainWorker.shutdown();neuralRenderer.reset();feedback::shutdown();overlaycompositor::host().shutdown();}
static void updateNeuralStatus(bool force=false){
 HWND label=GetDlgItem(ui,9312);if(!label)return;
 static renderpresentation::StatusCadence cadence;
 const uint64_t key=(generation<<4)|uint64_t(renderDLSS5Requested)|(uint64_t(renderUpgradeEnabled)<<1)|(uint64_t(renderVulkanEnabled)<<2)|(uint64_t(renderSceneDetail!=0)<<3);
 if(!force&&!cadence.due(key,GetTickCount()))return;std::wstring text;
 if(!renderDLSS5Requested)text=L"DLSS 5：关闭";
#ifndef _WIN64
 else text=L"DLSS 5：需要 64 位工具箱；当前保留普通渲染。";
#else
 else if(!renderUpgradeEnabled)text=L"DLSS 5：请先开启渲染升级。";
 else if(!renderVulkanEnabled||!renderExtraQuality||!renderSceneDetail)text=L"DLSS 5：需要额外画质处理、Vulkan GPU 和精细场景；当前保留普通渲染。";
 else{
  auto completed=worker.get();
  if(completed&&completed->request.generation==generation&&completed->request.pid==pid&&completed->request.match==displayMatch()&&completed->request.neural){
   const auto&d=completed->neural;
   if(d.used)text=L"DLSS 5：已处理世界画面 · "+std::to_wstring(int(std::lround(d.milliseconds)))+L" ms";
   else{int count=MultiByteToWideChar(CP_UTF8,0,d.reason.data(),int(d.reason.size()),nullptr,0);std::wstring reason(size_t(std::max(0,count)),L' ');if(count)MultiByteToWideChar(CP_UTF8,0,d.reason.data(),int(d.reason.size()),reason.data(),count);if(reason.size()>100){reason.resize(97);reason+=L"…";}text=L"DLSS 5：未生效 · "+reason;}
  }else text=L"DLSS 5：等待完整 GPU 世界画面及后端初始化。";
 }
#endif
 static std::wstring previous;static HWND previousLabel=nullptr;
 if(label!=previousLabel||text!=previous){previousLabel=label;previous=text;SetWindowTextW(label,text.c_str());}
}
static void update(POINT origin,const RECT&client,const external::display::State&displayState){
 updateNeuralStatus();
 // Saved precision/VXL/xBRZ values are only presets until explicitly enabled.
 const bool quality=renderExtraQuality;
 const bool qualityMaterial=quality&&renderMaterialRefresh;
 const int qualityDetail=quality?renderSceneDetail:0;
 const int qualityColor=quality?renderColorPrecision:0;
 const int qualityPalette=quality?renderPaletteRange:0;
 const int qualityScale=quality&&renderXbrzEnabled?renderPixelScale:1;
 const bool qualityGPU=renderVulkanEnabled; // native Vulkan is not an extra-quality effect
 const bool qualityNeural=quality&&renderDLSS5Requested;
 // The whole tool remains system-DPI aware. A physical-pixel option must
 // explicitly validate monitor geometry instead of assuming a 4K monitor
 // makes a 2K client window magically 4K.
 const bool requested=renderUpgradeEnabled||unifiedOverlayEnabled;
 const bool visible=requested&&enabled&&foregroundGame()&&!snap.chat&&pid&&game&&
  (!displayState.readable||displayState.visible());
 const uint32_t viewNow=GetTickCount(),match=displayMatch();
 if(!renderUpgradeEnabled)suspendScene();
 if(!visible){
  overlaycompositor::host().configure(uintptr_t(game),pid,match,client.right,client.bottom,0,0,false,false,informationFPS());
  suspend();progress=0;return;
 }
 external::mainview::View view;
 if(!displayState.readable||!external::mainview::sample([](uint32_t p,void*out,size_t n){return externalReader.readFresh(p,out,n);},view)){
  // Unknown is not known-hidden. A short torn read must not toggle the HWND
  // or invalidate an in-flight job; known menus/focus/world changes do so above.
  if(viewLease.retain(uintptr_t(game),pid,match,client.right,client.bottom,viewNow)){
   const auto&prior=viewLease.view();
   overlaycompositor::host().configure(uintptr_t(game),pid,match,client.right,client.bottom,
    prior.cameraX,prior.cameraY,true,false,informationFPS());
   if(renderUpgradeEnabled){
    auto held=groupLease.frame();
    if(held){
     Request current=held->request;current.generation=generation;current.pid=pid;current.match=match;current.view=prior;
     current.materialRefresh=qualityMaterial;current.detail=qualityDetail;current.colorPrecision=qualityColor;current.pixelScaleMode=qualityScale;
     current.paletteRange=qualityPalette;current.reveal=uint32_t(extra.renderReveal||extra.mapStrategy==3);
     current.gpu=qualityGPU;
     current.neural=qualityNeural;
     current.neuralToken=neuralRenderer.token();
     // Scale-mode changes require a fresh verified request, even during a read gap.
     if(verifiedScaleSetting!=renderPixelScale||verifiedPhysicalSetting!=renderPhysicalOutput)groupLease.reset();
     if(groupLease.retain(current,viewNow)){
      for(auto id:{overlaycompositor::RenderUpgrade,feedback::SelectionLayer,feedback::CursorLayer})
       overlaycompositor::host().touch(id,uintptr_t(game),pid);
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
  visible?view.cameraX:0,visible?view.cameraY:0,visible,false,informationFPS(),renderUpgradeEnabled);
 if(!renderUpgradeEnabled){progress=0;return;}
 active=true;
 const int left=int((int64_t(view.x)*client.right)/view.screenW),top=int((int64_t(view.y)*client.bottom)/view.screenH);
 const int right=int((int64_t(view.x+view.width)*client.right+view.screenW-1)/view.screenW);
 const int bottom=int((int64_t(view.y+view.height)*client.bottom+view.screenH-1)/view.screenH);
 // "Auto" is deliberately adaptive only when the real monitor/target
 // qualifies as physical output. Existing nonphysical configurations retain
 // their original nearest/area behavior.
 const int effectiveScale=qualityScale; // no implicit 4K xBRZ
 verifiedScaleSetting=renderPixelScale;verifiedPhysicalSetting=renderPhysicalOutput;
 Request request{generation,qualityMaterial,qualityDetail,qualityColor,pid,displayMatch(),GetTickCount(),right-left,bottom-top,
  uint32_t(extra.renderReveal||extra.mapStrategy==3),
  qualityPalette,effectiveScale,view,context};
 request.gpu=qualityGPU;
 request.neural=qualityNeural;
 request.neuralToken=neuralRenderer.token();
 cameraPrefetch.observePresentation(view,viewNow);request.cameraMotion=cameraPrefetch;
 auto result=worker.get();
 renderpresentation::Shift shift;
 bool fresh=result&&renderpresentation::scrollVisible(*result,request,GetTickCount(),shift);
 std::shared_ptr<const Frame> feedbackFrame=result;
 auto alignFeedback=[&]{
  feedbackFrame=result;
  if(fresh&&!external::mainview::sameViewport(result->request.view,request.view)){
   auto aligned=std::make_shared<Frame>(*result);aligned->request=request;
   const int dx=shift.x+result->paddingX,dy=shift.y+result->paddingY;
   for(auto&object:aligned->objects){object.bounds.x+=dx;object.bounds.y+=dy;}
   feedbackFrame=std::move(aligned);
  }
 };
 alignFeedback();
 // Gather the required feedback before atomically publishing the opaque scene.
 std::vector<overlaycompositor::Surface> layers;
 bool feedbackReady=fresh&&feedback::update(request,feedbackFrame,left,top,layers);
 if(fresh&&!feedbackReady&&feedback::unsafeView){
  // A scroll tick can land during the feedback read itself. Retry once at
  // its now-current camera; never spin, reuse a different world or repaint
  // pixels on this UI thread. The previous atomic group stays committed.
  external::mainview::View retry;
  if(external::mainview::sample([](uint32_t p,void*out,size_t n){return externalReader.readFresh(p,out,n);},retry)&&
    renderpresentation::sameProjection(request.view,retry)&&retry.frame>=request.view.frame){
   request.view=retry;request.context=overlaycompositor::host().configure(uintptr_t(game),pid,request.match,client.right,client.bottom,
    retry.cameraX,retry.cameraY,true,false,informationFPS(),true);
   context=request.context;viewLease.observe(uintptr_t(game),pid,request.match,client.right,client.bottom,retry,GetTickCount());
   fresh=result&&renderpresentation::scrollVisible(*result,request,GetTickCount(),shift);
   layers.clear();alignFeedback();feedbackReady=fresh&&feedback::update(request,feedbackFrame,left,top,layers);
  }
 }
 std::shared_ptr<const Frame> presentedFrame=result;bool displaying=false;
 if(fresh&&feedbackReady){
  auto pixels=std::shared_ptr<const std::vector<uint32_t>>(result->image,&result->image->pixels);
  layers.push_back({overlaycompositor::RenderUpgrade,5,request.context,
   {left+shift.x,top+shift.y,result->image->width,result->image->height},true,255,std::move(pixels),
   {left,top,request.width,request.height},renderpresentation::sceneCutouts(feedback::nativeCutouts,shift,result->image->width,result->image->height)});imageToken=context.token;
  groupLease.publish(request,feedbackFrame,layers,GetTickCount());displaying=true;
 }else if(!feedback::unsafeView&&groupLease.retain(request,GetTickCount())){
  // Preserve the last COMPLETE atomic scene/input group across a short torn
  // cursor/selection read. Failure never refreshes this lease's deadline.
  layers=groupLease.layers();presentedFrame=groupLease.frame();displaying=true;
 }else{layers.clear();groupLease.reset();imageToken=0;if(!fresh)feedback::hide();}
 overlaycompositor::host().replaceGroupAt(request.context,{overlaycompositor::RenderUpgrade,feedback::SelectionLayer,feedback::CursorLayer},std::move(layers));
 terrainWorker.submit(request,std::clamp(informationFPS(),30,120));
 samplingWorker.submit(request,informationFPS());
 worker.submit(request,informationFPS());
 const uint32_t now=GetTickCount();
 // Scene publication, current cursor feedback and worker submission run at
 // their own cadence. A hidden settings-page label needs only 4 updates/sec.
 const unsigned statusKind=(displaying?1:0)|(fresh?2:0)|(feedbackReady?4:0);
 if(!statusCadence.due((generation<<4)|statusKind,now))return;
 std::wstring status;
 if(!displaying&&!fresh)status=L"渲染升级：采样/素材或视角变化，暂用原画面";
 else if(!displaying&&!feedbackReady)status=L"渲染升级：等待有效光标或选择状态，暂用原画面";
 else{
  status=!renderExtraQuality?L"渲染升级：基础快速绘制（原生色盘/光照/几何）":
          request.paletteRange==1?L"渲染升级：额外画质处理 · RGB888 色盘":L"渲染升级：额外画质处理 · 原生 DAC 色盘";
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

  status+=request.colorPrecision?L" · 连续 RGB 光照":L" · 原生光照色阶";
  if(request.detail)status+=L" · 精细 VXL/目标像素";
  status+=L" · 单位采样/地形分页/整帧绘制独立线程 · 目标 "+std::to_wstring(informationFPS())+L" Hz";
  if(request.materialRefresh)status+=L" · 素材插值";
  if(request.gpu)status+=presentedFrame->gpuUsed?(request.detail?L" · Vulkan GPU 精细场景":L" · Vulkan GPU 基础场景"):L" · Vulkan 未启用或已回退 CPU";
  if(request.neural)status+=presentedFrame->neural.used?L" · DLSS 5 已处理":L" · DLSS 5 未生效（保留普通画面）";
  status+=L" · 光棱/激光与矿车采集保留原生像素；电弧近似，辐射束/波纹/粒子待补，实机待测";
 }
 if(result&&result->hasCompletionGap)status+=L" · 世界新帧间隔 "+std::to_wstring(int(std::lround(result->completionGapMs)))+L" ms";
 const auto uploads=overlaycompositor::host().uploadStatistics();
 if(uploads.count)status+=L" · 覆盖层提交间隔 "+std::to_wstring(uploads.intervalMs)+L" ms（连续最大 "+std::to_wstring(uploads.maxGapMs)+L" ms，非显示 FPS）";
 // The native status control is only 48 design pixels high. Put current
 // measurements first; the verbose configuration summary must not hide them.
 if(displaying&&presentedFrame){
  auto ms=[](double value){wchar_t text[32]{};swprintf(text,32,L"%.1f",value);return std::wstring(text);};
  std::wstring timing=L"独立采样 "+ms(presentedFrame->sampleMs)+L" · 场景 "+ms(presentedFrame->sceneMs)+L" · 后处理 "+ms(presentedFrame->finishMs)+L" ms";
  timing+=L" · 对象 "+std::to_wstring(presentedFrame->drawnObjects)+L"/"+std::to_wstring(presentedFrame->requestedObjects);
  timing+=L"\r\n新帧 "+(presentedFrame->hasCompletionGap?ms(presentedFrame->completionGapMs):std::wstring(L"待测"))+L" · 观测 "+ms(presentedFrame->observationMs)+L" ms";
  if(presentedFrame->gpuUsed)timing+=L" · GPU等待 "+ms(presentedFrame->gpuWaitMs)+L"／回读整理 "+ms(presentedFrame->gpuCopyMs)+L" ms";
  status=timing+L"\r\n"+status;
 }
 statusText(status);
}
static void presentCompleted(){
 if(!renderUpgradeEnabled||!enabled)return;
 // Completion needs only the current viewport/input feedback. Do not repeat
 // the entire tactical information scan before exposing an already drawn frame.
 externalReader.begin();
 auto state=external::display::inspect([](uint32_t p,void*out,size_t n){return externalReader.readFresh(p,out,n);});
 RECT client{};POINT origin{};if(game){GetClientRect(game,&client);ClientToScreen(game,&origin);}
 update(origin,client,state);
}
}
