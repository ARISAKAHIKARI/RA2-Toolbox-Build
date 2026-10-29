#pragma once
#include "feedback_orders.h"
#include "visual_pose.h"
// Preserve native transient pixels that are not standalone AnimClass actors.
// This only clears their SHP alpha footprint in the upgraded world surface;
// the running game still supplies color, lighting, animation and occlusion.
namespace external { namespace nativeeffects {
constexpr uint32_t GatherShape=0xB1CF98;
inline bool mergeCutouts(std::vector<nativefeedback::Rect>&out,int width,int height){
 std::vector<nativefeedback::Rect> rows;
 for(auto rect:out){rect=nativefeedback::intersect(rect,{0,0,width,height});
  if(rect.valid())for(int y=rect.y;y<rect.y+rect.h;++y){if(rows.size()>=131072)return false;rows.push_back({rect.x,y,rect.w,1});}}
 std::sort(rows.begin(),rows.end(),[](const auto&a,const auto&b){return a.y!=b.y?a.y<b.y:a.x<b.x;});
 std::vector<nativefeedback::Rect> merged;
 for(auto rect:rows){if(!merged.empty()&&merged.back().y==rect.y&&rect.x<=merged.back().x+merged.back().w)
   merged.back().w=std::max(merged.back().x+merged.back().w,rect.x+rect.w)-merged.back().x;
  else{if(merged.size()>=16384)return false;merged.push_back(rect);}}
 out=std::move(merged);return true;
}
struct GatherState {
 uint32_t object=0,id=0,type=0,loco=0;int phase=0,frame=0;uint16_t facing=0;
 nativefeedback::Point3 location,point;
};
// 4CACB0/4CAD00 use the native quantized sine table. These eight mathematical
// results (not copied table bytes) were checked at the actual mining draw call.
inline nativefeedback::Point3 gatherPoint(nativefeedback::Point3 p,unsigned direction){
 constexpr double q=.707106769084930419921875,half=1.2246063538223773e-16;
 constexpr std::array<double,8> cosine{{half,q,1,q,-2*half,-q,-1,-q}};
 constexpr std::array<double,8> sine{{1,q,0,-q,-1,-q,half,q}};
 // The engine adds in x87 extended precision before truncating. Splitting the
 // integer and fractional parts retains its tiny negative cardinal offsets
 // without depending on the host compiler's long-double representation.
 auto coordinate=[](int base,double offset){double whole=std::floor(offset);int result=base+int(whole);return result<0&&offset!=whole?result+1:result;};
 p.x=coordinate(p.x,cosine[direction&7]*30);p.y=coordinate(p.y,-sine[direction&7]*30);return p;
}
// Read only a proven stationary Drive locomotor path. Unknown/modded movement
// implementations do not grant a transparent footprint for an assumed effect.
template<class Read>bool gather(Read read,const nativefeedback::Object&object,int frame,GatherState&out){
 using namespace nativefeedback;
 uint32_t vt=0;if(!range(object.pointer,0x6D4)||!read(object.pointer,&vt,4)||vt!=0x7F5C70)return false;
 uint8_t harvesting=0;if(!read(object.pointer+0x6D2,&harvesting,1)||harvesting!=1)return false;
 GatherState next;next.object=object.pointer;next.id=object.id;next.type=object.type;
 uint32_t id=0,type=0;uint8_t harvester=0,hidden[2]{};uint32_t special=0,lvt=0,linked=0;
 if(!object.id||!read(object.pointer+0x10,&id,4)||id!=object.id||!read(object.pointer+0x6C4,&type,4)||type!=object.type||
    !range(type,0xE0F)||!read(type+0xE0E,&harvester,1)||!harvester||
    !read(object.pointer+0x270,hidden,2)||hidden[0]||hidden[1]||!read(object.pointer+0x278,&special,4)||special||
    !read(object.pointer+0x9C,&next.location,sizeof next.location)||!worldPoint(next.location)||
    !read(object.pointer+0x538,&next.phase,4)||
    !read(object.pointer+0x674,&next.loco,4)||!range(next.loco,0x48)||
    !read(next.loco,&lvt,4)||lvt!=0x7E7EB0||!read(next.loco+8,&linked,4)||linked!=object.pointer)return false;
 std::array<uint8_t,24> facing{};std::array<int,6> destinations{};std::array<int,3> empty{};
 if(!read(object.pointer+0x388,facing.data(),facing.size())||!read(next.loco+0x30,destinations.data(),sizeof destinations)||
    !read(0x8A0790,empty.data(),sizeof empty))return false;
 int started=0,duration=0;std::memcpy(&started,facing.data()+8,4);std::memcpy(&duration,facing.data()+16,4);
 int16_t rate=0;std::memcpy(&rate,facing.data()+20,2);
 if(rate>0&&(started==-1?int64_t(duration):int64_t(duration)-(int64_t(frame)-started))>0)return false;
 const bool noDestination=std::equal(empty.begin(),empty.end(),destinations.begin()+3);
 const bool noNext=std::equal(empty.begin(),empty.end(),destinations.begin());
 // 4AFB80 returns unpowered when the next destination is empty and the
 // remaining destination is empty or already at the current x/y.
 if(!noDestination&&(!noNext||destinations[3]!=next.location.x||destinations[4]!=next.location.y))return false;
 next.facing=nativefeedback::u16(facing.data());
 unsigned direction=(((unsigned(next.facing)>>12)+1)>>1)&7;
 next.point=gatherPoint(next.location,direction);
 const int32_t combined=int32_t(uint32_t(next.phase)+uint32_t(frame));
 next.frame=(7-int(direction))*15+combined%15;
 if(next.frame<0||next.frame>=120)return false;
 if(!worldPoint(next.point))return false;
 uint32_t idAfter=0,typeAfter=0,locoAfter=0;uint8_t harvestingAfter=0;nativefeedback::Point3 positionAfter;
 std::array<uint8_t,24> facingAfter{};
 if(!read(object.pointer+0x10,&idAfter,4)||idAfter!=id||!read(object.pointer+0x6C4,&typeAfter,4)||typeAfter!=type||
    !read(object.pointer+0x674,&locoAfter,4)||locoAfter!=next.loco||!read(object.pointer+0x6D2,&harvestingAfter,1)||harvestingAfter!=1||
    !read(object.pointer+0x9C,&positionAfter,sizeof positionAfter)||std::memcmp(&positionAfter,&next.location,sizeof positionAfter)||
    !read(object.pointer+0x388,facingAfter.data(),facingAfter.size())||facingAfter!=facing)return false;
 out=next;return true;
}
class EffectStencil {
 struct Mask {std::array<uint8_t,24> header{};int x=0,y=0;std::vector<nativefeedback::Rect> runs;};
 uint32_t shape_=0;std::array<uint8_t,8> header_{};std::map<unsigned,Mask> masks_;
 template<class Read>bool mask(Read read,uint32_t pointer,unsigned frame,const Mask*&out){
  using namespace nativefeedback;
  std::array<uint8_t,8> header{};if(!range(pointer,16)||!read(pointer,header.data(),8))return false;
  if(u16(header.data())==0xffff){uint32_t resolved=0;if(!read(pointer+12,&resolved,4)||!range(resolved,8)||!read(resolved,header.data(),8))return false;pointer=resolved;}
  if(u16(header.data())||frame>=u16(header.data()+6))return false;
  if(pointer!=shape_||header_!=header){clear();shape_=pointer;header_=header;}
  std::array<uint8_t,24> fh{};if(!range(pointer,8+size_t(frame+1)*24)||!read(pointer+8+frame*24,fh.data(),fh.size()))return false;
  auto found=masks_.find(frame);if(found!=masks_.end()&&found->second.header==fh){out=&found->second;return true;}
  std::array<uint32_t,256> palette;palette.fill(0xffffffff);palette[0]=0;voxel::Image image;
  if(!voxel::sprite(read,pointer,palette,image,int(frame),nullptr,255)||image.width>512||image.height>512)return false;
  Mask value;value.header=fh;value.x=image.x;value.y=image.y;
  for(int y=0;y<image.height;++y)for(int x=0;x<image.width;){
   if(!(image.pixels[size_t(y)*image.width+x]>>24)){++x;continue;}int left=x++;
   while(x<image.width&&(image.pixels[size_t(y)*image.width+x]>>24))++x;
   value.runs.push_back({left,y,x-left,1});
  }
  if(found==masks_.end()&&masks_.size()>=120)return false;
  out=&masks_.insert_or_assign(frame,std::move(value)).first->second;return true;
 }
public:
 void clear(){shape_=0;header_={};masks_.clear();}
 // On success append to placement/beam spans, preserving the caller's existing
 // cutouts. Malformed/inactive objects cannot punch guessed rectangles.
 template<class Read>bool capture(Read read,const mainview::View&view,int width,int height,const std::vector<nativefeedback::Object>&objects,std::vector<nativefeedback::Rect>&out){
  if(!view.valid()||width<1||height<1||width>8192||height>8192||objects.size()>8192)return false;
  const double sx=double(width)/view.width,sy=double(height)/view.height;double zscale=0;bool hasScale=false;
  if(!std::isfinite(sx)||!std::isfinite(sy)||sx<=0||sy<=0||sx>16||sy>16)return false;
  int frame=0;if(!read(0xA8ED84,&frame,4)||frame<0)return false;
  uint32_t shape=0;std::vector<nativefeedback::Rect> pending;
  for(const auto&object:objects){GatherState state;if(!gather(read,object,frame,state))continue;
   // A default scale is acceptable for a standalone preview, never for holes
   // exposing pixels the game drew at its current projection.
   if(!hasScale){if(!read(0xB0CD48,&zscale,8)||!std::isfinite(zscale)||zscale<=0||zscale>1)return false;hasScale=true;}
   if(!shape&&(!read(GatherShape,&shape,4)||!nativefeedback::range(shape,16)))return false;
   const Mask*image=nullptr;if(!mask(read,shape,unsigned(state.frame),image))return false;
   const int x=int((int64_t(state.point.x)-state.point.y)*30/256)-view.cameraX;
   const int y=int((int64_t(state.point.x)+state.point.y)*15/256)-screenHeight(state.point.z,zscale)-view.cameraY;
   for(auto run:image->runs){
    auto rect=nativefeedback::intersect({int(std::floor((x+image->x+run.x)*sx)),int(std::floor((y+image->y+run.y)*sy)),
     int(std::ceil((x+image->x+run.x+run.w)*sx))-int(std::floor((x+image->x+run.x)*sx)),
     int(std::ceil((y+image->y+run.y+1)*sy))-int(std::floor((y+image->y+run.y)*sy))},{0,0,width,height});
    if(rect.valid())for(int yy=rect.y;yy<rect.y+rect.h;++yy){if(pending.size()>=131072)return false;pending.push_back({rect.x,yy,rect.w,1});}
   }
  }
  int after=0;uint32_t shapeAfter=0;
  if(!pending.empty()&&(!read(0xA8ED84,&after,4)||after!=frame||!read(GatherShape,&shapeAfter,4)||shapeAfter!=shape))return false;
  if(!mergeCutouts(pending,width,height)||out.size()+pending.size()>131072)return false;
  out.insert(out.end(),pending.begin(),pending.end());return true;
 }
};
}}
