#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../../external_reader/source/camera_assets.h"
#include "../../external_reader/source/radar_scene.h"
#include "../../external_reader/source/scene_bank.h"
#include "../../external_reader/source/radar_navigation.h"
#include "../source/radar_layout.h"
#include <cassert>
#include <cstdio>
#include <iostream>
using namespace external;
namespace {
constexpr uint32_t Cells=0x100000,Objects=0xC00000,House=0x300000,Scenario=0x600000,Type=0x400000;
std::vector<uint8_t>memory(32*1024*1024);uint32_t failAt=0;size_t reads=0;
template<class T>void put(uint32_t at,const T&v){std::memcpy(memory.data()+at,&v,sizeof v);}
struct Read {bool operator()(uint32_t at,void*out,size_t n)const{++reads;if(uint64_t(at)+n>memory.size()||(failAt>=at&&failAt<uint64_t(at)+n))return false;std::memcpy(out,memory.data()+at,n);return true;}
 bool fresh(uint32_t at,void*out,size_t n)const{return (*this)(at,out,n);}bool readFresh(uint32_t at,void*out,size_t n){return fresh(at,out,n);}bool imagePath(wchar_t*,DWORD*){return false;}};
using Base=cameraassets::Context<Read>;
struct Assets:Base {using Base::Base;
 bool palette(bool,std::array<uint32_t,256>&p){p.fill(0xffffffffu);return true;}
 bool tile(uint32_t,unsigned sub,const std::array<uint32_t,256>&,voxel::Image&out){out.width=60;out.height=30;out.x=-30;out.y=-15;out.pixels.assign(1800,0xff000000u|((sub*0x175323u)&0xffffffu));out.nativeDepth.enabled=true;out.nativeDepth.gradient=nativesurface::Gradient::Flat;return true;}
 bool shroud(bool,int frame,voxel::Image&out){out.canvasWidth=out.width=60;out.canvasHeight=out.height=30;out.x=-30;out.y=-15;out.indices.assign(1800,254);for(int y=0;y<30;++y)for(int x=0;x<60;++x)if(std::abs((x+.5)-30)/30.+std::abs((y+.5)-15)/15.<=1)out.indices[y*60+x]=frame==15?0:127;return true;}
};
radar::Geometry setup(int width=64,int height=64,bool raised=false){
 std::fill(memory.begin(),memory.end(),0);mapbounds::Bounds bounds{width,height,2,2,width-4,height-8};
 put(radar::Map+0xF4,width);put(radar::Map+0xF8,height);put(radar::Map+0xFC,bounds.left);put(radar::Map+0x100,bounds.top);put(radar::Map+0x104,bounds.columns);put(radar::Map+0x108,bounds.rows);
 put(radar::Map+0x13C,Cells);put(radar::Map+0x140,512*512);put(radar::Map+0x1488,.39130434f);put(0xA83D4C,House);put(0xA8B230,Scenario);put(Scenario+0x352C,100);put(Scenario+0x3534,std::array<int,3>{{100,100,100}});put(0xA8ED84,1);put(0xA8E9A0,uint8_t(1));put(0xA8E378,uint8_t(1));
 put(0xA8ED2C,0x500000u);put(0xA8ED30,1);put(0xA8ED38,1);put(0x500000,Type);put(Type+0x2F0,1);
 size_t count=0;for(int y=1;y<512;++y)for(int x=1;x<512;++x)if(bounds.legal(x,y)){auto p=Objects+uint32_t(count++)*0x150;put(Cells+uint32_t(y*512+x)*4,p);put(p+0x24,int16_t(x));put(p+0x26,int16_t(y));put(p+0x11B,int8_t(raised?(x*13+y*7)%7:0));put(p+0x11C,uint8_t(raised&&(x+y)%5==0));put(p+0x12C,uint8_t(0x10));put(p+0x140,uint8_t(1));put(p+0xEC,(x+y)%12);put(p+0x38,0);put(p+0x44,-1);put(p+0x48,-1);put(p+0x11A,uint8_t((x*7+y*13)%256));}
 radar::Geometry g;g.width=width;g.height=height;g.bounds=bounds;g.cells=Cells;g.count=512*512;g.house=House;g.factor=.39130434f;g.rw=g.rawWidth=2*bounds.columns;g.rh=g.rawHeight=2*bounds.rows+2;g.offsetX=width-2*bounds.left-1;g.offsetY=width+2*bounds.top+1;return g;
}
void publish(const radar::Geometry&g){put(radar::Map+0xFC,g.bounds.left);put(radar::Map+0x100,g.bounds.top);put(radar::Map+0x104,g.bounds.columns);put(radar::Map+0x108,g.bounds.rows);put(radar::Map+0x1490,g.offsetX);put(radar::Map+0x1498,g.offsetY);put(radar::Map+0x14A4,g.rw);put(radar::Map+0x14A8,g.rh);put(radar::Map+0x1240,g.rawWidth);put(radar::Map+0x1244,g.rawHeight);}
}
int main(int argc,char**){
 if(argc>1){int width,height,raised,left,top,columns,rows,bw,bh,ox,oy,rw,rh;
  while(std::cin>>width>>height>>raised>>left>>top>>columns>>rows>>bw>>bh>>ox>>oy>>rw>>rh){auto source=setup(width,height,raised);source.bounds={width,height,left,top,columns,rows};source.rawWidth=bw;source.rawHeight=bh;source.offsetX=ox;source.offsetY=oy;source.rw=rw;source.rh=rh;radar::ActiveProjection p;radar::Geometry result;assert(p.resolve(Read{},source,result));std::cout<<result.offsetX<<' '<<result.offsetY<<' '<<result.rawWidth<<' '<<result.rawHeight<<'\n';}return 0;
 }
 Read read;auto native=setup();publish(native);radar::Sampler<Read>sampler(read),old(read);sampler.effects(false,false);old.effects(false,false);
 auto sample=[&](auto&s,bool active){const auto revision=s.revision();for(int n=0;n<20;++n){assert(s.step(1,4096,true,true,active));if(s.hasTerrain()&&s.revision()!=revision)return s.snapshot(100,1);}assert(false);return radar::Frame{};};
 auto initial=sample(sampler,true);assert(initial.terrain&&!initial.terrain->geo.projectionAdjusted);Assets assets(read);radar::SceneRenderer<Assets>renderer;renderer.outputSized(true);xbrzadapter::gpuPresentation(false);
 auto render=[&](const radar::Frame&f){for(int n=0;n<20;++n)if(auto scene=renderer.render(f,320,160,read,assets,{},1,false,true,false,true))return scene;assert(false);return std::shared_ptr<const radar::Scene>{};};
 auto first=render(initial);const auto originalPixels=first->pixels;auto previous=initial;previous.scene=first;
 // Outputs below come from actual native 654490 replay on the same synthetic
 // 64x64 flat map. Only live bounds/offset/working dimensions change; the
 // allocated 120x114 RGB backing remains unchanged throughout the trigger.
 native.bounds={64,64,12,18,24,20};native.offsetX=39;native.offsetY=101;native.rw=48;native.rh=42;publish(native);
 const auto current=radar::publicationContext(read);assert(!radar::sameWorld(previous,current));auto wrong=sample(old,false),correct=sample(sampler,true);assert(wrong.terrain->geo.rawWidth==120&&wrong.terrain->geo.rawHeight==114);
 const auto&g=correct.terrain->geo;assert(g.projectionAdjusted&&g.rawWidth==48&&g.rawHeight==42&&g.sourceWidth()==120&&g.sourceHeight()==114&&radar::sameWorld(correct,current));assert(!radar::reusableRealScene(previous,correct,320,160));assert(!sampler.radarProjectionCompatible(wrong));
 auto cropped=render(correct);assert(cropped->pixels!=originalPixels);auto oldScene=render(wrong);assert(oldScene->pixels!=cropped->pixels);assert(render(correct)->pixels==cropped->pixels);
 double u=0,v=0;g.project(50,60,u,v);assert(std::abs(u-29./48)<1e-9&&std::abs(v-9./42)<1e-9);int x=0,y=0,oldX=0,oldY=0;assert(radar::navigate(g,193,81,322,162,320,240,x,y)&&radar::navigate(wrong.terrain->geo,193,81,322,162,320,240,oldX,oldY)&&(x!=oldX||y!=oldY));
 auto normalLayout=radarlayout::place(1280,720,1280,720,10,1,0,0,{},120.*30/(114.*15),0,0,120*30,114*15);auto cropLayout=radarlayout::place(1280,720,1280,720,10,1,0,0,{},48.*30/(42.*15),0,0,48*30,42*15);assert(normalLayout.valid&&cropLayout.valid&&(normalLayout.destination.w!=cropLayout.destination.w||normalLayout.destination.h!=cropLayout.destination.h));
 // Vertical-only change: columns and native allocation stay constant.
 native.bounds={64,64,12,24,24,14};native.offsetY=113;native.rh=30;publish(native);auto vertical=sample(sampler,true);assert(vertical.terrain->geo.rawWidth==48&&vertical.terrain->geo.rawHeight==30&&vertical.terrain->geo.offsetY==113);assert(render(vertical)->pixels!=cropped->pixels);
 // Required projection reads fail closed and recover without poisoning cache.
 native.bounds={64,64,6,8,50,46};native.offsetX=51;native.offsetY=81;native.rw=100;native.rh=94;publish(native);auto retained=vertical.terrain;failAt=Objects+0x24;assert(!sampler.step(1,4096,true,true,true)&&sampler.terrainFrame(0).terrain==retained);failAt=0;auto expanded=sample(sampler,true);assert(expanded.terrain->geo.rawWidth==100&&expanded.terrain->geo.rawHeight==94&&render(expanded));
 native.bounds={64,64,2,2,60,56};native.offsetX=59;native.offsetY=69;native.rw=120;native.rh=114;publish(native);auto restored=sample(sampler,true);assert(!restored.terrain->geo.projectionAdjusted&&sampler.radarProjectionCompatible(restored)&&render(restored)->pixels==originalPixels);
 // The read-only projection cache performs no new Cell reads for unchanged
 // context, but a reset (new process/match) cannot reuse its address identity.
 radar::ActiveProjection projection;radar::Geometry a,b;assert(projection.resolve(read,native,a));const auto before=reads;assert(projection.resolve(read,native,b)&&reads==before&&a.same(b));projection.reset();assert(projection.resolve(read,native,b)&&reads>before);
 std::puts("PASS actual Sampler/Renderer/Live-mask/scene-publication/layout/navigation: same-frame trigger shrink, vertical-only change, expansion and restore with unchanged 120x114 allocation; old 8437 span counterexample reproduced; failed projection read retention/recovery; normal projection shares world source, adjusted projection remains private.");
}
