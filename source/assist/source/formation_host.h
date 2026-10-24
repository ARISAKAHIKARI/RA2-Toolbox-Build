#include "../../external_reader/source/voxel_preview.h"
#include "../../external_reader/source/scene_lighting.h"
#include <map>
#include <tuple>
namespace formationhost {
struct Cached {gameassets::ActorArt art;bool attempted=false,ready=false;uint32_t retry=0;uint64_t used=0,viewUsed[32]{};std::map<int,external::voxel::Image> views;};
struct Visual {int x,y;const external::voxel::Image*image;external::lighting::State light;};
using ModelKey=std::tuple<uint32_t,uint32_t,int>;
static std::map<ModelKey,Cached> models;
static formation::Preview preview;static std::vector<Visual> visuals;
static std::vector<ModelKey> wanted;static uint64_t useSerial=0;static int wantedFacing=0;
static uint32_t epoch=0,process=0,revision=0,sampledAt=0;static uint64_t visualSignature=0;static int missing=0;
static external::lighting::State sceneLight;
static bool read(uint32_t p,void*out,size_t n){return externalReader.readFresh(p,out,n);}
static uint32_t word(uint32_t p){uint32_t v=0;read(p,&v,4);return v;}
static void reset(){models.clear();visuals.clear();preview={};missing=0;++revision;}
static bool needed(const ModelKey&key){return std::find(wanted.begin(),wanted.end(),key)!=wanted.end();}
static bool evictModel(){auto oldest=models.end();for(auto it=models.begin();it!=models.end();++it)if(!needed(it->first)&&(oldest==models.end()||it->second.used<oldest->second.used))oldest=it;if(oldest==models.end())return false;models.erase(oldest);++revision;return true;}
static size_t pixelCount(){size_t n=0;for(const auto&e:models){for(const auto&v:e.second.views)n+=v.second.pixels.size();}return n;}
static bool roomForImage(size_t pixels){while(pixelCount()+pixels>4*1024*1024){Cached*owner=nullptr;int angle=-1;uint64_t oldest=UINT64_MAX;for(auto&e:models)for(const auto&v:e.second.views)if(!(needed(e.first)&&v.first==wantedFacing)&&e.second.viewUsed[v.first]<oldest){owner=&e.second;angle=v.first;oldest=owner->viewUsed[angle];}if(owner){owner->views.erase(angle);++revision;}else if(!evictModel())return false;}return true;}
static ModelKey modelKey(const formation::Unit&u){external::GameView game(&read);int variant=-1;
 if(int(word(u.type+0x808))>0&&!uint8_t(word(u.type+0xCD5))){int n=int(word(u.pointer+0x124));if(n>=0&&n<18)variant=n;}
 return {u.type,word(u.pointer+0x21C),variant};
}
static bool capture(const formation::Unit&u,Cached&cache){
 cache={};cache.used=++useSerial;cache.attempted=true;cache.retry=GetTickCount()+2000;gameassets::update();
 if(!gameassets::captureActor(u.pointer,u.type,u.id,cache.art))return false;
 for(;;){size_t faces=0;for(const auto&e:models)faces+=e.second.art.model.faces.size();if(faces<=400000)break;if(!evictModel()){cache.art={};return false;}}
 cache.ready=true;return true;
}
static void update(){
 static bool wasActive=false;bool activeNow=shared&&shared->formationPreview.active&&extra.formationEnabled&&!snap.chat;if(activeNow!=wasActive){wasActive=activeNow;++paintRevision;}
 if(process!=pid||epoch!=commandMatch()){reset();process=pid;epoch=commandMatch();}
 // Preview positions follow the same paint clock as every other unit overlay.
 sampledAt=GetTickCount();
 formation::Preview next{};
 if(!activeNow||!shared||!foregroundGame()){if(!visuals.empty()){visuals.clear();++paintRevision;}return;}
 long seq=shared->formationSeq;if(seq&1)return;MemoryBarrier();next=shared->formationPreview;MemoryBarrier();if(seq!=shared->formationSeq||!next.active||next.match!=epoch||next.count<0||next.count>formation::Maximum)return;
 auto light=external::lighting::sample(read);if(light!=sceneLight){sceneLight=light;++revision;}
 // Input may stand still indefinitely; validity is anchored by the live shared
 // context, not a timer that would make stationary previews disappear.
 visuals.clear();missing=0;wanted.clear();preview=next;external::Projection projection{snap.cameraX,snap.cameraY,snap.width,snap.height};projection.zScale=external::projectionZScale(read);
 int facing=int(std::lround(next.facing*32/(2*formation::Pi)))&31;wantedFacing=facing;int work=0;
 for(int n=0;n<next.count;++n)if(next.slots[n].valid){int x,y;if(external::project(projection,next.slots[n].x,next.slots[n].y,next.slots[n].z,x,y))wanted.push_back(modelKey(next.units[n]));}
 for(int n=0;n<next.count;++n){const auto&u=next.units[n];const auto&s=next.slots[n];if(!s.valid)continue;int x,y;if(!external::project(projection,s.x,s.y,s.z,x,y))continue;
  if(word(u.pointer+0x10)!=u.id)continue;auto key=modelKey(u);
  // Ownership can change between the wanted scan and this fresh read.
  // Protect this cache entry too before any eviction can invalidate c.
  if(!needed(key))wanted.push_back(key);
  auto found=models.find(key);if(found==models.end()){if(models.size()>=formation::Maximum&&!evictModel()){++missing;continue;}found=models.emplace(key,Cached{}).first;}auto&c=found->second;c.used=++useSerial;
  if(!c.ready&&work<4&&(!c.attempted||int32_t(GetTickCount()-c.retry)>=0)){++work;if(!capture(u,c))c.art={};++revision;}
  if(!c.ready){++missing;continue;}
  auto it=c.views.find(facing);if(it==c.views.end()){if(work>=4){++missing;continue;}++work;external::voxel::Image rendered;external::GameView game(&read);
   external::visual::Pose pose;pose.body=pose.turret=uint8_t(external::visual::previewBody(facing));pose.frame=external::visual::previewFrame(game,u.type,u.kind,facing);
   if(!gameassets::renderActor(c.art,pose,rendered)){++missing;continue;}
   // Both SHP and VXL keep rendered headings; changing the drag direction
   // never reopens archives or redecodes the source model.
   if(c.views.size()>=8){auto old=c.views.begin();for(auto v=c.views.begin();v!=c.views.end();++v)if(c.viewUsed[v->first]<c.viewUsed[old->first])old=v;c.views.erase(old);++revision;}
   if(!roomForImage(rendered.pixels.size())){++missing;continue;}it=c.views.emplace(facing,std::move(rendered)).first;++revision;
  }
  const external::voxel::Image*image=&it->second;c.viewUsed[facing]=++useSerial;
  visuals.push_back({x,y,image,external::lighting::at(read,s.x,s.y,s.z,sceneLight)});
 }
 std::sort(visuals.begin(),visuals.end(),[](const Visual&a,const Visual&b){return a.y<b.y;});
 uint64_t hash=(uint64_t(revision)<<32)^uint64_t(facing+1)*1099511628211ull^uint64_t(next.shape+1)*16777619ull^missing;for(const auto&v:visuals){hash=(hash^uint32_t(v.x))*1099511628211ull;hash=(hash^uint32_t(v.y))*1099511628211ull;hash=(hash^uintptr_t(v.image))*1099511628211ull;hash=(hash^uint32_t(v.light.intensity))*1099511628211ull;for(int c:v.light.tint)hash=(hash^uint32_t(c))*1099511628211ull;}if(hash!=visualSignature){visualSignature=hash;++paintRevision;}
}
static bool visible(){return shared&&shared->formationPreview.active&&extra.formationEnabled&&!snap.chat;}
static void draw(Graphics&g){
 for(const auto&v:visuals){auto gains=v.light.gains();ColorMatrix matrix={};for(int n=0;n<5;++n)matrix.m[n][n]=n<3?gains[n]:1.f;ImageAttributes attributes;attributes.SetColorMatrix(&matrix);auto&i=*v.image;Bitmap bitmap(i.width,i.height,i.width*4,PixelFormat32bppPARGB,reinterpret_cast<BYTE*>(const_cast<uint32_t*>(i.pixels.data())));g.DrawImage(&bitmap,RectF(float(v.x+i.x),float(v.y+i.y),float(i.width),float(i.height)),0.f,0.f,float(i.width),float(i.height),UnitPixel,&attributes);}
 if(visible()){const wchar_t*names[]={L"矩形横队",L"分散横队",L"圆形聚集",L"扇形",L"正方形聚集",L"空心框聚集"};std::wstring text=L"队形 · "+std::wstring(names[std::clamp(preview.shape,0,formation::Shapes-1)])+(extra.formationMouse==1?L" · 左拖排列 / 左击提交预览 · Esc 取消":L" · 右拖排列 / 右击提交预览 · Esc 取消");
  if(missing)text+=L" · "+std::to_wstring(missing)+L" 个模型未载入";drawText(g,text,18,44,12,Color(230,220,244,255));}
}
}
static bool formationHotkey(WORD key){
 if(!shared||!extra.formationEnabled||!shared->formationPreview.active)return false;
 uint32_t normalized=key;if((extra.formationHold&255)==VK_MENU)normalized&=~(4u<<8);if((extra.formationHold&255)==VK_CONTROL)normalized&=~(2u<<8);if((extra.formationHold&255)==VK_SHIFT)normalized&=~(1u<<8);
 static typekeys::Tap tap;bool single=false,twice=false;for(auto k:extra.formationKeys)if(k&&(k&0x7ff)==normalized){single|=!(k&0x8000);twice|=(k&0x8000)!=0;}
 if(single||twice){if(twice){if(!tap.press(normalized,GetTickCount(),commandMatch(),350))return true;normalized|=0x8000;}if(shared->formationKey==shared->formationKeyAck){shared->formationKeyCode=normalized;MemoryBarrier();InterlockedIncrement((volatile LONG*)&shared->formationKey);wakeGame();}return true;}return false;
}
