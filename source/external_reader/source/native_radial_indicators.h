#pragma once
#include "ground_geometry.h"
#include "native_ui_font.h"
#include "native_effect_math.h"
#include "main_viewport.h"
namespace external { namespace radialindicators {
// GPL-3.0: Vera20K3d42426 presentation/radial_indicators.rs and
// render/radial_indicator.rs. Read native range/pulse owners; never advance
// the engine's draw clock or call its drawing methods from an external thread.
struct Draw {nativefeedback::Point3 point;float radius=0;uint32_t color=0;bool pixels=false,truncate=true,fade=true,rays=true;};
inline uint32_t interpolate(uint32_t rgb,uint32_t to,unsigned factor){uint32_t out=0;for(int shift:{0,8,16}){int a=(rgb>>shift)&255,b=(to>>shift)&255;out|=uint32_t(std::clamp((a*int(256-factor)+b*int(factor))>>8,0,255))<<shift;}return out;}
inline int radiusPixels(float radius,bool truncate=true){if(!std::isfinite(radius)||radius<0||radius>128)return -1;double r=truncate?std::trunc(radius):radius;return int((r+.5)/double(float(std::sqrt(2.)))*60.);}
template<class Read>int buildingRange(GameView<Read>&g,uint32_t p,uint32_t type){
 int psychic=g.template get<int>(type+0x170C);if(psychic>0)return psychic;
 if(g.template get<uint8_t>(type+0xCD1))return g.template get<int8_t>(type+(g.template get<uint8_t>(p+0x268)?0xCD3:0xCD2));
 if(g.template get<uint8_t>(type+0x16C8)||g.template get<uint8_t>(type+0x16C7))return g.template get<int8_t>(type+0x1707);
 int slot=g.template get<int>(type+0x808)>0?g.template get<int>(p+0x138,-1):0;if(slot<0||slot>=18)return 0;
 uint32_t weapon=g.template get<uint32_t>(type+0x898+slot*28);if(g.template get<float>(p+0x150)>=2){uint32_t elite=g.template get<uint32_t>(type+0xA94+slot*28);if(elite)weapon=elite;}
 int range=g.template get<int>(weapon+0xB4);return range>0&&range<65536?range/256:0;
}
template<class Read,class Objects>bool capture(Read read,const mainview::View&view,const Objects&objects,uint32_t tick,std::vector<Draw>&out){
 GameView g(read);if(g.template get<int>(0xA8ED84,-1)!=view.frame)return false;std::vector<uint32_t>selected,after;if(!g.vecChecked(0xA8ECB8,selected,8192))return false;std::map<uint32_t,const typename Objects::value_type*>visible;for(const auto&o:objects)visible[o.pointer]=&o;
 auto color=[&](uint32_t owner,uint32_t&rgb){uint8_t channels[3]{};if(!read(owner+0x56F9,channels,3))return false;rgb=uint32_t(channels[0])<<16|uint32_t(channels[1])<<8|channels[2];return true;};
 for(auto p:selected){auto found=visible.find(p);if(found==visible.end()||found->second->kind!=3)continue;const auto&o=*found->second;if(g.kind(p)!=3||g.template get<uint32_t>(p+0x10)!=o.id||g.type(p,3)!=o.type||!g.live(p)||!g.template get<uint8_t>(o.type+0x238)||!g.template get<uint8_t>(p+0x660))continue;
  bool controlled=o.owner==view.house||(g.template get<int>(0xA8B238,-1)==0&&(g.template get<uint8_t>(o.owner+0x1EC)||g.template get<uint8_t>(o.owner+0x1ED)));if(!controlled)continue;
  int radius=buildingRange(g,p,o.type);if(radius<=0||radius>128)continue;Draw draw;draw.radius=float(radius);if(!ground::target(read,p,draw.point)||!color(o.owner,draw.color))return false;
  if(!g.template get<uint8_t>(o.type+0x1708)){out.push_back(draw);continue;}
  unsigned phase=(tick>>1)&1023;draw.rays=false;draw.fade=false;if(!(phase&512)){auto outer=draw;if(!(phase&256))outer.color=interpolate(draw.color,0xFFFFFF,(~phase)&255);out.push_back(outer);}
  int pixels=radiusPixels(draw.radius);int expanding=pixels<0?0:int((unsigned(pixels)*phase)>>10);if(expanding>32){draw.radius=float(expanding);draw.pixels=true;out.push_back(draw);}
 }
 // Armed SW is an index into SuperWeaponTypeClass::Array, not SuperClass.
 // CurrentFoundationCenter supplies the existing cursor cell. Native global
 // phase/direction freeze when no positive-range superweapon is selected.
 if(g.template get<uint8_t>(0xA8EB7E)&&!g.template get<uint32_t>(0x88098C)){
  int index=g.template get<int>(0x8809A0,-1);std::vector<uint32_t>types;if(index>=0&&g.vecChecked(0xA8E330,types,8192)&&size_t(index)<types.size()){
   uint32_t type=types[size_t(index)];float radius=0;int multiplier=0,phase=0,direction=0;int16_t cell[2]{};
   if(!read(type+0xF8,&radius,4)||!read(type+0xFC,&multiplier,4)||!read(0xB0E64C,&phase,4)||!read(0x842954,&direction,4)||!read(0x88095C,cell,4))return false;
   if(std::isfinite(radius)&&radius>0&&radius<=128&&phase>=0&&phase<=100&&(direction==1||direction==-1)&&cell[0]>=0&&cell[1]>=0){
    nativefeedback::Point3 point{cell[0]*256+128,cell[1]*256+128,0};uint32_t source=0;int height=0;if(ground::at(read,point,height,source,false)){Draw draw;draw.point={point.x-126,point.y-126,height};draw.radius=radius;draw.rays=false;if(!color(view.house,draw.color))return false;
     if(multiplier<=0){draw.color=interpolate(draw.color,0,unsigned(phase));out.push_back(draw);}
     else if(multiplier<=64&&double(multiplier)*radius<64){draw.truncate=false;for(int n=0;n<=double(multiplier)*radius;++n){phase+=10*direction;if(phase>=100)direction=-1;else if(phase<=0)direction=1;auto ring=draw;ring.radius=float(float(n)/multiplier);ring.color=interpolate(draw.color,0,unsigned(std::clamp(phase,0,100)));out.push_back(ring);}}
    }
   }
  }
 }
 return g.template get<int>(0xA8ED84,-1)==view.frame&&g.vecChecked(0xA8ECB8,after,8192)&&after==selected&&out.size()<=256;
}
// Original7BB350: independent integer midpoint walks; explicit DWORD wrap.
// GPL-3.0 adaptation of Vera20K surface_line.rs. UI outlines ignore world Z/A.
template<class Emit>void nativeEllipse(int cx,int cy,int rx,int ry,int width,int height,Emit emit){
 if(rx<0||ry<0||rx>8192||ry>8192)return;
 auto add=[](int a,int b){return int32_t(uint32_t(a)+uint32_t(b));};
 auto mul=[](int a,int b){return int32_t(uint32_t(a)*uint32_t(b));};
 auto pixel=[&](int x,int y){if(x>=0&&y>=0&&x<width&&y<height)emit(nativefx::LinePixel{x,y,0,0});};
 auto quartet=[&](int x,int y){pixel(cx+x,cy+y);pixel(cx-x,cy-y);pixel(cx+x,cy-y);pixel(cx-x,cy+y);};
 const int xx=mul(rx,rx),yy=mul(ry,ry);pixel(cx,cy+ry);pixel(cx,cy-ry);
 int x=0,y=ry,dx=0,dy=mul(mul(xx,ry),2),decision=add(xx/4,-mul(xx,ry));
 for(int n=0;n<=rx;++n){decision=add(add(decision,dx),yy);if(decision>=0){dy=add(dy,-mul(xx,2));decision=add(decision,-dy);--y;}dx=add(dx,mul(yy,2));++x;if(dx>=dy)break;quartet(x,y);}
 pixel(cx+rx,cy);pixel(cx-rx,cy);x=rx;y=0;dx=mul(mul(yy,rx),2);dy=0;decision=add(yy/4,-mul(yy,rx));
 for(int n=0;n<=ry;++n){decision=add(add(decision,dy),xx);if(decision>=0){dx=add(dx,-mul(yy,2));decision=add(decision,-dx);--x;}dy=add(dy,mul(xx,2));++y;if(dy>dx)break;quartet(x,y);}
}
template<class Sink>void geometry(const std::vector<Draw>&draws,const mainview::View&view,double zscale,const nativefx::Math&math,Sink&sink,int width,int height,bool format){
 const double sx=double(width)/view.width,sy=double(height)/view.height;
 for(const auto&d:draws){const int rx=d.pixels?nativefx::chop(d.radius):nativefx::chop(((d.truncate?std::trunc(d.radius):double(d.radius))+.5)/math.rootWide(2.)*60.),ry=rx/2;
  if(rx<0||rx>8192)continue;const int cx=int((int64_t(d.point.x)-d.point.y)*30/256)-view.cameraX,cy=int((int64_t(d.point.x)+d.point.y)*15/256)-screenHeight(d.point.z,zscale)-view.cameraY;
  if(cx+rx<=0||cx-rx>=view.width||cy+ry<=0||cy-ry>=view.height)continue;
  const uint32_t rgb=d.fade?interpolate(d.color,0,50):d.color;
  for(int inset=0;inset<2;++inset){std::vector<nativefx::LinePixel>pixels;nativeEllipse(cx,cy,rx-inset,ry-inset,view.width,view.height,[&](auto p){pixels.push_back(p);});sink.nativePixels(pixels,rgb,4,255,format);}
  if(!d.rays||!rx||!ry)continue;
  constexpr float strengths[4]={.05f,.2f,.4f,1.f};
  for(unsigned n=0;n<4;++n){double angle=double(int32_t(uint32_t(view.frame)+n))*.005;angle-=nativefx::chop(angle*.15915494309189535)*6.2831853071795864769;int ex=0,ey=0;
   if(std::abs(angle-1.5707963267948966)<.001)ey=-ry;
   else if(std::abs(angle-4.71238898038469)<.001)ey=ry;
   else{const double cosine=math.cosine(angle),tangent=cosine?math.sine(angle)/cosine:1e30;double x=math.rootWide(1./(1./(double(rx)*rx)+tangent*tangent/(double(ry)*ry))),y=math.rootWide((1.-x*x/(double(rx)*rx))*ry*ry);if(angle>1.5707963267948966&&angle<4.71238898038469)x=-x;if(angle<3.141592653589793)y=-y;ex=nativefx::chop(x);ey=nativefx::chop(y);}
   sink.nativeAA({{cx*sx,cy*sy}},{{(cx+ex)*sx,(cy+ey)*sy}},-500,-500,d.color,strengths[n],format,&math);
  }
 }
}
template<class Painter>void ellipse(Painter&p,int cx,int cy,int rx,int ry,uint32_t color){
 if(rx<0||ry<0||rx>8192||ry>8192)return;auto emit=[&](int x,int y){for(int sx:{-1,1})for(int sy:{-1,1})p.fill({cx+sx*x,cy+sy*y,1,1},color);};
 if(!rx||!ry){for(int x=-rx;x<=rx;++x)p.fill({cx+x,cy,1,1},color);return;}
 double x=0,y=ry,xx=double(rx)*rx,yy=double(ry)*ry,dx=0,dy=2*xx*y,decision=yy-xx*ry+.25*xx;
 while(dx<dy){emit(int(x),int(y));++x;dx+=2*yy;if(decision<0)decision+=yy+dx;else{--y;dy-=2*xx;decision+=yy+dx-dy;}}
 decision=yy*(x+.5)*(x+.5)+xx*(y-1)*(y-1)-xx*yy;
 while(y>=0){emit(int(x),int(y));--y;dy-=2*xx;if(decision>0)decision+=xx-dy;else{++x;dx+=2*yy;decision+=xx-dy+dx;}}
}
template<class Painter>void line(Painter&p,int x0,int y0,int x1,int y1,uint32_t rgb,float strength){
 // Coverage is premultiplied for the existing overlay compositor. No ordinary
 // opaque rectangle is used to stand in for the native antialiased ray.
 int steps=std::max(std::abs(x1-x0),std::abs(y1-y0));if(steps>16384)return;bool horizontal=std::abs(x1-x0)>=std::abs(y1-y0);
 for(int i=0;i<=steps;++i){double t=steps?double(i)/steps:0,x=x0+(x1-x0)*t,y=y0+(y1-y0)*t;int px=int(std::floor(x)),py=int(std::floor(y));double fraction=horizontal?y-py:x-px;
  for(int pass=0;pass<2;++pass){unsigned alpha=unsigned(std::clamp(strength*(pass?fraction:1-fraction)*255,0.,255.));if(!alpha)continue;uint32_t color=alpha<<24;for(int shift:{0,8,16})color|=((((rgb>>shift)&255)*alpha+127)/255)<<shift;p.fill({px+int(!horizontal&&pass),py+int(horizontal&&pass),1,1},color);}
 }
}
template<class Painter>bool paint(const std::vector<Draw>&draws,const mainview::View&view,double zscale,Painter&p){
 p.lane(3);for(const auto&d:draws){int rx=d.pixels?int(d.radius):radiusPixels(d.radius,d.truncate),ry=rx/2;if(rx<0||rx>8192)continue;int cx=int((int64_t(d.point.x)-d.point.y)*30/256)-view.cameraX,cy=int((int64_t(d.point.x)+d.point.y)*15/256)-screenHeight(d.point.z,zscale)-view.cameraY;
  if(cx+rx<0||cx-rx>=view.width||cy+ry<0||cy-ry>=view.height)continue;uint32_t rgb=d.fade?interpolate(d.color,0,50):d.color;ellipse(p,cx,cy,rx,ry,localui::rgb565(rgb));ellipse(p,cx,cy,rx-1,ry-1,localui::rgb565(rgb));
  if(d.rays&&rx&&ry){constexpr float strengths[4]={.05f,.2f,.4f,1.f};for(int n=0;n<4;++n){double angle=std::fmod(double(int32_t(uint32_t(view.frame)+n))*.005,6.2831853071795864769),tangent=std::tan(angle),x=std::sqrt(1./(1./(double(rx)*rx)+tangent*tangent/(double(ry)*ry))),y=std::sqrt(std::max(0.,(1.-x*x/(double(rx)*rx))*ry*ry));if(angle>1.5707963267948966&&angle<4.71238898038469)x=-x;if(angle<3.141592653589793)y=-y;line(p,cx,cy,cx+int(x),cy+int(y),d.color,strengths[n]);}}
 }return p.good();
}
}}
