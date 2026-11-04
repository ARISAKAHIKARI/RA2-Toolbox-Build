#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../../external_reader/source/camera_assets.h"
#include "../../external_reader/source/scene_extra_layers.h"
#include "../../external_reader/source/native_draw_state.h"
#include <cassert>
#include <cstdio>
using namespace external;
namespace {
constexpr uint32_t Type=0x200000,Shape=0x300000,Cell=0x400000;
std::vector<uint8_t> memory(0x1000000);uint32_t failAt=0;unsigned theaterReads=0;
template<class T>void put(uint32_t at,T value){std::memcpy(memory.data()+at,&value,sizeof value);}
struct Read {
 bool operator()(uint32_t at,void*out,size_t n)const{
  if(at==0x822CF8)++theaterReads;
  if(uint64_t(at)+n>memory.size()||(failAt>=at&&failAt<uint64_t(at)+n))return false;
  std::memcpy(out,memory.data()+at,n);return true;
 }
 bool readFresh(uint32_t at,void*out,size_t n){return (*this)(at,out,n);}
 bool imagePath(wchar_t*,DWORD*){return false;}
};
struct Materials {
 bool paletteReady=true,spriteReady=true;int count=2;
 bool colors(std::array<uint32_t,256>&out){out.fill(0xffffffffu);return paletteReady;}
 bool convertPalette(uint32_t,std::array<uint32_t,256>&out){return colors(out);}
 bool spritePalette(uint32_t,const std::string&,bool,uint32_t,std::array<uint32_t,256>&out){return colors(out);}
 bool particlePalette(std::array<uint32_t,256>&out){return colors(out);}
 bool debrisPalette(uint32_t,std::array<uint32_t,256>&out){return colors(out);}
 bool palette(bool,std::array<uint32_t,256>&out){return colors(out);}
 std::vector<std::string> spriteNames(uint32_t,uint32_t,bool){return {};}
 int resourceFrames(uint32_t,uint32_t,bool=false){return count;}
 bool resourceSprite(uint32_t,uint32_t,const std::array<uint32_t,256>&,int,voxel::Image&out,bool,bool){if(!spriteReady)return false;out.width=out.height=1;out.pixels={0xffffffffu};return true;}
 bool overlayDepthShape(voxel::Image&,unsigned,bool){return true;}
};
}
int main(){
 Read read;Materials assets;sceneextras::Layers layers;radar::Frame frame;
 auto terrain=std::make_shared<radar::Terrain>();terrain->geo.bounds={4,4,0,0,4,4};terrain->art.resize(512*512);
 auto&cell=terrain->art[5*512+5];cell.cell=Cell;cell.overlayType=cell.overlaySourceType=Type;cell.overlay=1;
 frame.terrain=terrain;put(Type+0xA4,Shape);unsigned draws=0;auto draw=[&](const auto&...){++draws;};
 auto ground=[&](){layers.begin();draws=0;return layers.ground(frame,read,assets,{},draw);};
 assert(ground()&&draws==2); // Body and shadow are both required.
 assets.paletteReady=false;assert(!ground()&&draws==0); // Even a decoded image must not publish a missing live palette.
 assets.paletteReady=true;assert(ground()&&draws==2);
 for(uint32_t field:{Type+0xA4,Type+0x2AA,Type+0x2A9}){failAt=field;assert(!ground());failAt=0;assert(ground());}
 assets.count=0;assert(!ground());assets.count=2;assert(ground());
 layers.reset();assets.spriteReady=false;assert(!ground());assets.spriteReady=true;assert(ground());
 put(Type+0xA4,uint32_t(0));assert(ground()&&draws==0);put(Type+0xA4,Shape);assert(ground());
 // Removing a confirmed member is valid; never resurrect an old cached overlay.
 auto empty=std::make_shared<radar::Terrain>(*terrain);empty->art[5*512+5].overlayType=0;frame.terrain=empty;assert(ground()&&draws==0);frame.terrain=terrain;
 cell.overlayType=0;cell.smudgeType=Type;layers.reset();put(Type+0x298,1);put(Type+0x29C,1);
 assert(ground()&&draws==1);failAt=Type+0x298;assert(!ground());failAt=0;assets.paletteReady=false;assert(!ground());assets.paletteReady=true;assert(ground());
 // HWND is a DWORD. Low-byte-zero windows must not skip the real list.
 put(0xB73550,uint32_t(0));auto captured=sceneextras::capture(read,0x500000,1,true,false);assert(captured.frozenValid&&captured.frozen.empty());
 failAt=0xB73550;captured=sceneextras::capture(read,0x500000,1,true,false);assert(!captured.frozenValid);failAt=0;
 for(uint32_t hwnd:{1u,0x100u,0x112200u,0x112233u}){
  put(0xB73550,hwnd);captured=sceneextras::capture(read,0x500000,1,true,false);assert(captured.frozenValid);
  failAt=0x8B3D14;captured=sceneextras::capture(read,0x500000,1,true,false);assert(!captured.frozenValid);failAt=0;
 }
 // A non-allied, undetected fully cloaked object stays hidden for every
 // nonzero window handle; headless behavior is the original separate case.
 constexpr uint32_t Actor=0x600000,Viewer=0x500000,Enemy=0x700000;
 put(Actor+0x21C,Enemy);put(Actor+0x220,2);put(Viewer+0x30,0);
 for(uint32_t hwnd:{1u,0x100u,0x112200u,0x112233u}){put(0xB73550,hwnd);assert(!nativedraw::capture(read,Actor,Type,3,Viewer,1).visible);}
 put(0xB73550,uint32_t(0));assert(nativedraw::capture(read,Actor,Type,3,Viewer,1).visible);
 failAt=0xB73550;assert(!nativedraw::capture(read,Actor,Type,3,Viewer,1).visible);failAt=0;
 auto extras=std::make_shared<sceneextras::Snapshot>();frame.extras=extras;
 auto frozen=[&](){layers.begin();draws=0;return layers.frozen(frame,read,assets,{},1.,draw);};
 assert(!frozen());extras->frozenValid=true;assert(frozen()&&draws==0);
 sceneextras::Frozen building;building.point={5*256,5*256,0};building.records.push_back({Type,0,1,0});extras->frozen.push_back(building);
 assert(frozen()&&draws==2); // A null optional bib does not block publication.
 failAt=Type+0xA4;assert(!frozen());failAt=0;assets.paletteReady=false;assert(!frozen());assets.paletteReady=true;assert(frozen());
 assets.count=0;assert(!frozen());assets.count=2;assert(frozen());
 terrain->reveal=true;extras->frozenValid=false;assert(frozen()&&draws==0);terrain->reveal=false;
 // Production asset revision: unknown must not masquerade as theatre zero,
 // change the old cache key, or leak it across Context identities.
 cameraassets::Context<Read> context(read);put(0x822CF8,uint32_t(2));uint64_t revision=0;
 theaterReads=0;assert(context.revisionChecked(revision)&&theaterReads==1);const auto old=revision;
 failAt=0x822CF8;revision=77;assert(!context.revisionChecked(revision)&&revision==77);assert(context.revision()==old);
 failAt=0;assert(context.revisionChecked(revision)&&revision==old);
 put(0x822CF8,uint32_t(6));assert(!context.revisionChecked(revision)&&context.revision()==old);
 put(0x822CF8,uint32_t(0));assert(context.revisionChecked(revision)&&revision!=old);
 failAt=0x822CF8;context.identity(7,8);revision=99;assert(!context.revisionChecked(revision)&&revision==99&&context.revision()==0);
 std::puts("PASS: production material publication/recovery, full DWORD HWND frozen/cloak matrix, theatre revision without cache-key flashing");
}
