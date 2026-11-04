#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>
#include "native_effect_math.h"
// Passive observations and bounded PARGB rasterization. No Win32/game calls,
// no writes through the reader, and no game RNG or mission/selection setters.
namespace external { namespace nativefeedback {
constexpr size_t MaxBolts=8192; // snapshot validation bound, not a per-frame draw quota
constexpr uint32_t MousePointer=0x887640u, Map=0x87F7E8u, Bolts=0x8A0E88u;
inline bool range(uint32_t p,size_t n){return p>=0x10000u&&n<=8*1024*1024&&uint64_t(p)+n<=0x100000000ull;}
struct Rect {int x=0,y=0,w=0,h=0;bool valid()const{return w>0&&h>0;}};
inline Rect intersect(Rect a,Rect b){
 int64_t x=std::max<int64_t>(a.x,b.x),y=std::max<int64_t>(a.y,b.y);
 int64_t r=std::min(int64_t(a.x)+a.w,int64_t(b.x)+b.w),d=std::min(int64_t(a.y)+a.h,int64_t(b.y)+b.h);
 return r>x&&d>y?Rect{int(x),int(y),int(r-x),int(d-y)}:Rect{};
}
struct Band {bool active=false;std::array<int32_t,4> edges{};};
template<class Read>bool band(Read read,uint32_t tactical,Band&out){
 out={};if(!range(tactical,0xDA0))return false;
 uint8_t active=0,again=0;std::array<int32_t,4>a{},b{};
 // YRpp DisplayClass::LeftPressAndDraggingRectangle; TacticalClass::Band.
 if(!read(Map+0x11CF,&active,1)||active>1)return false;
 if(!active)return true;
 if(!read(tactical+0xD90,a.data(),sizeof a)||!read(tactical+0xD90,b.data(),sizeof b)||
    !read(Map+0x11CF,&again,1)||active!=again||a!=b)return false;
 for(int32_t v:a)if(v<-32768||v>32768)return false;
 out={true,a};return true;
}
// Band coordinates are tactical-view local; they are NOT world coordinates.
inline std::vector<Rect> bandStrips(const Band&b,Rect viewport,double sx,double sy){
 std::vector<Rect> out;if(!b.active||!viewport.valid()||!std::isfinite(sx)||!std::isfinite(sy)||sx<=0||sy<=0||sx>16||sy>16)return out;
 for(auto v:b.edges)if(v<-32768||v>32768)return out;
 int l=int(std::floor(std::min(b.edges[0],b.edges[2])*sx))+viewport.x;
 int t=int(std::floor(std::min(b.edges[1],b.edges[3])*sy))+viewport.y;
 int r=int(std::floor(std::max(b.edges[0],b.edges[2])*sx))+viewport.x;
 int d=int(std::floor(std::max(b.edges[1],b.edges[3])*sy))+viewport.y;
 int tx=std::max(1,int(std::lround(sx))),ty=std::max(1,int(std::lround(sy)));
 for(Rect q:std::array<Rect,4>{{{l,t,r-l+tx,ty},{l,d,r-l+tx,ty},{l,t,tx,d-t+ty},{r,t,tx,d-t+ty}}}){
  q=intersect(q,viewport);if(q.valid())out.push_back(q);
 }return out;
}
struct Mouse {
 uint32_t pointer=0,shape=0,resolved=0,converter=0;int32_t frame=-1,refCount=-1,hotX=0,hotY=0;
 int width=0,height=0,frames=0;std::array<uint8_t,24> frameHeader{};
 bool same(const Mouse&o)const{return pointer==o.pointer&&shape==o.shape&&resolved==o.resolved&&converter==o.converter&&frame==o.frame&&
  refCount==o.refCount&&hotX==o.hotX&&hotY==o.hotY&&width==o.width&&height==o.height&&frames==o.frames&&frameHeader==o.frameHeader;}
};
inline uint16_t u16(const uint8_t*p){return uint16_t(p[0])|(uint16_t(p[1])<<8);}
inline uint32_t u32(const uint8_t*p){uint32_t v=0;std::memcpy(&v,p,4);return v;}
template<class Read>bool mouseOnce(Read read,Mouse&out){
 Mouse v;uint32_t vt=0;std::array<uint32_t,5> funcs{};std::array<uint8_t,0x44> state{};uint8_t head[8]{};
 if(!read(MousePointer,&v.pointer,4)||!range(v.pointer,state.size())||!read(v.pointer,state.data(),state.size()))return false;
 vt=u32(state.data());if(!range(vt,sizeof funcs)||!read(vt,funcs.data(),sizeof funcs)||funcs[1]!=0x7B8A00||funcs[3]!=0x7B9930||funcs[4]!=0x7B9750)return false;
 v.shape=u32(state.data()+4);v.frame=int32_t(u32(state.data()+8));v.refCount=int32_t(u32(state.data()+12));
 v.hotX=int32_t(u32(state.data()+0x3C));v.hotY=int32_t(u32(state.data()+0x40));v.resolved=v.shape;
 if(!range(v.resolved,16)||!read(v.resolved,head,sizeof head))return false;
 if(u16(head)==0xFFFF){if(!read(v.shape+12,&v.resolved,4)||!range(v.resolved,8)||!read(v.resolved,head,sizeof head))return false;}
 v.width=u16(head+2);v.height=u16(head+4);v.frames=u16(head+6);
 if(u16(head)||v.width<1||v.height<1||v.width>512||v.height>512||v.frames<1||v.frames>8192||v.frame<0||v.frame>=v.frames||
    v.hotX<0||v.hotY<0||v.hotX>=v.width||v.hotY>=v.height||!range(v.resolved,8+size_t(v.frames)*24)||
    !read(v.resolved+8+uint32_t(v.frame)*24,v.frameHeader.data(),v.frameHeader.size())||
    !read(0x87F6C8,&v.converter,4)||!range(v.converter,0x188))return false;
 if(u16(v.frameHeader.data()+4)>512||u16(v.frameHeader.data()+6)>512)return false;
 out=v;return true;
}
template<class Read>bool mouse(Read read,Mouse&out){
 Mouse a,b;out={};if(!mouseOnce(read,a)||!mouseOnce(read,b)||!a.same(b))return false;out=a;return true;
}
template<class Read>bool mousePalette(Read read,uint32_t converter,std::array<uint32_t,256>&out){
 if(!range(converter,0x188))return false;
 uint32_t vt=0,bpp=0,row=0,again=0;int shades=0;std::array<int,6>s{},s2{};std::array<uint16_t,256> colors{};
 if(!read(converter,&vt,4)||vt!=0x7E5358||!read(converter+4,&bpp,4)||bpp!=2||
    !read(converter+0x16C,&shades,4)||shades!=1||!read(converter+0x174,&row,4)||!range(row,sizeof colors)||
    !read(0x8A0DD0,s.data(),sizeof s)||!read(row,colors.data(),sizeof colors)||
    !read(converter+0x174,&again,4)||again!=row||!read(0x8A0DD0,s2.data(),sizeof s2)||s!=s2)return false;
 if(s!=std::array<int,6>{{11,3,0,3,5,2}}&&s!=std::array<int,6>{{10,3,0,3,5,3}})return false;
 std::array<uint32_t,256> result{};
 for(unsigned i=1;i<256;++i){unsigned v=colors[i];unsigned r=((v>>s[0])<<s[1])&255,g=((v>>s[4])<<s[5])&255,b=((v>>s[2])<<s[3])&255;result[i]=0xff000000u|(r<<16)|(g<<8)|b;}
 out=result;return true;
}
struct Point3 {int32_t x=0,y=0,z=0;};
inline bool worldPoint(Point3 p){return p.x>=0&&p.y>=0&&p.x<512*256&&p.y<512*256&&p.z>=-16384&&p.z<=100000;}
struct Bolt {Point3 source,target;uint32_t seed=0;bool alternate=false;int zAdjust=0;bool rgb555=false;std::array<uint32_t,3>colors{};std::shared_ptr<const nativefx::Math>math;};
// Public EBolt layout. A snapshot is diagnostic/visual only; Lifetime must
// never be shifted by this renderer, and Random is read rather than advanced.
template<class Read>bool bolts(Read read,std::vector<Bolt>&out){
 out.clear();std::array<uint32_t,6> v{},after{};
 if(!read(Bolts,v.data(),sizeof v)||v[2]>MaxBolts||v[4]>v[2]||(v[4]&&!range(v[1],size_t(v[4])*4)))return false;
 std::vector<uint32_t> ptr(v[4]);if(!ptr.empty()&&!read(v[1],ptr.data(),ptr.size()*4))return false;
 std::vector<Bolt> pending;
 auto math=nativefx::math(read);std::array<uint16_t,16>palette{};uint32_t converter=0,row=0,bpp=0;std::array<int,6>format{};
 if(!ptr.empty()&&(!math||!read(0x87F6C4,&converter,4)||!read(converter+4,&bpp,4)||bpp!=2||!read(converter+0x174,&row,4)||!read(row,palette.data(),sizeof palette)||!read(0x8A0DD0,format.data(),sizeof format)))return false;
 const bool f555=format==std::array<int,6>{{10,3,0,3,5,3}};
 if(!ptr.empty()&&!f555&&format!=std::array<int,6>{{11,3,0,3,5,2}})return false;
 for(uint32_t p:ptr){std::array<uint8_t,0x30>a{},b{};
  if(!range(p,a.size())||!read(p,a.data(),a.size())||!read(p,b.data(),b.size())||a!=b)return false;
  Bolt e;std::memcpy(&e.source,a.data(),12);std::memcpy(&e.target,a.data()+12,12);e.seed=u32(a.data()+0x1C);e.zAdjust=int32_t(u32(a.data()+0x18));e.math=math;e.rgb555=f555;for(unsigned n=0;n<3;++n)e.colors[n]=nativepacked::color(palette[n==0?10:n==1?5:15],0xff000000u,f555);e.alternate=a[0x2C]!=0;
  if(a[0x2C]>1||!worldPoint(e.source)||!worldPoint(e.target)||!u32(a.data()+0x28))continue;
  pending.push_back(e);
 }
 if(!read(Bolts,after.data(),sizeof after)||v!=after)return false;
 if(!ptr.empty()){std::vector<uint32_t> verify(ptr.size());if(!read(v[1],verify.data(),verify.size()*4)||ptr!=verify)return false;}
 if(!ptr.empty()){std::array<uint16_t,16>again{};uint32_t current=0,currentRow=0;std::array<int,6>verifyFormat{};if(!read(0x87F6C4,&current,4)||current!=converter||!read(converter+0x174,&currentRow,4)||currentRow!=row||!read(row,again.data(),sizeof again)||again!=palette||!read(0x8A0DD0,verifyFormat.data(),sizeof verifyFormat)||format!=verifyFormat)return false;}
 out=std::move(pending);return true;
}
inline uint32_t over(uint32_t s,uint32_t d){
 unsigned inv=255-(s>>24);uint32_t o=0;
 for(int sh:{0,8,16,24})o|=std::min(255u,((s>>sh)&255)+(((d>>sh)&255)*inv+127)/255)<<sh;
 return o;
}
struct Canvas {
 int width=0,height=0;std::vector<uint32_t> pixels;
 bool reset(int w,int h){width=height=0;pixels.clear();if(w<1||h<1||w>8192||h>8192||uint64_t(w)*h>16*1024*1024)return false;width=w;height=h;pixels.assign(size_t(w)*h,0);return true;}
 void dot(int x,int y,uint32_t color){if(x>=0&&y>=0&&x<width&&y<height){auto&d=pixels[size_t(y)*width+x];d=over(color,d);}}
 void fill(Rect r,uint32_t c){r=intersect(r,{0,0,width,height});for(int y=r.y;y<r.y+r.h;++y)for(int x=r.x;x<r.x+r.w;++x)dot(x,y,c);}
 // Clip before rasterization to bound work for offscreen beams.
 void line(double x0,double y0,double x1,double y1,uint32_t color,int thickness=1){
  if(!width||!height||!std::isfinite(x0)||!std::isfinite(y0)||!std::isfinite(x1)||!std::isfinite(y1)||std::abs(x0)>1e7||std::abs(y0)>1e7||std::abs(x1)>1e7||std::abs(y1)>1e7)return;
  double dx=x1-x0,dy=y1-y0,lo=0,hi=1;
  auto edge=[&](double p,double q){if(p==0)return q>=0;double t=q/p;if(p<0){if(t>hi)return false;lo=std::max(lo,t);}else{if(t<lo)return false;hi=std::min(hi,t);}return true;};
  if(!edge(-dx,x0)||!edge(dx,width-1-x0)||!edge(-dy,y0)||!edge(dy,height-1-y0))return;
  int ax=int(std::lround(x0+lo*dx)),ay=int(std::lround(y0+lo*dy)),bx=int(std::lround(x0+hi*dx)),by=int(std::lround(y0+hi*dy));
  int sx=ax<bx?1:-1,sy=ay<by?1:-1,ex=std::abs(bx-ax),ey=-std::abs(by-ay),err=ex+ey;
  int radius=(std::clamp(thickness,1,9)-1)/2;
  for(int n=0;n<=width+height+2;++n){fill({ax-radius,ay-radius,2*radius+1,2*radius+1},color);if(ax==bx&&ay==by)break;int e=2*err;if(e>=ey){err+=ey;ax+=sx;}if(e<=ex){err+=ex;ay+=sy;}}
 }
 void brackets(Rect r,uint32_t c){if(!r.valid())return;int k=std::clamp(std::min(r.w,r.h)/4,3,10);int l=r.x,t=r.y,rr=r.x+r.w-1,bb=r.y+r.h-1;
  for(int x:{l,rr})for(int y:{t,bb}){line(x,y,x+(x==l?k:-k),y,c);line(x,y,x,y+(y==t?k:-k),c);}}
};
inline uint32_t hash(uint32_t x){x^=x>>16;x*=0x7feb352du;x^=x>>15;x*=0x846ca68bu;return x^(x>>16);}
template<class Target>inline void bolt(Target&c,double x0,double y0,double x1,double y1,uint32_t seed,bool alternate,double scale){
 double length=std::hypot(x1-x0,y1-y0);if(!std::isfinite(length)||length<.5||length>100000||!std::isfinite(scale)||scale<=0||scale>16)return;
 int n=std::clamp(int(length/12),4,64);double nx=-(y1-y0)/length,ny=(x1-x0)/length;
 uint32_t edge=alternate?0xffb02058u:0xff4058e8u,core=alternate?0xffffd0e0u:0xffe0f8ffu;
 // Soft premultiplied glow is a presentation-only approximation. Do not
 // read/advance game RNG, EBolt lifetime, target position or visibility here.
 // Draw outside-to-inside so shroud clipping still applies to the final layer.
 uint32_t glow=alternate?0x28220917u:0x280a1425u;
 const int haloWidth=std::clamp(int(std::lround(7*scale)),1,9);
 for(int strand=0;strand<2;++strand){double ax=x0,ay=y0;
  for(int i=1;i<=n;++i){double t=double(i)/n,jitter=i==n?0:(int(hash(seed^uint32_t(i*131+strand*7919))&65535)-32768)/32768.*std::min(6.*scale,length/10.);
   double bx=x0+(x1-x0)*t+nx*jitter,by=y0+(y1-y0)*t+ny*jitter;
   c.line(ax,ay,bx,by,glow,haloWidth);
   c.line(ax,ay,bx,by,edge,std::clamp(int(std::lround(3*scale)),1,9));
   c.line(ax,ay,bx,by,core);ax=bx;ay=by;
  }
 }
}
}}
