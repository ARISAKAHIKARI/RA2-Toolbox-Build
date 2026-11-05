#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../../external_reader/source/radar_scene.h"
#include <cassert>
#include <cstdio>
using namespace external;
struct Assets {
 struct ActorArt{};
 uint64_t revision(){return 1;}static size_t cost(const ActorArt&){return sizeof(ActorArt);}
 bool palette(bool,std::array<uint32_t,256>&p){p.fill(0xffffffff);return true;}
 bool shroud(bool,int,voxel::Image&){return false;}
 bool tile(uint32_t,unsigned,const std::array<uint32_t,256>&,voxel::Image&){return false;}
 bool overlay(uint32_t,uint32_t,int,voxel::Image&,bool=false){return false;}
 bool convertPalette(uint32_t,std::array<uint32_t,256>&){return false;}
 bool spritePalette(uint32_t,const std::string&,bool,uint32_t,std::array<uint32_t,256>&){return false;}
 bool particlePalette(std::array<uint32_t,256>&){return false;}
 bool debrisPalette(uint32_t,std::array<uint32_t,256>&){return false;}
 std::vector<std::string>spriteNames(uint32_t,uint32_t,bool){return {};}
 int resourceFrames(uint32_t,uint32_t,bool=false){return 0;}
 bool resourceSprite(uint32_t,uint32_t,const std::array<uint32_t,256>&,int,voxel::Image&,bool,bool){return false;}
 bool overlayDepthShape(voxel::Image&,unsigned,bool){return false;}
 void buildingPose(uint32_t,uint32_t,visual::Pose&){}
 bool captureActor(uint32_t,uint32_t,uint32_t,ActorArt&){return true;}
 bool captureAnimation(uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,ActorArt&){return true;}
 bool renderActor(const ActorArt&,const visual::Pose&,voxel::Image&i){i.width=i.height=1;i.pixels={0xff334455};i.writeDepth=false;return true;}
};
int main(){
 std::vector<uint8_t>mem(0x1000000);auto put=[&](uint32_t p,auto v){std::memcpy(mem.data()+p,&v,sizeof v);};
 auto read=[&](uint32_t p,void*out,size_t n){if(uint64_t(p)+n>mem.size())return false;std::memcpy(out,mem.data()+p,n);return true;};
 constexpr uint32_t shape=0x200000,convert=0x300000,row=0x310000;
 put(0xAC147C,shape);put(shape+2,uint16_t(8));put(shape+4,uint16_t(8));put(shape+6,uint16_t(20));
 for(int rank:{14,15}){const uint32_t h=shape+8+rank*24,offset=488+rank*3;put(h,uint16_t(4));put(h+2,uint16_t(4));put(h+4,uint16_t(3));put(h+6,uint16_t(1));put(h+20,offset);put(shape+offset,uint8_t(1));if(rank==15){put(shape+offset+1,uint8_t(1));put(shape+offset+2,uint8_t(1));}}
 put(0x87F6C4,convert);put(convert,uint32_t(0x7E5358));put(convert+4,uint32_t(2));put(convert+0x16C,1);put(convert+0x174,row);put(row+2,uint16_t(0xffff));put(0x8A0DD0,std::array<int,6>{{11,3,0,3,5,2}});
 auto t=std::make_shared<radar::Terrain>();auto&g=t->geo;g.width=g.height=4;g.offsetX=g.offsetY=4;g.rw=g.rh=g.rawWidth=g.rawHeight=8;g.factor=1;g.cells=0x100000;g.house=0x400000;g.count=4096;g.bounds={4,4,0,0,4,4};assert(g.valid());
 t->reveal=true;t->colors.resize(512*512);t->baseColors.resize(512*512);t->art.resize(512*512);t->colors[5*512+5]=t->baseColors[5*512+5]=0x193347;
 Assets a;radar::SceneRenderer<Assets> r;r.outputSized(true);radar::Frame f;f.terrain=t;f.frame=1;f.tick=1;f.markerScanValid=true;
 radar::Marker q;q.object=0x650000;q.type=0x660000;q.id=3;q.kind=1;q.x=q.y=5*256;q.u=.2;q.v=.3;q.rank=14;f.markers={q};
 auto render=[&](int w=240,int h=120){return r.render(f,w,h,read,a,{},1,false,true,false,true);};
 auto whites=[](const auto&s){return std::count(s->pixels.begin(),s->pixels.end(),0xffffffffu);};
 auto veteran=render();assert(veteran&&whites(veteran)==1);assert(render()==veteran);
 f.markers[0].rank=15;auto elite=render();assert(elite&&whites(elite)==3&&elite->pixels!=veteran->pixels);assert(render()==elite);
 auto small=render(120,60),large=render(480,240);assert(small&&large&&whites(small)<3&&whites(large)==12);
 f.markers[0].u=.8;auto moved=render();assert(moved&&moved->pixels!=elite->pixels);
 f.markers[0].rank=-1;auto gone=render();assert(gone&&whites(gone)==0&&whites(elite)==3);
 std::puts("PASS rank SHP: production decode, shared image cache and scene compositor; half/native/double output, promotion/move/removal and retained front");
}
