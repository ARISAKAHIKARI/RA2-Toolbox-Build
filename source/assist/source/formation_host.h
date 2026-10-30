#include "../../shared/source/latest_worker.h"
#include "formation_engine.h"
#include "../../external_reader/source/render_profile.h"
namespace formationhost {
using Assets=external::cameraassets::Context<external::ReadSession>;
using Engine=formationengine::Renderer<external::ReadSession,Assets>;
struct Request {uint32_t pid,match,tick;external::Projection projection;formation::Preview preview;external::presentation::Selection selection;};
struct Frame {uint32_t pid,match,tick;external::Projection projection;formation::Preview preview;std::vector<Engine::Visual> visuals;int missing=0;external::presentation::Selection selection;std::shared_ptr<const external::voxel::Image>composite;};
static external::ReadSession reader;static Assets assets(reader);static Engine engine(reader,assets);
static asyncwork::Latest<Request,Frame> worker([](const Request&r)->std::shared_ptr<const Frame>{
 if(engine.process!=r.pid||engine.epoch!=r.match){engine.reset();reader.close();engine.process=r.pid;engine.epoch=r.match;}
 if(!reader.open(r.pid))return {};reader.begin();auto projection=r.projection;projection.zScale=external::projectionZScale(external::SharedReadView{&reader});
 static external::presentation::Profile applied;static bool have=false;if(!have||applied!=r.selection.profile){engine.reset();applied=r.selection.profile;have=true;}external::xbrzadapter::gpuPresentation(r.selection.profile.gpu);external::presentation::apply(r.selection.profile,assets,projection.width,projection.height,projection.width,projection.height);assets.beginPresentation();
 engine.update(r.preview,projection);auto out=std::make_shared<Frame>();out->pid=r.pid;out->match=r.match;out->tick=r.tick;out->projection=projection;out->preview=r.preview;out->visuals=engine.visuals;out->missing=engine.missing;out->selection=r.selection;out->composite=engine.compose(r.selection.profile.precision!=0,unsigned(r.selection.profile.scale));return out;
});
static std::shared_ptr<const Frame> frame;
static bool visible(){return shared&&shared->formationPreview.active&&extra.formationEnabled&&!snap.chat;}
static void update(){
 if(!visible()||!foregroundGame()){if(frame){frame.reset();++paintRevision;}worker.invalidate();return;}
 auto preview=std::make_unique<formation::Preview>();long seq=shared->formationSeq;if(seq&1)return;MemoryBarrier();*preview=shared->formationPreview;MemoryBarrier();if(seq!=shared->formationSeq||!preview->active||preview->match!=commandMatch()||preview->count<0||preview->count>formation::Maximum)return;
 if(frame&&(frame->pid!=pid||frame->match!=commandMatch()||DWORD(GetTickCount()-frame->tick)>=500)){frame.reset();++paintRevision;}
 auto next=worker.get();if(next&&next->pid==pid&&next->match==commandMatch()&&DWORD(GetTickCount()-next->tick)<500&&next!=frame){frame=std::move(next);++paintRevision;}
 auto selection=external::presentation::settings().get();if(frame&&frame->selection.revision!=selection.revision){frame.reset();worker.invalidate();++paintRevision;}
 Request request{pid,commandMatch(),GetTickCount(),{snap.cameraX,snap.cameraY,snap.width,snap.height},*preview,selection};worker.submit(std::move(request));
}
struct Drawing {std::shared_ptr<const Frame> frame;bool active=false;int mouse=0,cameraX=0,cameraY=0;};
static Drawing drawing(){return {frame,visible(),int(extra.formationMouse),snap.cameraX,snap.cameraY};}
static void draw(Graphics&g,const Drawing&d){
 if(d.frame&&d.frame->composite){const auto&i=*d.frame->composite;Bitmap bitmap(i.width,i.height,i.width*4,PixelFormat32bppPARGB,reinterpret_cast<BYTE*>(const_cast<uint32_t*>(i.pixels.data())));g.DrawImage(&bitmap,RectF(float(i.x+d.frame->projection.cameraX-d.cameraX),float(i.y+d.frame->projection.cameraY-d.cameraY),float(i.width),float(i.height)),0.f,0.f,float(i.width),float(i.height),UnitPixel,nullptr);}
 if(d.active){const wchar_t*names[]={L"矩形横队",L"分散横队",L"圆形聚集",L"扇形",L"正方形聚集",L"空心框聚集"};std::wstring text=L"队形 · "+std::wstring(names[d.frame?std::clamp(d.frame->preview.shape,0,formation::Shapes-1):0])+(d.mouse==1?L" · 左拖排列 / 左击提交预览 · Esc 取消":L" · 右拖排列 / 右击提交预览 · Esc 取消");if(d.frame&&d.frame->missing)text+=L" · "+std::to_wstring(d.frame->missing)+L" 个模型未载入";drawText(g,text,18,44,12,Color(230,220,244,255));}
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
