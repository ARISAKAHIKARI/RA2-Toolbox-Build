#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../../external_reader/source/camera_assets.h"
#include "../../external_reader/source/radar_scene.h"
#include RA2_REFERENCE_SCENE
#include <cassert>
#include <cstdio>
using namespace external;
namespace {
constexpr uint32_t Cells=0x100000,Cell=0x200000,House=0x300000,Scenario=0x600000;
constexpr size_t Index=5*512+5;
std::vector<uint8_t>memory(0xC00000);uint32_t failAt=0;size_t freshCalls=0,maskCaptures=0;
template<class T>void put(uint32_t p,const T&v){std::memcpy(memory.data()+p,&v,sizeof v);}
struct Read {
 bool operator()(uint32_t at,void*out,size_t n)const{if(uint64_t(at)+n>memory.size()||(failAt>=at&&failAt<uint64_t(at)+n))return false;std::memcpy(out,memory.data()+at,n);return true;}
 bool fresh(uint32_t at,void*out,size_t n)const{++freshCalls;if(at==Cells&&n==4096*4)++maskCaptures;return (*this)(at,out,n);}
 bool readFresh(uint32_t at,void*out,size_t n){return fresh(at,out,n);}
 bool imagePath(wchar_t*,DWORD*){return false;}
};
using Base=cameraassets::Context<Read>;
struct Assets:Base {using Base::Base;
 bool palette(bool,std::array<uint32_t,256>&out){out.fill(0xffffffffu);return true;}
 bool tile(uint32_t,unsigned,const std::array<uint32_t,256>&,voxel::Image&out){out.width=60;out.height=30;out.x=-30;out.y=-15;out.pixels.assign(1800,0xff2266aau);out.nativeDepth.enabled=true;out.nativeDepth.gradient=nativesurface::Gradient::Flat;return true;}
 bool shroud(bool,int f,voxel::Image&out){out.canvasWidth=out.width=60;out.canvasHeight=out.height=30;out.x=-30;out.y=-15;out.indices.assign(1800,254);for(int y=0;y<30;++y)for(int x=0;x<60;++x)if(std::abs((x+.5)-30)/30.+std::abs((y+.5)-15)/15.<=1)out.indices[y*60+x]=f==15?0:127;return true;}
 bool captureActor(uint32_t,uint32_t,uint32_t,ActorArt&,const visual::Pose*,int,uint32_t){return true;}
 bool refreshActorPalette(ActorArt&,bool&changed){changed=false;return true;}
 bool renderSharedActor(const ActorArt&,const visual::Pose&,std::shared_ptr<const voxel::Image>&out){static auto image=[](){auto p=std::make_shared<voxel::Image>();p->width=p->height=8;p->x=p->y=-4;p->pixels.assign(64,0xffff3344);p->skipZ=true;p->writeDepth=false;return p;}();out=image;return true;}
};
void setup(){
 put(radar::Map+0xF4,4);put(radar::Map+0xF8,4);put(radar::Map+0x104,4);put(radar::Map+0x108,4);
 put(radar::Map+0x13C,Cells);put(radar::Map+0x140,4096);put(radar::Map+0x1490,4);put(radar::Map+0x1498,4);
 put(radar::Map+0x1240,8);put(radar::Map+0x1244,8);put(0xA83D4C,House);put(0xA8B230,Scenario);put(0xA8ED84,1);put(0xA8E9A0,uint8_t(1));put(0xA8E378,uint8_t(1));
 put(Scenario+0x352C,100);put(Scenario+0x3534,std::array<int,3>{{100,100,100}});
 put(Cells+Index*4,Cell);put(Cell+0x24,int16_t(5));put(Cell+0x26,int16_t(5));put(Cell+0x12C,uint8_t(0));put(Cell+0x140,uint8_t(1));
 put(Cells+(Index+1)*4,Cell+0x1000);put(Cell+0x1024,int16_t(6));put(Cell+0x1026,int16_t(5));put(Cell+0x112C,uint8_t(0x10));put(Cell+0x1140,uint8_t(1));
}
}
int main(){
 setup();Read read;Assets a(read),b(read);radar::BeforeSceneRenderer<Assets>before;radar::SceneRenderer<Assets>after;before.outputSized(true);after.outputSized(true);xbrzadapter::gpuPresentation(false);
 auto hidden=std::make_shared<radar::Terrain>();auto&g=hidden->geo;g.width=g.height=4;g.offsetX=g.offsetY=4;g.rw=g.rh=g.rawWidth=g.rawHeight=8;g.factor=1;g.cells=Cells;g.house=House;g.count=4096;g.bounds={4,4,0,0,4,4};assert(g.valid());
 hidden->colors.resize(512*512);hidden->baseColors.resize(512*512);hidden->art.resize(512*512);hidden->shade.resize(512*512);hidden->baseColors[Index]=0x193347;hidden->shade[Index]={-2,-1};auto&c=hidden->art[Index];c.cell=Cell;c.type=0x250000;c.sourceColor=0x193347;c.sourceHidden=true;
 hidden->colors[Index+1]=hidden->baseColors[Index+1]=0x193347;hidden->shade[Index+1]={-1,-1};hidden->art[Index+1]=c;hidden->art[Index+1].cell=Cell+0x1000;hidden->art[Index+1].sourceHidden=false;
 auto explored=std::make_shared<radar::Terrain>(*hidden);explored->colors[Index]=0x193347;explored->art[Index].sourceHidden=false;explored->shade[Index]={-1,-1};
 radar::Frame frame;frame.terrain=hidden;frame.frame=1;frame.tick=1;frame.markerScanValid=true;auto extras=std::make_shared<sceneextras::Snapshot>();extras->frozenValid=true;frame.extras=extras;
 auto oldRender=[&]{return before.render(frame,120,60,read,a,{},1,false,true,false,true);};
 auto newRender=[&]{return after.render(frame,120,60,read,b,{},1,false,true,false,true);};
 auto oldHidden=oldRender(),newHidden=newRender();if(!oldHidden||!newHidden)std::fprintf(stderr,"initial old=%d new=%d oldcells=%llu newcells=%llu reason=%s/%s\n",bool(oldHidden),bool(newHidden),(unsigned long long)before.nativeStats().cells,(unsigned long long)after.nativeStats().cells,radar::renderFailureName(before.stageStats().failure),radar::renderFailureName(after.stageStats().failure));assert(oldHidden&&newHidden);
 put(Cell+0x12C,uint8_t(0x10));frame.tick=34;auto oldLag=oldRender(),newClear=newRender();if(!oldLag||!newClear||oldLag->pixels!=oldHidden->pixels||newClear->pixels==newHidden->pixels)std::fprintf(stderr,"reveal old=%d new=%d oldsame=%d newsame=%d fresh=%zu\n",bool(oldLag),bool(newClear),oldLag&&oldLag->pixels==oldHidden->pixels,newClear&&newClear->pixels==newHidden->pixels,freshCalls);assert(oldLag&&newClear&&oldLag->pixels==oldHidden->pixels&&newClear->pixels!=newHidden->pixels);
 const auto clearPixels=newClear->pixels;const auto draws=after.nativeStats().cells;
 for(int n=0;n<8;++n){frame.terrain=n%2?hidden:explored;frame.tick+=34;++frame.frame;put(0xA8ED84,frame.frame);auto old=oldRender(),now=newRender();assert(old&&now&&now->pixels==clearPixels);assert((old->pixels==oldHidden->pixels)==bool(n%2));}
 assert(after.nativeStats().cells==draws); // No exploration-driven terrain rebuild.
 auto current=newRender();const auto calls=maskCaptures;frame.tick+=1;assert(newRender()->pixels==clearPixels&&maskCaptures==calls); // actors may render at full rate
 frame.tick+=34;failAt=Cell+0x12C;assert(!newRender());assert(current->pixels==clearPixels);failAt=0;assert(newRender()->pixels==clearPixels);
 // Real fog/shroud changes are accepted in both directions; no monotonic OR
 // mask would satisfy this case. Failed captures never replace the last image.
 put(Cell+0x12C,uint8_t(0));frame.tick+=34;auto dark=newRender();assert(dark&&dark->pixels!=clearPixels);put(Cell+0x12C,uint8_t(0x10));frame.tick+=34;assert(newRender()->pixels==clearPixels);
 frame.tick=0;frame.frame=0;put(0xA8ED84,0);assert(newRender()); // rewind refreshes visibility
 // Exercise the actual SceneRenderer across distinct 33ms capture epochs,
 // with a moving actor defeating whole-frame reuse. Equal visibility keeps
 // its immutable mask owner, so the terrain prefix remains reusable.
 frame.terrain=explored;radar::Marker actor;actor.object=0x700000;actor.type=0x710000;actor.id=9;actor.owner=House;actor.kind=2;actor.x=actor.y=5*256;frame.markers={actor};unsigned prefixHits=0;
 for(int n=0;n<16;++n){frame.tick+=34;++frame.frame;put(0xA8ED84,frame.frame);frame.markers[0].x+=8;const auto captures=maskCaptures;auto image=newRender();assert(image&&maskCaptures>captures);prefixHits+=after.stageStats().cpuTerrainReused;
  Assets controlAssets(read);radar::SceneRenderer<Assets>control;control.outputSized(true);auto reference=control.render(frame,120,60,read,controlAssets,{},1,false,true,false,true);assert(reference&&reference->pixels==image->pixels);
 }assert(prefixHits>=15);
 put(Cell+0x12C,uint8_t(0));frame.tick+=34;auto changed=newRender();assert(changed&&!after.stageStats().cpuTerrainReused);put(Cell+0x12C,uint8_t(0x10));frame.tick+=34;assert(newRender()&&!after.stageStats().cpuTerrainReused);
 std::printf("PASS actual SceneRenderer 16 fresh visibility epochs + moving actor: terrain-prefix hits=%u/16; every frame equal to a cold renderer, alpha changes invalidate.\n",prefixHits);
 std::puts("PASS actual SceneRenderer: 2bb alternating stale terrain/shade banks reproduce flashing; current full-map visibility stays clear, unchanged terrain draws stay cached; 30Hz mask budget, read failure retention/recovery, valid darkening and frame rewind.");
}

