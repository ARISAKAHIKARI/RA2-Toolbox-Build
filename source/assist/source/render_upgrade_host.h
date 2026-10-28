#pragma once
#include "../../shared/source/latest_worker.h"
#include "../../shared/source/overlay_compositor.h"
#include "../../shared/source/physical_window.h"
#include "../../external_reader/source/main_viewport.h"
#include "../../external_reader/source/feedback_scene.h"
#include "../../external_reader/source/feedback_cursor.h"
#include "../../external_reader/source/material_motion.h"
#include <chrono>
// External read/render only. No Runtime requests or engine draw calls.
namespace renderupgrade {
static double presentationNow(){return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();}
struct Request {uint64_t generation=0;bool materialRefresh=true;int detail=1,colorPrecision=1;uint32_t pid=0,match=0,tick=0;int width=0,height=0;uint32_t reveal=0;int paletteRange=1,pixelScaleMode=0;external::mainview::View view;overlaycompositor::Context context;};
struct Frame {
 Request request;std::shared_ptr<const external::radar::Scene> image;
 std::vector<external::nativefeedback::Object> objects;
 uint64_t assetRevision=0;double renderMs=0,observationMs=0;int logicFrame=-1;double yellow=.5,red=.25;external::nativefeedback::EffectStatus effects;
};
static external::ReadSession reader;
static external::cameraassets::Context<external::ReadSession> assets(reader);
static external::radar::SceneRenderer<decltype(assets)> renderer;
static external::radar::Sampler<external::SharedReadView> sampler(external::SharedReadView{&reader});
static uint32_t process=0,epoch=0;static bool active=false;static uint32_t submittedAt=0;static uint64_t imageToken=0;
static std::atomic<int> progress{0};
static uint64_t generation=1,workerGeneration=0;
static external::materialmotion::Timeline timeline;
static int sampledLogicFrame=-1;static bool sampleValid=false;
static asyncwork::Latest<Request,Frame> worker([](const Request&r)->std::shared_ptr<const Frame>{
 const double started=presentationNow();
 // Latest intentionally retains old results for null callbacks. Return an
 // explicit empty-image publication instead, so a failed live-light/palette
 // sample withdraws the old opaque scene immediately on the next UI update.
 auto unavailable=[&]()->std::shared_ptr<const Frame>{auto out=std::make_shared<Frame>();out->request=r;return out;};
 if(process!=r.pid||epoch!=r.match){reader.close();sampler.reset();renderer.reset();process=r.pid;epoch=r.match;sampleValid=false;timeline.reset();}
 if(workerGeneration!=r.generation){workerGeneration=r.generation;sampleValid=false;sampledLogicFrame=-1;timeline.reset();}
 if(!reader.open(r.pid)){progress=-1;return unavailable();}
 reader.begin(); // Explicit fresh page epoch for every presentation request.
 assets.identity(r.pid,r.match);
 auto fresh=[](uint32_t p,void*out,size_t n){return reader.readFresh(p,out,n);};
 external::mainview::View before;
 if(!external::mainview::sample(fresh,before)||!external::mainview::sameViewport(before,r.view)||before.frame<r.view.frame){progress=-1;return unavailable();}
 progress=1;
 // Repeated presentation frames reuse one immutable observed bank. Only a
 // changed logical frame rescans terrain, objects and native animation state.
 if(!sampleValid||before.frame!=sampledLogicFrame){
  reader.begin();
  if(!sampler.step(r.reveal,8192,true))return unavailable();
  auto raw=sampler.snapshot(GetTickCount());auto observed=external::radar::publicationContext(fresh);
  if(!raw.terrain||!raw.markerScanValid||!external::radar::freshMarkers(raw,observed))return unavailable();
  for(auto&m:raw.markers)if(m.kind==3)assets.buildingPose(m.object,m.type,m.pose);
  raw.posesPrepared=true;
  external::mainview::View captured;
  if(!external::mainview::sample(fresh,captured)||captured.frame!=before.frame||!external::mainview::sameViewport(before,captured))return unavailable();
  timeline.observe(raw,presentationNow());sampledLogicFrame=before.frame;sampleValid=true;
 }
 auto data=timeline.sample(presentationNow(),r.materialRefresh,r.detail!=0);
 if(!data.terrain)return unavailable();
 data.tick=GetTickCount(); // presentation retry deadlines keep running while paused
 assets.presentationDetail(r.detail);
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

 auto crop=external::mainview::crop(r.view,data.terrain->geo.offsetX,data.terrain->geo.offsetY);
 auto scene=renderer.render(data,r.width,r.height,external::SharedReadView{&reader},assets,{crop.x,crop.y,crop.width,crop.height,true},unsigned(r.pixelScaleMode),r.detail!=0,true,r.colorPrecision!=0);
 if(!scene)return unavailable();
 auto out=std::make_shared<Frame>();out->request=r;out->assetRevision=assetRevision;out->logicFrame=data.frame;out->observationMs=timeline.interval();
 const double zscale=external::projectionZScale(fresh);
 out->objects=external::nativefeedback::objectGeometry(renderer,data,r.view,r.width,r.height,zscale);
 out->yellow=assets.conditionYellow();out->red=assets.conditionRed();
 std::vector<external::nativefeedback::Bolt> bolts;const std::vector<uint8_t>*visibility=nullptr;
 if(external::nativefeedback::bolts(fresh,bolts)&&renderer.feedbackVisibility(visibility)){
  out->effects.complete=true;
  if(!bolts.empty()){
   auto decorated=std::make_shared<external::radar::Scene>(*scene);
   out->effects=external::nativefeedback::effects(bolts,*decorated,before,visibility,zscale);
   if(out->effects.complete)scene=std::move(decorated);
  }
 }
 external::mainview::View after;
 if(!external::mainview::sample(fresh,after)||!external::mainview::sameViewport(before,after)||after.frame<before.frame||
   !external::radar::sameWorld(data,external::radar::publicationContext(fresh)))return unavailable();
 out->image=std::move(scene);out->renderMs=presentationNow()-started;progress=2;return out;
});
#include "render_feedback_host.h"
static void suspend(){if(active){active=false;++generation;worker.invalidate();}feedback::hide();imageToken=0;overlaycompositor::host().hide(overlaycompositor::RenderUpgrade);}
static void stop(){suspend();overlaycompositor::host().suspend();}
static void shutdown(){stop();worker.shutdown();feedback::shutdown();overlaycompositor::host().shutdown();}
static void update(POINT origin,const RECT&client,bool battlefield){
 // The whole tool remains system-DPI aware. A physical-pixel option must
 // explicitly validate monitor geometry instead of assuming a 4K monitor
 // makes a 2K client window magically 4K.
 const bool requested=renderUpgradeEnabled||unifiedOverlayEnabled;
 bool visible=requested&&enabled&&foregroundGame()&&!snap.chat&&battlefield&&snap.renderStatus==1&&pid&&game;
 external::mainview::View view;
 if(visible&&!external::mainview::sample([](uint32_t p,void*out,size_t n){return externalReader.readFresh(p,out,n);},view))visible=false;
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
  visible?view.cameraX:0,visible?view.cameraY:0,visible,renderUpgradeEnabled&&renderUpgradeLayer==0,informationFPS());
 if(!visible||!renderUpgradeEnabled){suspend();progress=0;return;}
 active=true;
 const int left=int((int64_t(view.x)*client.right)/view.screenW),top=int((int64_t(view.y)*client.bottom)/view.screenH);
 const int right=int((int64_t(view.x+view.width)*client.right+view.screenW-1)/view.screenW);
 const int bottom=int((int64_t(view.y+view.height)*client.bottom+view.screenH-1)/view.screenH);
 // "Auto" is deliberately adaptive only when the real monitor/target
 // qualifies as physical output. Existing nonphysical configurations retain
 // their original nearest/area behavior.
 const int effectiveScale=renderPath==physicalpresentation::RenderPath::EnhancedPhysicalPixels&&
   renderPixelScale==0?3:renderPixelScale;
 Request request{generation,renderMaterialRefresh,renderSceneDetail,renderColorPrecision,pid,displayMatch(),GetTickCount(),right-left,bottom-top,
  uint32_t(extra.renderReveal||extra.mapStrategy==3),
  renderPaletteRange,effectiveScale,view,context};
 auto result=worker.get();
 const bool fresh=result&&result->image&&result->request.pid==request.pid&&result->request.match==request.match&&
  result->request.context.token==context.token&&result->request.reveal==request.reveal&&
  result->request.generation==request.generation&&result->request.materialRefresh==request.materialRefresh&&result->request.detail==request.detail&&result->request.colorPrecision==request.colorPrecision&&
  result->request.paletteRange==request.paletteRange&&result->request.pixelScaleMode==request.pixelScaleMode&&
  result->request.width==request.width&&result->request.height==request.height&&
  external::mainview::sameViewport(result->request.view,view)&&view.frame>=result->request.view.frame&&
  DWORD(GetTickCount()-result->request.tick)<400;
 // Gather the required feedback before atomically publishing the opaque scene.
 std::vector<overlaycompositor::Surface> layers;
 bool feedbackReady=fresh&&feedback::update(request,result,left,top,layers);
 if(fresh&&feedbackReady){
  auto pixels=std::shared_ptr<const std::vector<uint32_t>>(result->image,&result->image->pixels);
  layers.push_back({overlaycompositor::RenderUpgrade,renderUpgradeLayer==0?110:5,result->request.context,
   {left,top,request.width,request.height},true,255,std::move(pixels)});imageToken=context.token;
 }else{layers.clear();imageToken=0;if(!fresh)feedback::hide();}
 overlaycompositor::host().replaceGroup({overlaycompositor::RenderUpgrade,feedback::SelectionLayer,feedback::CursorLayer},std::move(layers));
 const uint32_t now=GetTickCount();if(DWORD(now-submittedAt)>=external::materialmotion::refreshPeriod(informationFPS(),renderMaterialRefresh)){submittedAt=now;worker.submit(std::move(request));}
 std::wstring status;
 if(!fresh)status=L"渲染升级：采样/素材或视角变化，暂用原画面";
 else if(!feedbackReady)status=L"渲染升级：等待有效光标或选择状态，暂用原画面";
 else{
  status=renderPaletteRange==1?L"渲染升级：RGB888 全范围色盘":L"渲染升级：原生 DAC 色盘";
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
  if(request.pixelScaleMode==3)status+=L" · 按实际输出比例重建";
  else if(request.pixelScaleMode==0)status+=L" · 原有面积采样";
  else if(request.pixelScaleMode==1)status+=L" · 按实际比例最近邻";
  else if(request.pixelScaleMode==2)status+=L" · 按实际比例轮廓插值";
  status+=L" · 游戏视口 "+std::to_wstring(request.view.width)+L"×"+std::to_wstring(request.view.height)+
          L" → 输出 "+std::to_wstring(request.width)+L"×"+std::to_wstring(request.height);

  status+=renderColorPrecision?L" · 连续 RGB 光照／匹配色盘恢复":L" · 兼容光照色阶";
  if(renderSceneDetail)status+=L" · VXL 自适应几何采样/256 向＋目标像素合成";
  if(renderMaterialRefresh)status+=L" · 素材高刷目标 "+std::to_wstring(informationFPS())+L" Hz";
  status+=L" · 本帧 "+std::to_wstring(int(std::lround(result->renderMs)))+L" ms";
  status+=L" · 电弧近似；激光/波纹/粒子待补，实机待测";
 }
 SetWindowTextW(GetDlgItem(ui,9303),status.c_str());
}
}
