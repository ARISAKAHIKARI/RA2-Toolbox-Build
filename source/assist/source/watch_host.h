#include "../../shared/source/latest_worker.h"
#include "watch_scene.h"
#include "watch_world_lifetime.h"
#include <cstdio>
#include <cstring>
namespace watchhost {
static constexpr int Width=external::watch::NativeViewWidth,Height=external::watch::NativeViewHeight;
struct View {external::watch::Frame frame;external::watch::Box box;bool shown=false;int pixelWidth=0,pixelHeight=0;std::shared_ptr<const std::vector<uint32_t>> pixels;std::shared_ptr<const gpupresent::Frame> gpu;};
static View views[3];static external::watch::Track tracks[3];static bool autoSuppressed=false,shown=false,suspended=false;
static uint32_t process=0,epoch=0,controls=0;static uint8_t trackingOptions=0xff;
static watchworld::Lifetime lifetime;static uint32_t lifetimeSample=0;static bool lifetimeReady=false;
static std::array<int,2> panelDemand[2][3]{};
static void demand(int slot,bool second,int width,int height){if(slot>=0&&slot<3)panelDemand[second][slot]={std::clamp(width,0,2048),std::clamp(height,0,2048)};}
struct Request {uint32_t pid,match,tick,control;int width,height;bool reveal,danger,suppressed;std::array<external::watch::Track,3> tracks;external::presentation::Selection selection;std::array<std::array<int,2>,3> outputSizes;};
struct Result {uint32_t pid,match,control,tick;std::array<external::watch::Track,3> tracks;bool suppressed=false;std::array<View,3> views;};
static std::shared_ptr<const Result> applied;
static external::ReadSession renderReader;
static external::cameraassets::Context<external::ReadSession> renderAssets(renderReader);
static watchscene::Renderer<external::ReadSession,decltype(renderAssets)> renderer(renderReader,renderAssets);
static asyncwork::Latest<Request,Result> worker([](const Request&r)->std::shared_ptr<const Result>{
 static watchpresentation::Metric queue,work,sampling,preparation,draws[3];static uint64_t reused=0,missing=0,unreadable=0,invalid=0;static std::array<uint64_t,decltype(renderer)::Timings::FailureCount> reasons{};static uint32_t firstMissing=0,lastMissing=0,reported=GetTickCount();
 const double workAt=watchpresentation::now();queue.add(watchpresentation::queueDelay(GetTickCount(),r.tick));
 if(renderer.process!=r.pid||renderer.epoch!=r.match){renderer.reset();renderReader.close();renderer.process=r.pid;renderer.epoch=r.match;}
 if(!renderReader.open(r.pid))return {};renderReader.begin();
 if(!external::display::inspect(external::SharedReadView{&renderReader}).visible())return {};
 renderer.width=r.width;renderer.height=r.height;renderer.revealMap=r.reveal;renderer.danger=r.danger;
 renderer.outputSizes=r.outputSizes;
 renderer.features(r.selection.profile);renderer.syncTracks(r.control,r.tracks,r.suppressed);const bool updated=renderer.update(GetTickCount());work.add(watchpresentation::now()-workAt,updated);
 const auto&t=renderer.timings;sampling.add(t.sample,updated);if(t.prepared)preparation.add(t.prepare,t.prepareComplete);for(int i=0;i<3;++i)if(t.attempted[i]){draws[i].add(t.draw[i],t.drawComplete[i]);reused+=t.reused[i];}
 ++reasons[t.failure];missing+=renderReader.worldMissingReads;unreadable+=renderReader.worldUnreadableReads;invalid+=renderReader.worldInvalidReads;
 if(renderReader.worldFirstMissingAddress){if(!firstMissing)firstMissing=renderReader.worldFirstMissingAddress;lastMissing=renderReader.worldLastMissingAddress;}
 unsigned directViews=0;for(const auto&v:renderer.views)directViews+=bool(v.scene)&&bool(v.scene->gpu);
 const uint32_t finished=GetTickCount();if(uint32_t(finished-reported)>=5000){char line[1200];std::snprintf(line,sizeof line,
  "watch render jobs=%llu fail=%llu direct=%u shared=%d reused=%llu source-age=%u ms queue=%.2f/max%.2f sample=%.2f prepare=%.2f/fail%llu draw=%.2f/%.2f/%.2f total=%.2f/max%.2f reasons[frame/scene/stale/terrain/anchor/prepare/draw/incomplete]=%llu/%llu/%llu/%llu/%llu/%llu/%llu/%llu missing-reads=%llu unreadable-reads=%llu invalid-reads=%llu first=%08X last=%08X",
  (unsigned long long)work.count,(unsigned long long)work.failed,directViews,renderer.sharedSource,(unsigned long long)reused,watchpresentation::queueDelay(finished,renderer.sourceTick),queue.average(),queue.maximum,sampling.average(),preparation.average(),(unsigned long long)preparation.failed,draws[0].average(),draws[1].average(),draws[2].average(),work.average(),work.maximum,
  (unsigned long long)reasons[1],(unsigned long long)reasons[2],(unsigned long long)reasons[3],(unsigned long long)reasons[4],(unsigned long long)reasons[5],(unsigned long long)reasons[6],(unsigned long long)reasons[7],(unsigned long long)reasons[8],(unsigned long long)missing,(unsigned long long)unreadable,(unsigned long long)invalid,firstMissing,lastMissing);
  // Log on the worker even during a continuous run of rejected frames.
  critical(std::wstring(line,line+std::strlen(line)));reported=finished;queue=work=sampling=preparation={};for(auto&d:draws)d={};reused=missing=unreadable=invalid=0;reasons={};firstMissing=lastMissing=0;
 }
 if(!updated)return {};
 auto result=std::make_shared<Result>();result->pid=r.pid;result->match=r.match;result->control=r.control;result->tick=r.tick;result->suppressed=renderer.autoSuppressed;std::copy(std::begin(renderer.tracks),std::end(renderer.tracks),result->tracks.begin());
 for(int i=0;i<3;++i){auto&v=renderer.views[i];auto&out=result->views[i];out.shown=v.shown;out.box=v.box;out.frame.focus=v.frame.focus;out.frame.bounds=v.frame.bounds;out.frame.center=v.frame.center;out.frame.cameraX=v.frame.cameraX;out.frame.cameraY=v.frame.cameraY;out.frame.dangerous=v.frame.dangerous;
  if(v.shown&&v.scene){
   out.gpu=v.scene->gpu;
   out.pixelWidth=v.scene->width;out.pixelHeight=v.scene->height;
   if(!out.gpu&&v.scene->pixels.size()==size_t(out.pixelWidth)*out.pixelHeight)out.pixels=std::shared_ptr<const std::vector<uint32_t>>(v.scene,&v.scene->pixels);
  }
  out.shown=out.shown&&(bool(out.gpu)||bool(out.pixels));
 }
 // Normal sampling shares a page cache. A trigger can replace VisibleRect
 // while assets decode, so validate the completed batch against uncached
 // bounds before publishing. Dropping the batch preserves the UI's slots.
 auto bounds=external::mapbounds::sample([](uint32_t p,void*out,size_t n){return renderReader.readLive(p,out,n);});
 for(const auto&v:result->views)if(v.shown&&(!bounds.valid()||v.frame.bounds!=bounds))return {};
 return result;
});
static void arrange(){bool visible[3]{};external::watch::Box boxes[3]{};shown=false;for(int i=0;i<3;++i)shown|=visible[i]=views[i].shown;external::watch::arrange(visible,snap.width,snap.height,boxes);for(int i=0;i<3;++i)views[i].box=boxes[i];}
// Foreground navigation can observe death/succession after a worker has
// sampled its scene but before that job completes. Commit such identity
// changes as controls, so neither an in-flight result nor a queued old
// request can overwrite the confirmed decision. Spatial movement alone
// does not invalidate the asynchronous renderer or its asset caches.
template<class Read>static bool followTracks(Read read,int slot=-1){
 bool changed=false;
 for(int i=0;i<3;++i){if(slot>=0&&i!=slot)continue;auto&track=tracks[i];auto before=track.target;external::watch::follow(read,track);
  if(!(before==track.target)){changed=true;views[i]={};if(!track)autoSuppressed=true;}
 }
 if(changed){worker.invalidate();++controls;applied.reset();arrange();++paintRevision;}
 return changed;
}
static void reset(){suspended=false;worker.invalidate();applied.reset();++controls;for(auto&t:tracks)t={};for(auto&v:views)v={};autoSuppressed=false;shown=false;lifetime.reset();lifetimeSample=0;lifetimeReady=false;}
static void suspend(){if(suspended)return;suspended=true;worker.invalidate();applied.reset();++controls;for(auto&v:views)v.shown=false;shown=false;}
static bool synchronizeWorld(){
 const bool valid=externalData&&externalData->status==1&&externalData->sampledTick&&uint32_t(GetTickCount()-externalData->sampledTick)<2000;
 const uint32_t sampled=externalData?externalData->sampledTick:0,match=valid?externalData->match:0;
 if(process==pid&&sampled==lifetimeSample&&(!match||match==epoch))return lifetimeReady&&valid;
 auto next=lifetime;auto decision=next.observe(pid,match,assist::worldlife::capture([](uint32_t p,void*out,size_t n){return externalReader.readFresh(p,out,n);}));
 if(decision.clear)reset();
 else if(decision.rebind){worker.invalidate();++controls;applied.reset();}
 lifetime=next;lifetimeSample=sampled;lifetimeReady=decision.ready;process=pid;
 if(decision.ready)epoch=match;
 return lifetimeReady&&valid;
}
static void close(int slot){if(slot<0||slot>=3)return;worker.invalidate();++controls;tracks[slot]={};views[slot]={};autoSuppressed=true;arrange();++paintRevision;}
static void pin(uint32_t key){
 if((!enabled||!extra.watchEnabled)&&!extra.watchSecondary)return;
 if(!foregroundGame()||snap.chat||formationInputOwns())return;
 if(!synchronizeWorld())return;
 auto read=[](uint32_t p,void*out,size_t n){return externalReader.readFresh(p,out,n);};followTracks(read);
 int slot=-1;auto result=external::watch::add(tracks,extra.watchKeys,key,external::watch::capture(read),slot);using R=external::watch::AddResult;
 if(result==R::NoTarget){notify(L"请先选中单位，或将鼠标放到单位上。");return;}
 if(result==R::Full){notify(L"此快捷键绑定的跟踪窗口已满；Shift＋右击窗口可关闭。");return;}
 if(result==R::AlreadyTracked){notify(L"此单位已在该快捷键的窗口中跟踪。");return;}
 worker.invalidate();++controls;autoSuppressed=true;++paintRevision;notify(L"已添加跟踪窗口 "+std::to_wstring(slot+1)+L"；左击跳转，Shift＋右击关闭。");
}
static void update(uint32_t now){
 auto selection=external::presentation::settings().get();static uint64_t selectedRevision=~uint64_t(0);
 if(selectedRevision!=selection.revision){selectedRevision=selection.revision;worker.invalidate();++controls;applied.reset();for(auto&v:views)v={};}

 if(!synchronizeWorld()){suspend();return;}
 bool primary=enabled&&extra.watchEnabled&&foregroundGame()&&!snap.chat;
 bool secondary=extra.watchSecondary&&secondaryDisplayAvailable();
 uint8_t options=uint8_t(primary||secondary)|(uint8_t(bool(extra.watchDanger))<<1);
 if(options!=trackingOptions){trackingOptions=options;worker.invalidate();++controls;}
 if((!primary&&!secondary)||!extra.watchDanger)autoSuppressed=false;
 if((!primary&&!secondary)||snap.renderStatus!=1||snap.width<360||snap.height<320){suspend();return;}
 suspended=false;
 // Asset decoding can take longer than an overlay interval. Keep the current
 // panels until an accepted result changes their lifetime, and consume each
 // immutable result once rather than replaying its old tracking decisions.
 auto result=worker.get();if(result&&result!=applied&&result->pid==pid&&result->match==epoch&&result->control==controls){applied=result;std::copy(result->views.begin(),result->views.end(),views);std::copy(result->tracks.begin(),result->tracks.end(),tracks);autoSuppressed=result->suppressed;}
 arrange();
 Request r{pid,epoch,now,controls,snap.width,snap.height,bool(extra.renderReveal||assist::shroudpolicy::fullPreview(extra.mapStrategy)),bool(extra.watchDanger),autoSuppressed,{}};std::copy(std::begin(tracks),std::end(tracks),r.tracks.begin());r.selection=selection;
 for(int i=0;i<3;++i){int w=std::max(primary?panelDemand[0][i][0]:0,secondary?panelDemand[1][i][0]:0),h=std::max(primary?panelDemand[0][i][1]:0,secondary?panelDemand[1][i][1]:0);r.outputSizes[i]={w>0?w:320,h>0?h:256};}worker.submit(std::move(r));
}
static void drawView(Graphics&g,const View&v){if(!v.shown||!v.pixels||v.pixelWidth<1||v.pixelHeight<1)return;Bitmap bm(v.pixelWidth,v.pixelHeight,v.pixelWidth*4,PixelFormat32bppPARGB,reinterpret_cast<BYTE*>(const_cast<uint32_t*>(v.pixels->data())));g.SetInterpolationMode(InterpolationModeNearestNeighbor);g.SetPixelOffsetMode(PixelOffsetModeHalf);g.DrawImage(&bm,RectF(0,0,float(Width),float(Height)),0.f,0.f,float(v.pixelWidth),float(v.pixelHeight),UnitPixel,nullptr);Pen border(Color(255,v.frame.dangerous?255:105,v.frame.dangerous?167:195,v.frame.dangerous?62:225),6.f);g.DrawRectangle(&border,3.f,3.f,float(Width-6),float(Height-6));}
static void shutdown(){worker.shutdown();}
}

