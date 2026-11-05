#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../../external_reader/source/camera_assets.h"
#include "../../external_reader/source/radar_scene.h"
#include "../../external_reader/source/radar_publication.h"
#include <cassert>
#include <cstdio>
using namespace external;
namespace {
std::vector<uint8_t> memory(0x1000000);uint32_t failAt=0;unsigned revisionReads=0;
template<class T>void put(uint32_t p,const T&v){std::memcpy(memory.data()+p,&v,sizeof v);}
struct Read {
 bool operator()(uint32_t at,void*out,size_t n)const{if(at==0x822CF8)++revisionReads;if(uint64_t(at)+n>memory.size()||(failAt>=at&&failAt<uint64_t(at)+n))return false;std::memcpy(out,memory.data()+at,n);return true;}
 bool readFresh(uint32_t at,void*out,size_t n){return (*this)(at,out,n);}
 bool imagePath(wchar_t*,DWORD*){return false;}
};
using TileContext=cameraassets::Context<Read>;
struct TileAssets:TileContext{
 using TileContext::TileContext;
 bool palette(bool,std::array<uint32_t,256>&out){out.fill(0xffffffffu);return true;}
 bool tile(uint32_t,unsigned,const std::array<uint32_t,256>&,voxel::Image&out){out.width=60;out.height=30;out.x=-30;out.y=-15;out.pixels.assign(1800,0xff2266aau);out.nativeDepth.enabled=true;out.nativeDepth.gradient=nativesurface::Gradient::Flat;return true;}
};
struct RankAssets:TileAssets{
 using TileAssets::TileAssets;
 bool captureActor(uint32_t,uint32_t,uint32_t,ActorArt&,const visual::Pose*,int,uint32_t){return true;}
 bool refreshActorPalette(ActorArt&,bool&changed){changed=false;return true;}
 bool renderSharedActor(const ActorArt&,const visual::Pose&,std::shared_ptr<const voxel::Image>&out){static auto image=[](){auto p=std::make_shared<voxel::Image>();p->width=p->height=1;p->pixels={0xff334455};return p;}();out=image;return true;}
};
void insignia(){
 constexpr uint32_t shape=0x200000,convert=0x300000,row=0x310000;
 put(0xAC147C,shape);put(shape+2,uint16_t(8));put(shape+4,uint16_t(8));put(shape+6,uint16_t(20));
 for(int frame:{14,15,19}){const uint32_t h=shape+8+frame*24,offset=488+frame*3;put(h,uint16_t(4));put(h+2,uint16_t(4));put(h+4,uint16_t(3));put(h+6,uint16_t(1));put(h+20,offset);put(shape+offset,uint8_t(1));if(frame==15){put(shape+offset+1,uint8_t(1));put(shape+offset+2,uint8_t(1));}}
 put(0x87F6C4,convert);put(convert,uint32_t(0x7E5358));put(convert+4,uint32_t(2));put(convert+0x16C,1);put(convert+0x174,row);put(row+2,uint16_t(0xffff));put(0x8A0DD0,std::array<int,6>{{11,3,0,3,5,2}});
}
}
int main(){
 Read read;cameraassets::Context<Read> assets(read);radar::SceneRenderer<decltype(assets)> renderer;renderer.outputSized(true);xbrzadapter::gpuPresentation(false);
 auto terrain=std::make_shared<radar::Terrain>();auto&g=terrain->geo;g.width=g.height=4;g.offsetX=g.offsetY=4;g.rw=g.rh=g.rawWidth=g.rawHeight=8;g.factor=1;g.cells=0x100000;g.house=0x400000;g.count=4096;g.bounds={4,4,0,0,4,4};assert(g.valid());
 terrain->reveal=true;terrain->colors.resize(512*512);terrain->baseColors.resize(512*512);terrain->art.resize(512*512);terrain->colors[5*512+5]=0x193347;terrain->baseColors[5*512+5]=0x193347;
 radar::Frame frame;frame.terrain=terrain;frame.frame=1;frame.tick=1;frame.markerScanValid=true;
 revisionReads=0;auto first=renderer.render(frame,120,60,read,assets,{},1,false,true,false,true);assert(first&&first->width==120&&first->height==60&&revisionReads==1);assert(renderer.nativeStats().pages==0);assert(std::any_of(first->pixels.begin(),first->pixels.end(),[](auto p){return p==0xff193347;}));
 failAt=0x822CF8;assert(!renderer.render(frame,120,60,read,assets,{},1,false,true,false,true));failAt=0;assert(renderer.render(frame,120,60,read,assets,{},1,false,true,false,true));assert(renderer.nativeStats().pages==0);
 auto previous=frame;previous.scene=first;assert(radar::reusableRealScene(previous,frame,120,60));
 assert(!radar::reusableRealScene(previous,frame,121,60));previous.scene.reset();assert(!radar::reusableRealScene(previous,frame,120,60));previous.scene=first;
 previous.frame=2;assert(!radar::reusableRealScene(previous,frame,120,60));previous.frame=1;
 auto different=std::make_shared<radar::Terrain>(*terrain);auto changed=frame;changed.terrain=different;different->reveal=false;assert(!radar::reusableRealScene(previous,changed,120,60));different->reveal=true;different->geo.cells+=4;assert(!radar::reusableRealScene(previous,changed,120,60));
 // A static retained scene still has to execute TMP draws. baseScene only
 // contains RGB underlays and cannot satisfy render's default forceFrame=false.
 auto withTile=std::make_shared<radar::Terrain>(*terrain);withTile->art[5*512+5].type=0x250000;auto staticFrame=frame;staticFrame.terrain=withTile;
 TileAssets tileAssets(read);radar::SceneRenderer<TileAssets> tileRenderer;tileRenderer.outputSized(true);
 auto staticScene=tileRenderer.render(staticFrame,120,60,read,tileAssets,{},1,false,true,false,false);
 assert(staticScene&&std::any_of(staticScene->pixels.begin(),staticScene->pixels.end(),[](auto p){return p==0xff2266aau;}));
 // Unlike setting region.active, outputSized preserves radar decoration.
 insignia();radar::Marker veteran;veteran.kind=1;veteran.u=.2;veteran.v=.3;veteran.rank=14;radar::Marker elite=veteran;elite.u=.6;elite.rank=15;frame.markers={veteran,elite};
 sceneextras::Layers layers;radar::Scene decorated;decorated.width=120;decorated.height=60;decorated.pixels.resize(7200);sceneextras::RadarCPU cpu(decorated);sceneextras::radarDecorations(frame,120,60,read,assets,layers,cpu);
 assert(std::count(decorated.pixels.begin(),decorated.pixels.end(),0xffffffffu)==0);
 std::vector<camera::ObjectCanvas::Draw>rankDraws;
 auto captureRanks=[&]{rankDraws.clear();sceneextras::radarRanks(frame,read,layers,[&](const auto&image,double x,double y){rankDraws.push_back({image,float(x),float(y)});});};
 captureRanks();assert(rankDraws.size()==2&&rankDraws[0].image->width==3&&rankDraws[0].image->skipZ);
 auto frozenRank=rankDraws[0].image;captureRanks();assert(rankDraws[0].image==frozenRank);
 failAt=0xAC147C;layers.begin();captureRanks();assert(rankDraws.empty()&&frozenRank->pixels.size()==3);failAt=0;
 // Production PIPS now enters the same native SHP compositor as world art.
 // Compare actual output at half/native/double scale, then promotion, motion,
 // failure and removal. A rank change must change pixels, not metadata alone.
 RankAssets rankAssets(read);radar::SceneRenderer<RankAssets> rankRenderer;rankRenderer.outputSized(true);
 auto rankFrame=frame;rankFrame.markers.resize(1);rankFrame.markers[0].x=rankFrame.markers[0].y=5*256;rankFrame.markers[0].object=0x650000;rankFrame.markers[0].type=0x660000;rankFrame.markers[0].id=3;
 auto renderRank=[&](int w=240,int h=120){return rankRenderer.render(rankFrame,w,h,read,rankAssets,{},1,false,true,false,true);};
 auto whites=[](const auto&scene){return std::count(scene->pixels.begin(),scene->pixels.end(),0xffffffffu);};
 auto originalRank=renderRank();assert(originalRank&&whites(originalRank)==1);
 auto sameRank=renderRank();assert(sameRank==originalRank&&rankRenderer.stageStats().reused);
 rankFrame.markers[0].rank=15;auto eliteRank=renderRank();assert(eliteRank&&eliteRank!=originalRank&&eliteRank->pixels!=originalRank->pixels&&whites(eliteRank)==3);assert(renderRank()==eliteRank);
 auto smallRank=renderRank(120,60),largeRank=renderRank(480,240);assert(smallRank&&largeRank&&whites(smallRank)<whites(eliteRank)&&whites(largeRank)==12);
 const auto frozenPixels=eliteRank->pixels;failAt=0x822CF8;rankFrame.markers[0].u=.8;assert(!renderRank()&&eliteRank->pixels==frozenPixels);failAt=0;
 auto movedRank=renderRank();assert(movedRank&&movedRank->pixels!=eliteRank->pixels);
 rankFrame.markers[0].rank=-1;auto noRank=renderRank();assert(noRank&&whites(noRank)==0&&whites(eliteRank)==3);
 // Preparation for multiple watch views also retains world inputs only.
 frame.markers.clear();renderer.prepareWorld(true);auto ready=renderer.render(frame,320,256,read,assets,{0,0,240,120,true},1,false,true,false,true);assert(ready&&renderer.preparedWorld()&&renderer.preparedWorld()->ground.retainedGround()&&renderer.nativeStats().pages==0);
 std::puts("PASS production SceneRenderer: output-sized radar retains colors/domain; static forceFrame=false executes TMP; fallback reuse rejects size/reveal/identity/rewind changes; one revision read and recovery; PIPS 14/15 native SHP output at half/native/double scale, promotion/motion/removal and retained front; watch world zero native pages");
}
