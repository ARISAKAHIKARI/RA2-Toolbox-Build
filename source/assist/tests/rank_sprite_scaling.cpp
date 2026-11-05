#ifdef NDEBUG
#error Assertions required
#endif
#define NOMINMAX
#include <windows.h>
#include <cassert>
#include <cstdio>
#include "../../external_reader/source/local_ui_task.h"
#include "../../external_reader/source/render_profile.h"
#include "../../shared/source/panel_glyphs.h"
#include RANK_BASELINE_CLASSES
#include RANK_BASELINE_PANEL
#include RANK_BASELINE_CPU
using namespace external;
static std::vector<uint8_t>memory(0x1000000);static unsigned checks=0;
template<class T>static void put(uint32_t p,const T&v){assert(size_t(p)+sizeof v<=memory.size());std::memcpy(memory.data()+p,&v,sizeof v);}
static void check(bool v){++checks;assert(v);}
struct Read {bool operator()(uint32_t p,void*out,size_t n)const{if(uint64_t(p)+n>memory.size())return false;std::memcpy(out,memory.data()+p,n);return true;}};
constexpr uint32_t Shape=0x200000,Convert=0x300000,Palette=0x310000,Actor=0x400000,Type=0x500000,House=0x600000;
static void setup(){
 put(0xAC147C,Shape);put(Shape+2,uint16_t(5));put(Shape+4,uint16_t(5));put(Shape+6,uint16_t(20));
 const uint8_t star[25]={0,0,1,0,0,0,1,1,1,0,1,1,2,1,1,0,1,0,1,0,1,0,0,0,1};
 for(int frame:{14,15,19}){const uint32_t at=Shape+8+frame*24,offset=512+frame*25;put(at,uint16_t(2));put(at+2,uint16_t(2));put(at+4,uint16_t(5));put(at+6,uint16_t(5));put(at+20,uint32_t(offset));for(unsigned n=0;n<25;++n)put(Shape+offset+n,uint8_t(frame==15&&star[n]?2:star[n]));}
 put(0x87F6C4,Convert);put(Convert,uint32_t(0x7E5358));put(Convert+4,uint32_t(2));put(Convert+0x16C,1);put(Convert+0x174,Palette);put(Palette+2,uint16_t(0xffff));put(Palette+4,uint16_t(0xffe0));put(0x8A0DD0,std::array<int,6>{{11,3,0,3,5,2}});
 put(Actor,uint32_t(0x7F5C70));put(Actor+0x10,uint32_t(7));put(Actor+0x6C,100);put(Actor+0x74,uint8_t(1));put(Actor+0x90,uint8_t(1));put(Actor+0x6C4,Type);put(Actor+0x21C,House);put(Actor+0x9C,std::array<int,3>{{12*256,12*256,0}});put(0xB0CD48,.5);
}
static std::vector<uint32_t> pixels(const localui::Plan&plan,int w,int h){nativefeedback::Canvas canvas;check(canvas.reset(w,h));for(const auto&lane:plan.layers)for(const auto&p:lane)canvas.fill(p.rect,p.color);return canvas.pixels;}
template<class Engine,class Painter>static std::vector<uint32_t>draw(Engine&engine,int w,int h,float veterancy,bool rank=true,int x=35,int y=30){
 localui::Plan plan;Painter painter(plan,100,100,w,h);Read read;put(Actor+0x150,veterancy);
 if(rank){mainview::View view;view.house=House;view.cameraX=-25;view.cameraY=330;view.width=view.height=100;std::vector<nativefeedback::Object>objects{{Actor,7,Type,House,{},2}};bool font=false;check(engine.statuses(read,view,objects,painter,font));check(!font);}
 else check(engine.icon(read,painter,Shape,14,x,y));
 check(painter.good());return pixels(plan,w,h);
}
int main(){setup();Read read;localui::BeforeEngine old;localui::InspectedEngine current;
 unsigned changed=0;
 for(float rank:{-1.f,1.f,2.f})for(auto size:std::vector<std::pair<int,int>>{{75,75},{100,100},{125,125},{150,150},{200,200},{300,300},{400,400},{200,150}}){
  auto a=draw<localui::BeforeEngine,localui::BeforePainter>(old,size.first,size.second,rank),b=draw<localui::InspectedEngine,localui::Painter>(current,size.first,size.second,rank);
  if(size.first<=100&&size.second<=100)check(a==b);else{check(a!=b);++changed;}
  // Other native UI sprites still use their old nearest path.
  check((draw<localui::BeforeEngine,localui::BeforePainter>(old,size.first,size.second,rank,false)==draw<localui::InspectedEngine,localui::Painter>(current,size.first,size.second,rank,false)));
 }
 check((draw<localui::BeforeEngine,localui::BeforePainter>(old,200,200,0.f)==draw<localui::InspectedEngine,localui::Painter>(current,200,200,0.f)));
 // The recovery profile stays scale=1, while actual enlarged rank geometry
 // independently selects the bounded tiny-sprite reconstruction path.
 presentation::Profile recovery;check(recovery.scale==1);auto a=draw<localui::BeforeEngine,localui::BeforePainter>(old,200,200,1.f),b=draw<localui::InspectedEngine,localui::Painter>(current,200,200,1.f);check(a!=b);
 struct Request {presentation::Profile profile;};Request r1,r2;check(localui::sameProfile(r1,r2,0));r2.profile.scale=3;check(!localui::sameProfile(r1,r2,0));r2=r1;r2.profile.gpu=true;check(!localui::sameProfile(r1,r2,0));struct Legacy {};check(localui::sameProfile(Legacy{},Legacy{},0));
 liveshape::Cache shapes;shapes.begin();auto image=shapes.get(read,Shape,14,0x87F6C4);check(bool(image));auto source=std::shared_ptr<const std::vector<uint32_t>>(image,&image->pixels);
 screensprites::Cache cache;check(cache.get(source,5,5,5,5).pixels==source);check(cache.get(source,5,5,10,10,false).pixels==source);check(cache.get(source,5,5,3,3).pixels==source);
 auto one=cache.get(source,5,5,10,10);check(one.width==10&&one.height==10&&one.pixels!=source&&cache.builds()==1);check(cache.get(source,5,5,10,10).pixels==one.pixels&&cache.hits()==1);check(cache.get(source,5,5,15,15).pixels!=one.pixels&&cache.builds()==2);
 for(auto p:*one.pixels)for(int s:{0,8,16})check(((p>>s)&255)<=(p>>24));
 // Changed live palette yields a new immutable source owner and new raster.
 put(Palette+2,uint16_t(0xf800));shapes.begin();auto newImage=shapes.get(read,Shape,14,0x87F6C4);check(newImage&&newImage!=image);auto newSource=std::shared_ptr<const std::vector<uint32_t>>(newImage,&newImage->pixels);check(cache.get(newSource,5,5,10,10).pixels!=one.pixels);
 auto ranks=std::make_shared<panelglyphs::Annotations>();ranks->worldWidth=60;ranks->worldHeight=60;ranks->glyphs={{.25,.3,-1,-2,5,5,source},{.99,.99,0,0,5,5,source}};
 auto prior=std::make_shared<before_panelglyphs::Annotations>();prior->worldWidth=60;prior->worldHeight=60;prior->glyphs={{.25,.3,-1,-2,5,5,source},{.99,.99,0,0,5,5,source}};
 for(auto size:std::vector<std::pair<int,int>>{{32,32},{62,62},{92,92},{122,122},{182,182},{242,242},{122,92}}){
  auto previous=before_panelglyphs::place(prior,size.first,size.second);auto next=panelglyphs::place(ranks,size.first,size.second);check(previous.size()==next.size());
  for(size_t n=0;n<next.size();++n){check(previous[n].x==next[n].x&&previous[n].y==next[n].y&&previous[n].width==next[n].width&&previous[n].height==next[n].height);if(next[n].width>5||next[n].height>5)check(next[n].pixels!=source&&next[n].sourceWidth==next[n].width&&next[n].sourceHeight==next[n].height);else check(next[n].pixels==source);}
  std::vector<uint32_t> x(size.first*size.second),y=x;before_panelglyphs::paint(x.data(),size.first,size.second,previous);panelglyphs::paint(y.data(),size.first,size.second,next);if(size.first<=62&&size.second<=62)check(x==y);else check(x!=y);
 }
 localui::Plan clipped;localui::Painter p(clipped,100,100,150,150);p.rank(image,-5,-5);check(p.good());for(const auto&lane:clipped.layers)for(const auto&item:lane)check(item.rect.x>=0&&item.rect.y>=0&&item.rect.x+item.rect.w<=150&&item.rect.y+item.rect.h<=150);
 // The common pure CPU implementation is an exact extraction of the ordinary
 // SHP implementation, including partial alpha, 2/3/4x and fractional resolve.
 std::vector<uint32_t>mixed(35);uint32_t seed=42;for(auto&pixel:mixed){seed=seed*1664525+1013904223;pixel=spritecpu::premultiplied(seed);}
 for(int factor:{2,3,4}){spritecpu::ScaledImage now;before_spritecpu::ScaledImage before;check(spritecpu::scale(mixed,7,5,factor,now)&&before_spritecpu::scale(mixed,7,5,factor,before));check(now.pixels==before.pixels&&now.width==before.width&&now.height==before.height);for(int y=0;y<23;++y)for(int x=0;x<31;++x)check(spritecpu::sample(now,(x+.5)*7/31,(y+.5)*5/23)==before_spritecpu::sample(before,(x+.5)*7/31,(y+.5)*5/23));}
 for(double scale:{.5,1.,1.1,2.,2.1,3.,3.1,4.,4.1})check(spritecpu::factor(scale,scale)==before_spritecpu::factor(scale,scale));
 // Oversized but valid material remains drawable through the old native
 // fallback. An uncached result must not evict unrelated warm entries.
 auto oversized=std::make_shared<const std::vector<uint32_t>>(1025,0xffffffff);check(cache.get(oversized,1025,1,2050,2).pixels==oversized);check(cache.get(source,5,5,25,25).pixels==source);
 auto big=std::make_shared<const std::vector<uint32_t>>(256*256,0xff808080);auto warm=cache.get(source,5,5,10,10);auto uncached=cache.get(big,256,256,1024,1024);check(uncached.width==1024&&uncached.pixels!=big);check(cache.get(source,5,5,10,10).pixels==warm.pixels&&cache.bytes()<=4*1024*1024);
 for(int n=1;n<145;++n)cache.get(source,5,5,n%16+6,n/16+6);check(cache.bytes()<=4*1024*1024);
 std::printf("PASS %u rank scaling assertions; %u enlarged main rank cases reject pinned old block-pixel output; 1x/reduction and other UI exact; automatic local reconstruction with recovery scale=1; profile invalidation; immutable palette/cache/alpha; minimap unchanged anchors and physical minimum; ordinary SHP CPU extraction exact; oversized/budget fallback. No game execution.\n",checks,changed);
}
