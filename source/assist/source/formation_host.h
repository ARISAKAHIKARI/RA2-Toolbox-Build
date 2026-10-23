#include "../../external_reader/source/voxel_preview.h"
#include <map>
namespace formationhost {
struct Cached {external::voxel::Model model;std::array<uint32_t,256> palette{};external::voxel::Image sprite;bool attempted=false,ready=false,isVoxel=false,fileBacked=false;int fileFacing=-1;uint32_t retry=0;uint64_t used=0,viewUsed[32]{};std::map<int,external::voxel::Image> views;};
struct Visual {int x,y;const external::voxel::Image*image;};
static std::map<std::pair<uint32_t,uint32_t>,Cached> models;
static formation::Preview preview;static std::vector<Visual> visuals;
static std::vector<std::pair<uint32_t,uint32_t>> wanted;static uint64_t useSerial=0;static int wantedFacing=0;
static uint32_t epoch=0,process=0,revision=0,sampledAt=0;static uint64_t visualSignature=0;static int missing=0;
static bool read(uint32_t p,void*out,size_t n){return externalReader.readFresh(p,out,n);}
static uint32_t word(uint32_t p){uint32_t v=0;read(p,&v,4);return v;}
static void reset(){models.clear();visuals.clear();preview={};missing=0;++revision;}
static bool needed(const std::pair<uint32_t,uint32_t>&key){return std::find(wanted.begin(),wanted.end(),key)!=wanted.end();}
static bool evictModel(){auto oldest=models.end();for(auto it=models.begin();it!=models.end();++it)if(!needed(it->first)&&(oldest==models.end()||it->second.used<oldest->second.used))oldest=it;if(oldest==models.end())return false;models.erase(oldest);++revision;return true;}
static size_t pixelCount(){size_t n=0;for(const auto&e:models){n+=e.second.sprite.pixels.size();for(const auto&v:e.second.views)n+=v.second.pixels.size();}return n;}
static bool roomForImage(size_t pixels){while(pixelCount()+pixels>4*1024*1024){Cached*owner=nullptr;int angle=-1;uint64_t oldest=UINT64_MAX;for(auto&e:models)for(const auto&v:e.second.views)if(!(needed(e.first)&&v.first==wantedFacing)&&e.second.viewUsed[v.first]<oldest){owner=&e.second;angle=v.first;oldest=owner->viewUsed[angle];}if(owner){owner->views.erase(angle);++revision;}else if(!evictModel())return false;}return true;}
static bool captureMemory(const formation::Unit&u,Cached&cache){
 uint32_t owner=word(u.pointer+0x21C);cache={};cache.used=++useSerial;cache.attempted=true;cache.retry=GetTickCount()+2000;
 if(word(u.pointer+0x10)!=u.id||!external::voxel::palette(read,u.type,owner,cache.palette))return false;
 uint32_t parts[6]{};if(!read(u.type+0xB0,parts,sizeof parts))return false;
 if(parts[0]){cache.isVoxel=true;double sx=1,sy=1;read(u.type+0x360,&sx,8);read(u.type+0x368,&sy,8);if(!std::isfinite(sx)||sx<=0||sx>8)sx=1;if(!std::isfinite(sy)||sy<=0||sy>8)sy=1;
  if(!external::voxel::capturePart(read,parts[0],parts[1],cache.model,0,float(sx),float(sy)))return false;
  int offset=int32_t(word(u.type+0x720));if(offset<-4096||offset>4096)return false;
  // TurretOffset is in leptons; the voxel coordinate unit is 1/12 cell.
  for(int n=2;n<6;n+=2)if(parts[n]&&!external::voxel::capturePart(read,parts[n],parts[n+1],cache.model,float(offset)*12/256,float(sx),float(sy)))return false;
  uint32_t after[6]{};if(!read(u.type+0xB0,after,sizeof after)||memcmp(parts,after,sizeof parts))return false;
 }else if(!external::voxel::sprite(read,word(u.type+0xA4),cache.palette,cache.sprite))return false;
 for(;;){size_t faces=0;for(const auto&e:models)faces+=e.second.model.faces.size();if(faces<=400000)break;if(!evictModel()){cache.model.faces.clear();return false;}}
 if(!roomForImage(0))return false;cache.ready=true;return true;
}
static bool capture(const formation::Unit&u,Cached&cache){
 if(captureMemory(u,cache))return true;
 gameassets::update();cache.model={};cache.views.clear();cache.isVoxel=false;
 if(!gameassets::actor(u.pointer,u.type,u.id,wantedFacing*(2*formation::Pi/32),cache.sprite))return false;
 if(!roomForImage(0)){cache.sprite={};return false;}
 // File-backed fallback is keyed by the preview heading as well.
 cache.ready=true;cache.attempted=true;cache.fileBacked=true;cache.fileFacing=wantedFacing;return true;
}
static void update(){
 static bool wasActive=false;bool activeNow=shared&&shared->formationPreview.active&&extra.formationEnabled&&!snap.chat;if(activeNow!=wasActive){wasActive=activeNow;++paintRevision;}
 if(process!=pid||epoch!=commandMatch()){reset();process=pid;epoch=commandMatch();}
 if(activeNow&&sampledAt&&DWORD(GetTickCount()-sampledAt)<33)return;sampledAt=GetTickCount();
 formation::Preview next{};
 if(!activeNow||!shared||!foregroundGame()){if(!visuals.empty()){visuals.clear();++paintRevision;}return;}
 long seq=shared->formationSeq;if(seq&1)return;MemoryBarrier();next=shared->formationPreview;MemoryBarrier();if(seq!=shared->formationSeq||!next.active||next.match!=epoch||next.count<0||next.count>formation::Maximum)return;
 // Input may stand still indefinitely; validity is anchored by the live shared
 // context, not a timer that would make stationary previews disappear.
 visuals.clear();missing=0;wanted.clear();preview=next;external::Projection projection{snap.cameraX,snap.cameraY,snap.width,snap.height};
 int facing=int(std::lround(next.facing*32/(2*formation::Pi)))&31;wantedFacing=facing;int work=0;
 for(int n=0;n<next.count;++n)if(next.slots[n].valid){int x,y;if(external::project(projection,next.slots[n].x,next.slots[n].y,next.slots[n].z,x,y))wanted.emplace_back(next.units[n].type,word(next.units[n].pointer+0x21C));}
 for(int n=0;n<next.count;++n){const auto&u=next.units[n];const auto&s=next.slots[n];if(!s.valid)continue;int x,y;if(!external::project(projection,s.x,s.y,s.z,x,y))continue;
  if(word(u.pointer+0x10)!=u.id)continue;uint32_t owner=word(u.pointer+0x21C);auto key=std::make_pair(u.type,owner);
  // Ownership can change between the wanted scan and this fresh read.
  // Protect this cache entry too before any eviction can invalidate c.
  if(!needed(key))wanted.push_back(key);
  auto found=models.find(key);if(found==models.end()){if(models.size()>=64&&!evictModel()){++missing;continue;}found=models.emplace(key,Cached{}).first;}auto&c=found->second;c.used=++useSerial;
  if(c.fileBacked&&c.fileFacing!=wantedFacing){c.ready=false;c.attempted=false;}
  if(!c.ready&&work<1&&(!c.attempted||int32_t(GetTickCount()-c.retry)>=0)){++work;if(!capture(u,c)){c.model.faces.clear();c.sprite={};}++revision;}
  if(!c.ready){++missing;continue;}const external::voxel::Image*image=&c.sprite;
  if(c.isVoxel){auto it=c.views.find(facing);if(it==c.views.end()){if(work>=1){++missing;continue;}++work;external::voxel::Image rendered;
    if(!external::voxel::render(c.model,facing*(2*formation::Pi/32),c.palette,rendered)){++missing;continue;}
    // Bound angle-image cache independently from source geometry.
    if(c.views.size()>=8){auto old=c.views.begin();for(auto v=c.views.begin();v!=c.views.end();++v)if(c.viewUsed[v->first]<c.viewUsed[old->first])old=v;c.views.erase(old);++revision;}
    if(!roomForImage(rendered.pixels.size())){++missing;continue;}it=c.views.emplace(facing,std::move(rendered)).first;++revision;
   }image=&it->second;c.viewUsed[facing]=++useSerial;}
  visuals.push_back({x,y,image});
 }
 std::sort(visuals.begin(),visuals.end(),[](const Visual&a,const Visual&b){return a.y<b.y;});
 uint64_t hash=(uint64_t(revision)<<32)^uint64_t(facing+1)*1099511628211ull^uint64_t(next.shape+1)*16777619ull^missing;for(const auto&v:visuals){hash=(hash^uint32_t(v.x))*1099511628211ull;hash=(hash^uint32_t(v.y))*1099511628211ull;hash=(hash^uintptr_t(v.image))*1099511628211ull;}if(hash!=visualSignature){visualSignature=hash;++paintRevision;}
}
static bool visible(){return shared&&shared->formationPreview.active&&extra.formationEnabled&&!snap.chat;}
static void draw(Graphics&g){
 for(const auto&v:visuals){auto&i=*v.image;Bitmap bitmap(i.width,i.height,i.width*4,PixelFormat32bppPARGB,reinterpret_cast<BYTE*>(const_cast<uint32_t*>(i.pixels.data())));g.DrawImage(&bitmap,v.x+i.x,v.y+i.y,i.width,i.height);}
 if(visible()){const wchar_t*names[]={L"矩形横队",L"分散横队",L"圆形聚集",L"扇形"};std::wstring text=L"队形 · "+std::wstring(names[std::clamp(preview.shape,0,3)])+(extra.formationMouse==1?L" · 左拖排列 / 左击聚集 · Esc 取消":L" · 右拖排列 / 右击聚集 · Esc 取消");
  if(missing)text+=L" · "+std::to_wstring(missing)+L" 个模型未载入";drawText(g,text,18,44,12,Color(230,220,244,255));}
}
}
static bool formationHotkey(WORD key){
 if(!shared||!extra.formationEnabled||!shared->formationPreview.active)return false;
 uint32_t normalized=key;if((extra.formationHold&255)==VK_MENU)normalized&=~(4u<<8);if((extra.formationHold&255)==VK_CONTROL)normalized&=~(2u<<8);if((extra.formationHold&255)==VK_SHIFT)normalized&=~(1u<<8);
 static typekeys::Tap tap;bool single=false,twice=false;for(auto k:extra.formationKeys)if(k&&(k&0x7ff)==normalized){single|=!(k&0x8000);twice|=(k&0x8000)!=0;}
 if(single||twice){if(twice){if(!tap.press(normalized,GetTickCount(),commandMatch(),350))return true;normalized|=0x8000;}if(shared->formationKey==shared->formationKeyAck){shared->formationKeyCode=normalized;MemoryBarrier();InterlockedIncrement((volatile LONG*)&shared->formationKey);wakeGame();}return true;}return false;
}
