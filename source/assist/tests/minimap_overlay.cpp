#include <cassert>
#include <iostream>
#include <map>
#include <cstring>
#include "../../external_reader/source/radar_map.h"
#include "../source/radar_layout.h"
#include "../../external_reader/source/radar_navigation.h"
#include "../../external_reader/source/radar_publication.h"
struct Memory {
 std::map<uint32_t,uint8_t> bytes;
 bool operator()(uint32_t at,void*out,size_t n)const{auto*p=(uint8_t*)out;for(size_t i=0;i<n;++i){auto it=bytes.find(at+i);if(it==bytes.end())return false;p[i]=it->second;}return true;}
 template<class T>void set(uint32_t at,T v){auto*p=(uint8_t*)&v;for(size_t i=0;i<sizeof v;++i)bytes[at+i]=p[i];}
 void zero(uint32_t at,int n){for(int i=0;i<n;++i)bytes[at+i]=0;}
};
struct Read{Memory*m;bool operator()(uint32_t p,void*out,size_t n)const{return (*m)(p,out,n);}};
int main(){
 // Real-scene scale uses the native 30/15 projection span. Diagram mode
 // continues using the original 160x140 radar-box percentage.
 auto native=external::mapbounds::nativeViewport(80,80,10);
 assert(native.valid()&&native.width==240&&native.height==120);
 auto real=[&](int scale,int cw=1920,int ch=1080){return radarlayout::place(1920,1080,cw,ch,scale,1,0,0,{},2,0,0,native.width,native.height);};
 auto actual=real(100);assert(actual.valid&&actual.destination.w==242&&actual.destination.h==122);
 actual=real(10);assert(actual.valid&&actual.destination.w==26&&actual.destination.h==14);
 actual=real(200);assert(actual.valid&&actual.destination.w==482&&actual.destination.h==242);
 actual=real(100,3840,2160);assert(actual.valid&&actual.destination.w==482&&actual.destination.h==242);
 actual=real(100,3840,1080);assert(actual.valid&&actual.destination.w==242&&actual.destination.h==122);
 auto diagramLayout=radarlayout::place(1920,1080,1920,1080,100,1,0,0,{});
 assert(diagramLayout.valid&&diagramLayout.destination.w==162&&diagramLayout.destination.h==142);
 diagramLayout=radarlayout::place(1920,1080,1920,1080,10,1,0,0,{});
 assert(diagramLayout.valid&&diagramLayout.destination.w==18&&diagramLayout.destination.h==16);
 auto fractional=radarlayout::place(1920,1080,1920,1080,100,1,0,0,{},2,0,0,240.01,120.01);
 assert(fractional.valid&&fractional.destination.w==243&&fractional.destination.h==123);
 fractional=radarlayout::place(1920,1080,1920,1080,200,1,0,0,{},2,0,0,240.01,120.01);
 assert(fractional.valid&&fractional.destination.w==483&&fractional.destination.h==243);
 auto large=radarlayout::place(1920,1080,1920,1080,200,1,0,0,{},2,0,0,native.width,native.height);
 assert(large.valid&&large.destination.w<=1920&&large.destination.h<=1080);
 auto capped=radarlayout::place(8000,6000,8000,6000,100,1,0,0,{},2,0,0,5000,2000);
 assert(capped.valid&&capped.destination.w==2050&&capped.destination.h<=2050);
 for(int mode=0;mode<=3;++mode){
  auto plane=radarlayout::aircraft(1920,1080,640,480);
  auto fit=radarlayout::place(1920,1080,640,480,200,mode,300,300,plane,2,1,2,5000,2500);
  assert(fit.valid&&fit.destination.x>=0&&fit.destination.y>=0&&fit.destination.x+fit.destination.w<=640&&fit.destination.y+fit.destination.h<=480);
  assert(!radarlayout::overlap(fit.destination,fit.source)&&!radarlayout::overlap(fit.destination,plane));
 }
 // Actual aspect ratio participates in placement, not a later resize.
 for(double aspect:{.25,.5,1.,2.,4.}){
  auto plane=radarlayout::aircraft(1920,1080,1920,1080);
  auto box=radarlayout::place(1920,1080,1920,1080,200,2,0,0,plane,aspect);
  assert(box.valid&&box.destination.x==10&&box.destination.y+box.destination.h==1070);
  auto upper=radarlayout::place(1920,1080,1920,1080,200,1,0,0,plane,aspect);assert(upper.valid&&upper.destination.x==10&&upper.destination.y==10);
  auto full=radarlayout::place(1920,1080,1920,1080,200,3,100,300,plane);
  for(int ax=0;ax<3;++ax)for(int ay=0;ay<3;++ay){auto custom=radarlayout::place(1920,1080,1920,1080,200,3,100,300,plane,aspect,ax,ay);assert(custom.valid);
   assert(custom.destination.x==100+(full.destination.w-custom.destination.w)*ax/2);
   assert(custom.destination.y==300+(full.destination.h-custom.destination.h)*ay/2);}
 }
using namespace external::radar;Memory m;constexpr uint32_t cells=0x100000,cell=0x200000,house=0x300000;
 m.set(Map+0xF4,4);m.set(Map+0xF8,4);m.set(Map+0x13C,cells);m.set(Map+0x140,4096);m.set(Map+0x1488,10.f);m.set(Map+0x1490,4);m.set(Map+0x1498,4);m.set(Map+0x14A4,80);m.set(Map+0x14A8,80);
 m.set(Map+0xFC,0);m.set(Map+0x100,0);m.set(Map+0x104,4);m.set(Map+0x108,4);
 m.set(0xA83D4C,house);m.set(0xA8ED84,10);m.set<uint8_t>(0xA8E9A0,1);m.set<uint8_t>(0xA8E378,1);m.set(0xA8EDA0,0);m.set(0xA8ED8C,0);
 m.zero(cells,4096*4);m.set(cells+(3*512+3)*4,cell);m.zero(cell,0x148);m.set<int16_t>(cell+0x24,3);m.set<int16_t>(cell+0x26,3);m.set(cell+0xEC,2);m.set<int8_t>(cell+0x120,-1);m.set<int8_t>(cell+0x121,-1);
 constexpr uint32_t actors=0x410000,actor=0x420000,type=0x430000;
 m.zero(0xA8EC78,20);m.set(0xA8EC78+4,actors);m.set(0xA8EC78+8,1);m.set(0xA8EC78+16,1);m.set(actors,actor);
 m.zero(actor,0x800);m.set(actor,0x7E3EBCu);m.set<int>(actor+0x6C,100);m.set<uint8_t>(actor+0x74,1);m.set<uint8_t>(actor+0x90,1);
 m.set(actor+0x21C,house);m.set(actor+0x520,type);m.set(actor+0x9C,3*256+128);m.set(actor+0xA0,3*256+128);m.set(actor+0x10,7);
 m.zero(house+22265,3);m.set<uint8_t>(house+22265,200);
 Sampler<Read> sampler(Read{&m});assert(sampler.step(false,2048));assert(!sampler.snapshot(1).terrain);assert(sampler.step(false,2048));auto a=sampler.snapshot(2);assert(a.terrain&&a.terrain->colors[3*512+3]==terrainColor(2,0,false));
 assert(a.markerScanValid&&a.markers.size()==1);
 m.bytes.erase(actors);auto failedBodyRead=sampler.snapshot(3);assert(!failedBodyRead.markerScanValid&&failedBodyRead.markers.empty());
 m.set(actors,actor);m.bytes.erase(0xA8EC78+16);auto failedHeaderRead=sampler.snapshot(4);assert(!failedHeaderRead.markerScanValid&&failedHeaderRead.markers.empty());
 m.set(0xA8EC78+16,0);auto empty=sampler.snapshot(5);assert(empty.markerScanValid&&empty.markers.empty());
 double u,v;a.terrain->geo.project(3.5,3.5,u,v);assert(u==.5&&v==.375);std::vector<uint32_t> pixels;raster(*a.terrain,80,80,pixels);assert(pixels[30*80+40]==terrainColor(2,0,false));
 DiagramRenderer diagram;auto scaled=diagram.render(a.terrain,80,80);assert(scaled&&scaled->pixels==pixels);
 for(int i=0;i<100;++i)assert(diagram.render(a.terrain,80,80)==scaled);
 auto larger=diagram.render(a.terrain,1600,800);assert(larger!=scaled&&larger->pixels.size()==1600*800);
 auto changedTerrain=std::make_shared<Terrain>(*a.terrain);changedTerrain->colors[3*512+3]=0;
 auto changedDiagram=diagram.render(changedTerrain,80,80);assert(changedDiagram!=scaled&&changedDiagram->pixels!=scaled->pixels);
 assert(scaled->pixels==pixels);diagram.reset();assert(diagram.render(a.terrain,80,80)!=scaled);
 assert(!diagram.render(a.terrain,0,80)&&!diagram.render(a.terrain,2049,80));
 PublicationContext context{house,cells,a.frame+1,true,a.terrain->geo.bounds};assert(freshMarkers(a,context));
 auto published=publicationContext(Read{&m});assert(published.readable&&sameWorld(a,published));
 m.bytes.erase(0xA8ED84);published=publicationContext(Read{&m});assert(!published.readable&&!sameWorld(a,published));m.set(0xA8ED84,10);
 context.frame=a.frame+120;assert(sameWorld(a,context)&&!freshMarkers(a,context)); // slow rendering is not a map change
 context.frame=a.frame-1;assert(!sameWorld(a,context));context.frame=a.frame+120;
 context.house+=0x10000;assert(!sameWorld(a,context));context.house=house;context.cells+=4;assert(!sameWorld(a,context));context.cells=cells;
 context.active=false;assert(!sameWorld(a,context));
 int nx=0,ny=0;assert(navigate(a.terrain->geo,41,31,82,82,120,80,nx,ny));assert(nx==0&&ny==105);
 assert(navigate(a.terrain->geo,81,61,162,162,120,80,nx,ny)&&nx==0&&ny==105);
 assert(navigate(a.terrain->geo,-50,999,82,82,120,80,nx,ny)&&nx==-60&&ny==180);
 assert(!navigate(a.terrain->geo,1,1,2,82,120,80,nx,ny));
 // Interpolate known cells without bleeding visible colors into black shroud.
 Terrain mix=*a.terrain;mix.colors[3*512+3]=0x010101;mix.colors[3*512+4]=0xffffff;
 assert(smoothColor(mix,3.5,3.5)==0x010101);assert(smoothColor(mix,4.,3.5)==0x808080);
 mix.colors[3*512+4]=0;assert(smoothColor(mix,4.,3.5)==0x080c08);assert(smoothColor(mix,3.9,3.5)==0x010101);
 // Never expose a partially refreshed terrain bank; old immutable bank survives.
 m.set<int8_t>(cell+0x120,-2);sampler.step(false,2048);assert(sampler.snapshot(3).terrain==a.terrain);sampler.step(false,2048);assert(sampler.snapshot(4).terrain->colors[3*512+3]==0);assert(a.terrain->colors[3*512+3]!=0);
 sampler.step(true,4096);assert(sampler.snapshot(5).terrain->colors[3*512+3]!=0);
 // A half-transparent main viewport means an unmasked minimap, exactly like
 // transparent/direct display. No extra darkening belongs in its terrain bank.
 for(uint32_t reveal:{1u,2u,3u}){
  assert(sampler.step(reveal,4096,true));auto revealed=sampler.snapshot(5);
  assert(revealed.terrain&&revealed.terrain->colors[3*512+3]==terrainColor(2,0,false));
  assert(revealed.terrain->art[3*512+3].cell==cell);
 }
 m.set<int8_t>(cell+0x120,-1);m.set<int8_t>(cell+0x121,0);
 assert(sampler.step(0,4096));assert(sampler.snapshot(5).terrain->colors[3*512+3]==terrainColor(2,0,true));
 assert(sampler.step(2,4096));assert(sampler.snapshot(5).terrain->colors[3*512+3]==terrainColor(2,0,false));
 m.set<int8_t>(cell+0x120,-2);m.set<int8_t>(cell+0x121,-1);
 assert(sampler.step(0,4096));assert(sampler.snapshot(5).terrain->colors[3*512+3]==0);
 assert(sampler.step(1,4096));
 // Trigger-driven camera-domain change needs no new cell array/house/tick.
 // A legal border tile remains drawable even when it becomes non-usable.
 auto beforeCrop=sampler.snapshot(5);m.set(Map+0x100,1);m.set(Map+0x108,3);
 assert(!sameWorld(beforeCrop,publicationContext(Read{&m})));
 sampler.step(true,2048);assert(!sampler.snapshot(5).terrain);sampler.step(true,2048);
 auto cropped=sampler.snapshot(5);assert(cropped.terrain&&cropped.terrain->colors[3*512+3]!=0&&cropped.terrain->geo.bounds.top==1);
 // The native radar also updates projection after SetVisibleRect. Accept its
 // new offset/size, keep physical border cells and discard the old raster.
 m.set(Map+0x1498,6);m.set(Map+0x14A8,60);sampler.step(true,2048);assert(!sampler.snapshot(5).terrain);sampler.step(true,2048);
 auto shifted=sampler.snapshot(5);assert(shifted.terrain&&shifted.terrain->colors[3*512+3]!=0);
 shifted.terrain->geo.project(3.5,3.5,u,v);assert(u==.5&&v==1./6);
 auto shiftedDiagram=diagram.render(shifted.terrain,80,60);assert(shiftedDiagram&&shiftedDiagram->pixels[10*80+40]==terrainColor(2,0,false));
 m.set(Map+0x100,0);m.set(Map+0x108,4);m.set(Map+0x1498,4);m.set(Map+0x14A8,80);sampler.step(true,4096);
 auto restored=sampler.snapshot(5);assert(restored.terrain->colors[3*512+3]!=0);restored.terrain->geo.project(3.5,3.5,u,v);assert(u==.5&&v==.375);
 m.set(0xA8ED84,1);sampler.step(true,1);assert(!sampler.snapshot(6).terrain);sampler.step(true,4096);assert(sampler.snapshot(7).terrain);
 m.set(0xA83D4C,house+0x10000);sampler.step(true,1);assert(!sampler.snapshot(8).terrain);m.set(0xA8ED8C,1);assert(!sampler.step(true));assert(!sampler.snapshot(9).terrain);m.set(0xA8ED8C,0);
 m.set(Map+0x1488,std::numeric_limits<float>::quiet_NaN());assert(!sampler.step(true));m.set(Map+0x1488,10.f);m.set(Map+0x140,999999);assert(!sampler.step(true));
 raster(*a.terrain,-1,80,pixels);assert(pixels.empty());int count=0;
 for(int gw:{800,1920,2560,3840})for(int gh:{600,1080,1440,2160})for(int cw:{640,1920,3840})for(int ch:{480,1080,2160})for(int scale:{10,50,100,200})for(int mode:{0,1,2,3})for(int coord:{0,300,32768})for(bool plane:{false,true}){
  auto aircraft=plane?radarlayout::aircraft(gw,gh,cw,ch):radarlayout::Box{};auto layout=radarlayout::place(gw,gh,cw,ch,scale,mode,coord,coord,aircraft);assert(layout.valid);auto b=layout.destination;
  assert(b.x>=0&&b.y>=0&&b.x+b.w<=cw&&b.y+b.h<=ch);assert(!radarlayout::overlap(b,aircraft)&&!radarlayout::overlap(b,layout.source));++count;
 }
 std::cout<<"PASS: independent map projection/raster, atomic terrain banks, shroud/reveal, new round/house/menu resets, invalid geometry; "<<count<<" layout combinations\n";
}

