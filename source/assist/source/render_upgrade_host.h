#pragma once
#include "../../shared/source/latest_worker.h"
#include "../../shared/source/overlay_compositor.h"
#include "../../external_reader/source/main_viewport.h"
#include "../../external_reader/source/feedback_scene.h"
#include "../../external_reader/source/feedback_cursor.h"
// External read/render only. No Runtime requests or engine draw calls.
namespace renderupgrade {
struct Request {uint32_t pid=0,match=0,tick=0;int width=0,height=0;uint32_t reveal=0;int paletteRange=1,pixelScaleMode=0;external::mainview::View view;overlaycompositor::Context context;};
struct Frame {
 Request request;std::shared_ptr<const external::radar::Scene> image;
 std::vector<external::nativefeedback::Object> objects;
 double yellow=.5,red=.25;external::nativefeedback::EffectStatus effects;
};
static external::ReadSession reader;
static external::cameraassets::Context<external::ReadSession> assets(reader);
static external::radar::SceneRenderer<decltype(assets)> renderer;
static external::radar::Sampler<external::SharedReadView> sampler(external::SharedReadView{&reader});
static uint32_t process=0,epoch=0;static bool active=false;static uint32_t submittedAt=0;static uint64_t imageToken=0;
static std::atomic<int> progress{0};
static asyncwork::Latest<Request,Frame> worker([](const Request&r)->std::shared_ptr<const Frame>{
 if(process!=r.pid||epoch!=r.match){reader.close();sampler.reset();renderer.reset();process=r.pid;epoch=r.match;}
 if(!reader.open(r.pid)){progress=-1;return {};}
 auto fresh=[](uint32_t p,void*out,size_t n){return reader.readFresh(p,out,n);};
 external::mainview::View before;
 if(!external::mainview::sample(fresh,before)||!external::mainview::sameViewport(before,r.view)||before.frame<r.view.frame){progress=-1;return {};}
 reader.begin();progress=1;
 if(!sampler.step(r.reveal,8192,true))return {};
 auto data=sampler.snapshot(GetTickCount());auto observed=external::radar::publicationContext(fresh);
 if(!data.terrain||!data.markerScanValid||!external::radar::freshMarkers(data,observed))return {};
 assets.identity(r.pid,r.match);
 assets.presentationPalette(r.paletteRange==1?external::pixelupgrade::PaletteRange::FullRGB:external::pixelupgrade::PaletteRange::OriginalDAC);
 auto crop=external::mainview::crop(r.view,data.terrain->geo.offsetX,data.terrain->geo.offsetY);
 auto scene=renderer.render(data,r.width,r.height,external::SharedReadView{&reader},assets,{crop.x,crop.y,crop.width,crop.height,true},unsigned(r.pixelScaleMode));
 if(!scene)return {};
 auto out=std::make_shared<Frame>();out->request=r;
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
   !external::radar::sameWorld(data,external::radar::publicationContext(fresh)))return {};
 out->image=std::move(scene);progress=2;return out;
});
#include "render_feedback_host.h"
static void suspend(){if(active){active=false;worker.invalidate();}feedback::hide();imageToken=0;overlaycompositor::host().hide(overlaycompositor::RenderUpgrade);}
static void stop(){suspend();overlaycompositor::host().suspend();}
static void shutdown(){stop();worker.shutdown();feedback::shutdown();overlaycompositor::host().shutdown();}
static void update(POINT origin,const RECT&client,bool battlefield){
 (void)origin;
 const bool requested=renderUpgradeEnabled||unifiedOverlayEnabled;
 bool visible=requested&&enabled&&foregroundGame()&&!snap.chat&&battlefield&&snap.renderStatus==1&&pid&&game;
 external::mainview::View view;
 if(visible&&!external::mainview::sample([](uint32_t p,void*out,size_t n){return externalReader.readFresh(p,out,n);},view))visible=false;
 auto context=overlaycompositor::host().configure(uintptr_t(game),pid,displayMatch(),client.right,client.bottom,
  visible?view.cameraX:0,visible?view.cameraY:0,visible,renderUpgradeEnabled&&renderUpgradeLayer==0,informationFPS());
 if(!visible||!renderUpgradeEnabled){suspend();progress=0;return;}
 active=true;
 const int left=int((int64_t(view.x)*client.right)/view.screenW),top=int((int64_t(view.y)*client.bottom)/view.screenH);
 const int right=int((int64_t(view.x+view.width)*client.right+view.screenW-1)/view.screenW);
 const int bottom=int((int64_t(view.y+view.height)*client.bottom+view.screenH-1)/view.screenH);
 Request request{pid,displayMatch(),GetTickCount(),right-left,bottom-top,uint32_t(extra.renderReveal||extra.mapStrategy==3),renderPaletteRange,renderPixelScale,view,context};
 auto result=worker.get();
 const bool fresh=result&&result->image&&result->request.pid==request.pid&&result->request.match==request.match&&
  result->request.context.token==context.token&&result->request.reveal==request.reveal&&
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
 const uint32_t now=GetTickCount();if(DWORD(now-submittedAt)>=uint32_t(1000/std::clamp(informationFPS(),5,120))){submittedAt=now;worker.submit(std::move(request));}
 const wchar_t*status=!fresh?L"渲染升级：采样/素材或视角变化，暂用原画面":!feedbackReady?L"渲染升级：等待有效光标或选择状态，暂用原画面":
  (renderPixelScale && request.width==2*request.view.width && request.height==2*request.view.height ?
   L"渲染升级：24-bit 色盘 / 2x 轮廓插值 · 电弧近似；其他特效待补，实机待测" :
   L"渲染升级：24-bit 色盘 · 2x 插值仅在真实双倍输出时启用；电弧近似，其他特效待补");
 SetWindowTextW(GetDlgItem(ui,9303),status);
}
}
