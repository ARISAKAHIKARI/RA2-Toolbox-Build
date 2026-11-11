#include "middle_input_host.h"
#include "native_viewport_host.h"
#include "formation_quick.h"
#include "../../shared/source/latest_worker.h"
#include "formation_engine.h"
#include "preview_overlay_draw.h"
#include "../../external_reader/source/shared_scene.h"
#include "../../external_reader/source/render_profile.h"
namespace formationhost {
using Assets=external::cameraassets::Context<external::ReadSession>;
using Engine=formationengine::Renderer<external::ReadSession,Assets>;
struct Request {uint32_t pid,match,tick;external::Projection projection;formation::Preview preview;external::presentation::Selection selection;nativeviewhost::Transform transform;};
struct Frame {uint32_t pid,match,tick;worldframe::Token sourceFrame;external::Projection projection;formation::Preview preview;std::vector<Engine::Visual> visuals;int missing=0;external::presentation::Selection selection;std::shared_ptr<const external::voxel::Image>composite;nativeviewhost::Transform transform;};
static external::ReadSession reader;static Assets assets(reader);static Engine engine(reader,assets);
static asyncwork::Latest<Request,Frame> worker([](const Request&r)->std::shared_ptr<const Frame>{
 if(engine.process!=r.pid||engine.epoch!=r.match){engine.reset();reader.close();engine.process=r.pid;engine.epoch=r.match;}
 if(!reader.open(r.pid))return {};reader.begin();
 external::radar::FrameScope scope(reader,r.match,{},worldframe::Base);if(!scope)return {};const auto source=reader.worldFrameToken();
 auto current=[&]{uint32_t house=0,scenario=0;int frame=-1;return source&&reader.readLive(0xA83D4C,&house,4)&&house==source->key.house&&reader.readLive(0xA8B230,&scenario,4)&&scenario==source->key.scenario&&reader.readLive(0xA8ED84,&frame,4)&&frame>=source->key.frame&&(frame==source->key.frame||uint32_t(GetTickCount()-source->key.tick)<=150);};
 if(!current())return {};auto projection=r.projection;projection.zScale=external::projectionZScale(external::SharedReadView{&reader});
 static external::presentation::Profile applied;static bool have=false;if(!have||applied!=r.selection.profile){engine.reset();applied=r.selection.profile;have=true;}external::xbrzadapter::gpuPresentation(r.selection.profile.gpu);external::presentation::apply(r.selection.profile,assets,projection.width,projection.height,projection.width,projection.height);assets.beginPresentation();
 engine.update(r.preview,projection);auto out=std::make_shared<Frame>();out->pid=r.pid;out->match=r.match;out->sourceFrame=source;out->tick=r.tick;out->projection=projection;out->preview=r.preview;out->visuals=engine.visuals;out->missing=engine.missing;out->selection=r.selection;out->transform=r.transform;out->composite=engine.compose(r.selection.profile.precision!=0,unsigned(r.selection.profile.scale));if(!reader.worldFrameComplete()||!current())return {};return out;
});
static std::shared_ptr<const Frame> frame;
static bool visible(){return shared&&shared->formationPreview.active&&extra.formationEnabled&&!snap.chat;}
static void update(){
 if(!visible()||!foregroundGame()){if(frame){frame.reset();++paintRevision;}worker.invalidate();return;}
 nativeviewhost::Transform transform;if(!nativeviewhost::sampleTransform(shared,transform)){frame.reset();worker.invalidate();return;}
 auto preview=std::make_unique<formation::Preview>();long seq=shared->formationSeq;if(seq&1)return;MemoryBarrier();*preview=shared->formationPreview;MemoryBarrier();if(seq!=shared->formationSeq||!preview->active||preview->match!=commandMatch()||preview->count<0||preview->count>formation::Maximum)return;
 if(frame&&(frame->pid!=pid||frame->match!=commandMatch()||frame->transform!=transform||DWORD(GetTickCount()-frame->tick)>=500)){frame.reset();++paintRevision;}
 auto next=worker.get();if(next&&next->pid==pid&&next->match==commandMatch()&&next->transform==transform&&DWORD(GetTickCount()-next->tick)<500&&next!=frame){frame=std::move(next);++paintRevision;}
 auto selection=external::presentation::settings().get();if(frame&&frame->selection.revision!=selection.revision){frame.reset();worker.invalidate();++paintRevision;}
 Request request{pid,commandMatch(),GetTickCount(),{snap.cameraX,snap.cameraY,transform.active?transform.logical.w:snap.width,transform.active?transform.logical.h:snap.height},*preview,selection,transform};worker.submit(std::move(request));
}
struct Drawing {std::shared_ptr<const Frame> frame;bool active=false;int mouse=0,cameraX=0,cameraY=0;std::wstring quickHint;nativeviewhost::Transform transform;};
static Drawing drawing(){nativeviewhost::Transform transform;if(!nativeviewhost::sampleTransform(shared,transform))return {};return {frame&&frame->transform==transform?frame:nullptr,visible(),int(extra.formationMouse),snap.cameraX,snap.cameraY,runtimeGestureDescription(moorders::QuickFormation),transform};}
static void draw(Graphics&g,const Drawing&d){
 if(d.frame&&d.frame->composite)previewoverlay::draw(g,*d.frame->composite,d.frame->projection,d.transform,d.cameraX,d.cameraY);
 if(d.active){const wchar_t*names[]={L"矩形横队",L"分散横队",L"圆形聚集",L"扇形",L"正方形聚集",L"空心框聚集"};bool quick=d.frame&&d.frame->preview.shape==formation::quick::Shape;std::wstring text=L"队形 · "+std::wstring(quick?L"快速占位":names[d.frame?std::clamp(d.frame->preview.shape,0,formation::Shapes-1):0])+(quick?L" · "+d.quickHint+L" 点按聚集 / 拖线排位 · Esc 取消":d.mouse==1?L" · 左拖排列 / 左击提交预览 · Esc 取消":L" · 右拖排列 / 右击提交预览 · Esc 取消");if(d.frame&&d.frame->missing)text+=L" · "+std::to_wstring(d.frame->missing)+L" 个模型未载入";drawText(g,text,18,44,12,Color(230,220,244,255));}
}
static void shutdown(){worker.shutdown();}
}
static bool formationHotkey(WORD key){
 static uint32_t revision=0;if(revision!=settingsRevision){formationTap.clear();revision=settingsRevision;}
 if(!shared||!extra.formationEnabled||!shared->formationPreview.active){formationTap.clear();return false;}
 uint32_t normalized=keygesture::physical(key);if((extra.formationHold&255)==VK_MENU)normalized&=~(4u<<8);if((extra.formationHold&255)==VK_CONTROL)normalized&=~(2u<<8);if((extra.formationHold&255)==VK_SHIFT)normalized&=~(1u<<8);
 bool single=false,twice=false;for(auto k:extra.formationKeys)if(auto binding=formation::shapeKey(k);binding&&keygesture::physical(binding)==normalized){single|=!(binding&keygesture::Double);twice|=(binding&keygesture::Double)!=0;}
 if(single||twice){if(twice){if(!formationTap.press(normalized,GetTickCount(),commandMatch(),350))return true;normalized|=0x8000;}if(shared->formationKey==shared->formationKeyAck){shared->formationKeyCode=normalized;MemoryBarrier();InterlockedIncrement((volatile LONG*)&shared->formationKey);wakeGame();}return true;}formationTap.clear();return false;
}
