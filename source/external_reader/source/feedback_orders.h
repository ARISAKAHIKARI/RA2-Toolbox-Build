#pragma once
#include "feedback_selection.h"
#include "main_viewport.h"
#include "map_bounds.h"
#include "voxel_preview.h"
#include <map>
#include <memory>
namespace external { namespace nativefeedback {
struct Stroke {
 int x0=0,y0=0,x1=0,y1=0;uint32_t color=0;
 bool operator==(const Stroke&o)const{return x0==o.x0&&y0==o.y0&&x1==o.x1&&y1==o.y1&&color==o.color;}
};
inline bool clipStroke(double&x0,double&y0,double&x1,double&y1,int width,int height){
 if(!std::isfinite(x0)||!std::isfinite(y0)||!std::isfinite(x1)||!std::isfinite(y1)||width<1||height<1)return false;
 double dx=x1-x0,dy=y1-y0,lo=0,hi=1;
 auto edge=[&](double p,double q){if(!p)return q>=0;double t=q/p;if(p<0){if(t>hi)return false;lo=std::max(lo,t);}else{if(t<lo)return false;hi=std::min(hi,t);}return true;};
 if(!edge(-dx,x0)||!edge(dx,width-1-x0)||!edge(-dy,y0)||!edge(dy,height-1-y0))return false;
 x1=x0+hi*dx;y1=y0+hi*dy;x0+=lo*dx;y0+=lo*dy;return true;
}
template<class Read>bool actionColor(Read read,unsigned index,uint32_t&out){
 uint32_t converter=0,bpp=0,row=0,again=0;uint16_t color=0;std::array<int,6>shifts{},verify{};
 if(index>255||!read(0x87F6C4,&converter,4)||!range(converter,0x178)||!read(converter+4,&bpp,4)||bpp!=2||
  !read(converter+0x174,&row,4)||!range(row,512)||!read(row+index*2,&color,2)||
  !read(0x8A0DD0,shifts.data(),sizeof shifts)||!read(converter+0x174,&again,4)||again!=row||
  !read(0x8A0DD0,verify.data(),sizeof verify)||verify!=shifts)return false;
 if(shifts!=std::array<int,6>{{11,3,0,3,5,2}}&&shifts!=std::array<int,6>{{10,3,0,3,5,3}})return false;
 unsigned r=((color>>shifts[0])<<shifts[1])&255,g=((color>>shifts[4])<<shifts[5])&255,b=((color>>shifts[2])<<shifts[3])&255;
 out=0xff000000u|(r<<16)|(g<<8)|b;return true;
}
// Cell endpoints follow the projected terrain Level and bridge plane. The
// action line is functional feedback, not an exact muzzle/terrain-slope replay.
template<class Read>bool actionPoint(Read read,uint32_t object,const mainview::View&view,double zscale,double&x,double&y){
 uint32_t vt=0;if(!range(object,0x28)||!read(object,&vt,4))return false;
 if(vt==0x7E4EEC){
  int16_t cell[2]{};int8_t level=0;uint32_t flags=0;
  if(!read(object+0x24,cell,4)||!read(object+0x11B,&level,1)||!read(object+0x140,&flags,4)||cell[0]<=0||cell[1]<=0||cell[0]>=512||cell[1]>=512)return false;
  x=(cell[0]-cell[1])*30.-view.cameraX;y=(cell[0]+cell[1]+1-int(level))*15.-view.cameraY;
  if(flags&0x100)y-=60;return true;
 }
 int kind=vt==0x7E22A4?0:vt==0x7EB058?1:vt==0x7F5C70?2:vt==0x7E3EBC?3:-1;
 if(kind<0||!range(object,0x220))return false;
 Point3 point;if(!read(object+0x9C,&point,sizeof point)||!worldPoint(point))return false;
 if(kind==3){
  uint32_t type=0;int foundation=-1,w=0,h=0;
  if(!read(object+0x520,&type,4)||!range(type,0xEF4)||!read(type+0xEF0,&foundation,4)||foundation<0||foundation>20||
   !read(0x8192B8+foundation*4,&w,4)||!read(0x819310+foundation*4,&h,4)||w<1||h<1||w>32||h>32)return false;
  point.x+=w*128-128;point.y+=h*128-128;
 }
 x=(double(point.x)-point.y)*30/256.-view.cameraX;
 y=(double(point.x)+point.y)*15/256.-screenHeight(point.z,zscale)-view.cameraY;return true;
}
template<class Read>bool actionLines(Read read,const mainview::View&view,int width,int height,std::vector<Stroke>&out){
 out.clear();uint8_t enabled=0;int timer[3]{},again[3]{};
 if(!read(0x843108,&enabled,1)||enabled>1)return false;
 if(!enabled)return true;
 if(!read(0xB0EA80,timer,sizeof timer))return false;
 if(timer[2]<=0||(timer[0]!=-1&&(int64_t(view.frame)-timer[0]>=timer[2])))return true;
 std::array<uint32_t,6> head{},after{};
 if(!read(0xA8ECB8,head.data(),sizeof head)||head[2]>8192||head[4]>head[2]||head[4]>8192||
  (head[4]&&!range(head[1],size_t(head[4])*4)))return false;
 std::vector<uint32_t> selected(head[4]);if(!selected.empty()&&!read(head[1],selected.data(),selected.size()*4))return false;
 auto sorted=selected;std::sort(sorted.begin(),sorted.end());if(std::adjacent_find(sorted.begin(),sorted.end())!=sorted.end())return false;
 uint32_t attack=0,move=0;if(!selected.empty()&&(!actionColor(read,8,attack)||!actionColor(read,3,move)))return false;
 const double sx=double(width)/view.width,sy=double(height)/view.height,zscale=projectionZScale(read);
 for(uint32_t p:selected){
  uint32_t vt=0,owner=0,id=0,target=0,destination=0;uint8_t flags[4]{};
  if(!range(p,0x5A8)||!read(p,&vt,4)||!read(p+0x10,&id,4)||!id||!read(p+0x21C,&owner,4)||
   !read(p+0x80,flags,4)||flags[1]||!flags[3])return false;
  if(owner!=view.house||vt==0x7E3EBC)continue;
  if(vt!=0x7F5C70&&vt!=0x7EB058&&vt!=0x7E22A4)continue;
  if(!read(p+0x2B4,&target,4)||!read(p+0x5A4,&destination,4))return false;
  uint32_t endpoint=target?target:destination;if(!endpoint)continue;
  if(!target){
   uint32_t queue=0;int count=0,capacity=0;
   if(!read(p+0x58C,&queue,4)||!read(p+0x590,&capacity,4)||!read(p+0x598,&count,4)||count<0||count>512||capacity<count||capacity>8192)return false;
   if(count&&(!range(queue,size_t(count)*4)||!read(queue+(count-1)*4,&endpoint,4)))return false;
  }
  double x0=0,y0=0,x1=0,y1=0;
  if(!actionPoint(read,p,view,zscale,x0,y0)||!actionPoint(read,endpoint,view,zscale,x1,y1))continue;
  x0*=sx;y0*=sy;x1*=sx;y1*=sy;
  if(clipStroke(x0,y0,x1,y1,width,height))out.push_back({int(std::lround(x0)),int(std::lround(y0)),int(std::lround(x1)),int(std::lround(y1)),target?attack:move});
  uint32_t verifyID=0,verifyTarget=0,verifyDest=0;
  if(!read(p+0x10,&verifyID,4)||verifyID!=id||!read(p+0x2B4,&verifyTarget,4)||verifyTarget!=target||!read(p+0x5A4,&verifyDest,4)||verifyDest!=destination)return false;
 }
 if(!read(0x843108,&enabled,1)||enabled!=1||!read(0xB0EA80,again,sizeof again)||std::memcmp(timer,again,sizeof timer)||
  !read(0xA8ECB8,after.data(),sizeof after)||head!=after)return false;
 if(!selected.empty()){std::vector<uint32_t> verify(selected.size());if(!read(head[1],verify.data(),verify.size()*4)||verify!=selected)return false;}
 return true;
}
inline bool paintInteractions(const std::vector<Primitive>&primitives,const std::vector<Stroke>&lines,Canvas&out,Rect&bounds){
 bounds=envelope(primitives);
 for(const auto&line:lines){Rect r{std::min(line.x0,line.x1)-2,std::min(line.y0,line.y1)-2,std::abs(line.x1-line.x0)+5,std::abs(line.y1-line.y0)+5};
  if(!bounds.valid())bounds=r;else{int x=std::min(bounds.x,r.x),y=std::min(bounds.y,r.y);bounds={x,y,std::max(bounds.x+bounds.w,r.x+r.w)-x,std::max(bounds.y+bounds.h,r.y+r.h)-y};}}
 if(!bounds.valid()||!out.reset(bounds.w,bounds.h))return false;
 for(const auto&line:lines){int x0=line.x0-bounds.x,y0=line.y0-bounds.y,x1=line.x1-bounds.x,y1=line.y1-bounds.y;
  out.line(x0,y0,x1,y1,line.color);out.fill({x0-1,y0-1,3,3},line.color);out.fill({x1-1,y1-1,3,3},line.color);}
 for(const auto&p:primitives)out.fill({p.rect.x-bounds.x,p.rect.y-bounds.y,p.rect.w,p.rect.h},p.color);return true;
}
// Native placement validity is evaluated inside Cell::Draw and is not stored
// in Cell's foundation bits. Preserve the real colored stencil through exact
// alpha spans instead of guessing placement legality or calling game code.
class FoundationStencil {
 struct Mask {int x=0,y=0;std::vector<Rect>runs;};
 uint32_t shape_=0;std::map<unsigned,Mask>cache_;
 template<class Read>bool mask(Read read,uint32_t shape,unsigned frame,const Mask*&out){
  if(shape_!=shape){cache_.clear();shape_=shape;}
  auto found=cache_.find(frame);if(found!=cache_.end()){out=&found->second;return true;}
  std::array<uint32_t,256>pal;pal.fill(0xffffffffu);pal[0]=0;voxel::Image image;
  if(!voxel::sprite(read,shape,pal,image,int(frame),nullptr,255)||image.width>512||image.height>512)return false;
  Mask value;value.x=image.x;value.y=image.y;
  for(int y=0;y<image.height;++y)for(int x=0;x<image.width;){
   if(!(image.pixels[size_t(y)*image.width+x]>>24)){++x;continue;}int first=x++;
   while(x<image.width&&(image.pixels[size_t(y)*image.width+x]>>24))++x;
   value.runs.push_back({first,y,x-first,1});
  }
  if(cache_.size()>=32)return false;out=&cache_.emplace(frame,std::move(value)).first->second;return true;
 }
public:
 void clear(){shape_=0;cache_.clear();}
 template<class Read,class UI>bool capture(Read read,UI ui,const mainview::View&view,int width,int height,std::vector<Rect>&out){
  out.clear();std::array<uint8_t,0x38>state{},again{};
  if(!ui(Map+0x1174,state.data(),state.size()))return false;
  uint32_t building=u32(state.data()+0x30),type=u32(state.data()+0x34),copyBuilding=u32(state.data()+0x1C),copyType=u32(state.data()+0x20);
  if(!building&&!type&&!copyBuilding&&!copyType)return true;
  uint32_t shape=0,cells=0;int count=0;
  if(!ui(0x8A03FC,&shape,4)||!range(shape,8)||!read(Map+0x13C,&cells,4)||!read(Map+0x140,&count,4)||count<1||count>512*512||!range(cells,size_t(count)*4))return false;
  const auto bounds=mapbounds::sample(read);if(!bounds.valid())return false;
  const double sx=double(width)/view.width,sy=double(height)/view.height;
  if(!std::isfinite(sx)||!std::isfinite(sy)||sx<=0||sy<=0||sx>16||sy>16)return false;
  std::vector<Rect> pending;
  auto foundation=[&](bool copy)->bool{
   const int center=copy?0x0E:0,offset=copy?0x12:4,data=copy?0x18:8;
   if(copy?(!copyBuilding&&!copyType):(!building&&!type))return true;
   if(copy?(copyBuilding&&!range(copyBuilding,4))||(copyType&&!range(copyType,4)):(building&&!range(building,4))||(type&&!range(type,4)))return false;
   uint32_t pointer=u32(state.data()+data);if(!range(pointer,4))return false;
   int cx=int16_t(u16(state.data()+center))+int16_t(u16(state.data()+offset));
   int cy=int16_t(u16(state.data()+center+2))+int16_t(u16(state.data()+offset+2));
   const int limit=copy?50:120;bool ended=false;
   for(int i=0;i<=limit;++i){
    int16_t cell[2]{};if(!ui(pointer+uint32_t(i)*4,cell,sizeof cell))return false;
    if(cell[0]==0x7fff&&cell[1]==0x7fff){ended=true;break;}if(i==limit||cell[0]<-32||cell[0]>32||cell[1]<-32||cell[1]>32)return false;
    int x=cx+cell[0],y=cy+cell[1];if(!bounds.legal(x,y))continue;size_t index=size_t(y)*512+x;if(index>=size_t(count))continue;
    uint32_t p=0;int16_t identity[2]{};int8_t level=0;uint8_t slope=0,flags=0;
    if(!read(cells+uint32_t(index)*4,&p,4)||!range(p,0x12D)||!read(p+0x24,identity,4)||identity[0]!=x||identity[1]!=y||
     !read(p+0x11B,&level,1)||!read(p+0x11C,&slope,1)||slope>20||!ui(p+0x12C,&flags,1))return false;
    if(!(flags&(copy?0x04:0x02)))continue;
    const int px=(x-y)*30-view.cameraX,py=(x+y-int(level))*15-view.cameraY-1;
    for(unsigned frame:std::array<unsigned,2>{{copy?1u:0u,unsigned(2+slope)}}){
     const Mask*image=nullptr;if(!mask(ui,shape,frame,image))return false;
     for(auto run:image->runs){
      int l=int(std::floor((px+image->x+run.x)*sx)),r=int(std::ceil((px+image->x+run.x+run.w)*sx));
      int t=int(std::floor((py+image->y+run.y)*sy)),b=int(std::ceil((py+image->y+run.y+1)*sy));
      l=std::max(0,l);r=std::min(width,r);t=std::max(0,t);b=std::min(height,b);if(l>=r||t>=b)continue;
      for(int yy=t;yy<b;++yy){if(pending.size()>=131072)return false;pending.push_back({l,yy,r-l,1});}
     }
    }
   }
   return ended;
  };
  if(!foundation(false)||!foundation(true)||!ui(Map+0x1174,again.data(),again.size())||again!=state)return false;
  uint32_t shapeAfter=0;if(!ui(0x8A03FC,&shapeAfter,4)||shapeAfter!=shape)return false;
  std::sort(pending.begin(),pending.end(),[](const Rect&a,const Rect&b){return a.y!=b.y?a.y<b.y:a.x<b.x;});
  for(auto r:pending){if(!out.empty()&&out.back().y==r.y&&r.x<=out.back().x+out.back().w)out.back().w=std::max(out.back().x+out.back().w,r.x+r.w)-out.back().x;else out.push_back(r);}
  return out.size()<=16384;
 }
 template<class Read>bool capture(Read read,const mainview::View&view,int width,int height,std::vector<Rect>&out){return capture(read,read,view,width,height,out);}
};
}}
