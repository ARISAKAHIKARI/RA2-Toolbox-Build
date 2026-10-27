#pragma once
#include "../../shared/source/latest_worker.h"
#include "../../shared/source/overlay_compositor.h"
#include "../../external_reader/source/main_viewport.h"
// Included after the existing minimap pipeline. This is external read/render
// work only; no Runtime requests, native reveal or engine draw calls.
namespace renderupgrade {
struct Request {uint32_t pid=0,match=0,tick=0;int width=0,height=0;uint32_t reveal=0;external::mainview::View view;overlaycompositor::Context context;};
struct Frame {Request request;std::shared_ptr<const external::radar::Scene> image;};
static external::ReadSession reader;
static external::cameraassets::Context<external::ReadSession> assets(reader);
static external::radar::SceneRenderer<decltype(assets)> renderer;
static external::radar::Sampler<external::SharedReadView> sampler(external::SharedReadView{&reader});
static uint32_t process=0,epoch=0;static bool active=false;static uint32_t submittedAt=0;static uint64_t imageToken=0;
static std::atomic<int> progress{0}; // 0 off, 1 sampling/assets, 2 complete, -1 stale/unsupported
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
 auto crop=external::mainview::crop(r.view,data.terrain->geo.offsetX,data.terrain->geo.offsetY);
 auto scene=renderer.render(data,r.width,r.height,external::SharedReadView{&reader},assets,{crop.x,crop.y,crop.width,crop.height,true});
 if(!scene)return {};
 external::mainview::View after;
 if(!external::mainview::sample(fresh,after)||!external::mainview::sameViewport(before,after)||after.frame<before.frame||
   !external::radar::sameWorld(data,external::radar::publicationContext(fresh)))return {};
 auto out=std::make_shared<Frame>();out->request=r;out->image=std::move(scene);progress=2;return out;
});
static void suspend(){if(active){active=false;worker.invalidate();}imageToken=0;overlaycompositor::host().hide(overlaycompositor::RenderUpgrade);}
static void stop(){suspend();overlaycompositor::host().suspend();}
static void shutdown(){stop();worker.shutdown();overlaycompositor::host().shutdown();}
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
 Request request{pid,displayMatch(),GetTickCount(),right-left,bottom-top,uint32_t(extra.renderReveal||extra.mapStrategy==3),view,context};
 auto result=worker.get();
 const bool fresh=result&&result->image&&result->request.pid==request.pid&&result->request.match==request.match&&
  result->request.context.token==context.token&&result->request.reveal==request.reveal&&
  result->request.width==request.width&&result->request.height==request.height&&
  external::mainview::sameViewport(result->request.view,view)&&view.frame>=result->request.view.frame&&
  DWORD(GetTickCount()-result->request.tick)<400;
 if(fresh){
  auto pixels=std::shared_ptr<const std::vector<uint32_t>>(result->image,&result->image->pixels);
  overlaycompositor::host().publish({overlaycompositor::RenderUpgrade,renderUpgradeLayer==0?110:5,result->request.context,
   {left,top,request.width,request.height},true,255,std::move(pixels)});imageToken=context.token;
 }else{overlaycompositor::host().hide(overlaycompositor::RenderUpgrade);imageToken=0;}
 // At most one replaceable job, paced independently of game-thread execution.
 // Repeated submissions are coalesced by Latest; no accumulated camera backlog.
 const uint32_t now=GetTickCount();if(DWORD(now-submittedAt)>=uint32_t(1000/std::clamp(informationFPS(),5,120))){submittedAt=now;worker.submit(std::move(request));}
 const wchar_t*status=fresh?L"渲染升级：主相机视口 · 实验性（细节与联机待实测）":L"渲染升级：采样/素材准备或视角变化，暂用游戏原画面";
 SetWindowTextW(GetDlgItem(ui,9303),status);
}
}
