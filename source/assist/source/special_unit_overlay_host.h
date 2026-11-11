#pragma once
#include "special_unit_overlay_engine.h"
namespace specialoverlayhost {
using Assets=external::cameraassets::Context<external::ReadSession>;
using Engine=specialoverlay::Renderer<external::ReadSession,Assets>;
struct Request {uint32_t pid,match;worldframe::Token source;external::Projection projection;std::vector<external::enemies::Marker> targets;external::presentation::Selection selection;nativeviewhost::Transform transform;};
struct Frame {uint32_t pid=0,match=0,tick=0;worldframe::Token source;external::Projection projection;std::vector<specialoverlay::Identity> identities;external::presentation::Selection selection;nativeviewhost::Transform transform;std::shared_ptr<const external::voxel::Image> composite;int missing=0;};
static external::ReadSession reader;static Assets assets(reader);static Engine engine(reader,assets);
static asyncwork::Latest<Request,Frame>worker([](const Request&r)->std::shared_ptr<const Frame>{
 if(engine.paint.process!=r.pid||engine.paint.epoch!=r.match){engine.reset();reader.close();engine.paint.process=r.pid;engine.paint.epoch=r.match;}
 if(!reader.open(r.pid))return {};reader.begin();external::WorldFrameScope scope(reader,r.match,r.source);if(!scope)return {};
 auto current=[&]{uint32_t house=0,scenario=0;int frame=-1;return r.source&&reader.readLive(0xA83D4C,&house,4)&&house==r.source->key.house&&reader.readLive(0xA8B230,&scenario,4)&&scenario==r.source->key.scenario&&reader.readLive(0xA8ED84,&frame,4)&&frame>=r.source->key.frame&&(frame==r.source->key.frame||uint32_t(GetTickCount()-r.source->key.tick)<=150);};
 if(!current())return {};
 static external::presentation::Profile applied;static bool have=false;if(!have||applied!=r.selection.profile){engine.reset();applied=r.selection.profile;have=true;}
 auto projection=r.projection;projection.zScale=external::projectionZScale(external::SharedReadView{&reader});
 external::xbrzadapter::gpuPresentation(r.selection.profile.gpu);external::presentation::apply(r.selection.profile,assets,projection.width,projection.height,projection.width,projection.height);assets.beginPresentation();
 engine.update(r.targets,projection,r.source->key.frame);auto composite=engine.compose(r.selection.profile.precision!=0,unsigned(r.selection.profile.scale));
 if(!scope.complete()||!current())return {};
 auto out=std::make_shared<Frame>();out->pid=r.pid;out->match=r.match;out->tick=GetTickCount();out->source=r.source;out->projection=projection;out->identities=engine.identities;out->selection=r.selection;out->transform=r.transform;out->composite=std::move(composite);out->missing=engine.paint.missing;return out;
});
static std::shared_ptr<const Frame>frame;
static uint32_t lastConfirmed=0,submittedAt=0;static worldframe::Token submittedSource;static int submittedX=0,submittedY=0;static uint64_t submittedSettings=0;
static bool active(){return specialoverlay::enabled(extra.enemyMarkers,extra.enemySpecialOnly,extra.enemySpecialMode);}
static void clear(){if(!submittedSource&&!frame&&!lastConfirmed)return;worker.invalidate();submittedSource.reset();lastConfirmed=0;if(frame){frame.reset();++paintRevision;}}
static void update(const nativeviewhost::Transform&transform){
 if(!active()||!externalWorldSource||!externalData||!foregroundGame()||snap.chat){clear();return;}
 const auto now=GetTickCount();const auto source=externalWorldSource;const auto&markers=externalSampler.enemyMarkers();const auto selection=external::presentation::settings().get();
 if(source->key.pid!=pid||source->key.match!=commandMatch()||markers.match!=source->key.match||markers.house!=source->key.house||markers.frame!=source->key.frame){clear();return;}
 if(frame&&(!frame->source->key.sameWorld(source->key)||frame->transform!=transform||frame->selection.revision!=selection.revision)){clear();}
 if(!externalSourceConfirmed){if(!lastConfirmed||uint32_t(now-lastConfirmed)>=120)clear();return;}
 if(!markers.count){clear();return;}
 lastConfirmed=now;
 if(frame&&!specialoverlay::stillPresent(frame->identities,markers)){frame.reset();++paintRevision;}
 auto next=worker.get();
 if(next&&next!=frame&&next->pid==pid&&next->match==source->key.match&&next->source->key.sameWorld(source->key)&&next->source->key.frame<=source->key.frame&&
    (next->source->key.frame==source->key.frame||uint32_t(now-next->source->key.tick)<150)&&next->transform==transform&&next->selection.revision==selection.revision&&specialoverlay::stillPresent(next->identities,markers)){
  const bool changed=!frame||frame->composite!=next->composite||frame->projection.cameraX!=next->projection.cameraX||frame->projection.cameraY!=next->projection.cameraY;
  frame=std::move(next);if(changed)++paintRevision;
 }
 if(frame&&frame->source->key.frame!=source->key.frame&&uint32_t(now-frame->source->key.tick)>=150){frame.reset();++paintRevision;}
 const bool same=submittedSource==source&&submittedX==snap.cameraX&&submittedY==snap.cameraY&&submittedSettings==selection.revision;
 // Cached materials may finish over several bounded decode rounds. A warm
 // stationary publication needs no repeated worker job or composite upload.
 if(same&&frame&&frame->source==source&&frame->missing==0&&frame->projection.cameraX==snap.cameraX&&frame->projection.cameraY==snap.cameraY)return;
 if(same&&uint32_t(now-submittedAt)<16)return;
 submittedAt=now;submittedSource=source;submittedX=snap.cameraX;submittedY=snap.cameraY;submittedSettings=selection.revision;
 Request request{pid,source->key.match,source,{snap.cameraX,snap.cameraY,transform.active?transform.logical.w:snap.width,transform.active?transform.logical.h:snap.height},
  std::vector<external::enemies::Marker>(markers.items,markers.items+std::clamp(markers.count,0,external::enemies::Maximum)),selection,transform};worker.submit(std::move(request));
}
struct Drawing {std::shared_ptr<const Frame> frame;int cameraX=0,cameraY=0;previewoverlay::Bounds bounds;};
static Drawing drawing(){if(!active()||!frame||!frame->composite)return {};auto bounds=previewoverlay::bounds(*frame->composite,frame->projection,frame->transform,snap.cameraX,snap.cameraY);return {frame,snap.cameraX,snap.cameraY,bounds};}
static void draw(Graphics&g,const Drawing&d){if(d.frame&&d.frame->composite)previewoverlay::draw(g,*d.frame->composite,d.frame->projection,d.frame->transform,d.cameraX,d.cameraY);}
static void shutdown(){worker.shutdown();}
}
