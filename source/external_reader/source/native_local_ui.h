#pragma once
#include "native_feedback.h"
#include "main_viewport.h"
#include "feedback_selection.h"
#include "native_beam_effects.h"
#include "map_bounds.h"
#include <set>
namespace external {namespace localui {
// MAIN-VIEW ONLY. These are native UI preservation regions, never world
// materials or shared camera pixels. No game/font function is invoked.
struct Font {int width=0,height=0;};
template<class Read>bool font(Read read,Font&out){uint32_t p=0,data=0;int width=0,height=0;
 if(!read(0x89C4D0,&p,4)||!nativefeedback::range(p,0x24)||!read(p+4,&data,4)||!nativefeedback::range(data,12)||!read(data,&width,4)||!read(p+0x1C,&height,4)||width<1||width>128||height<1||height>128)return false;
 out={width,height};return true;
}
template<class Read>int length(Read read,uint32_t text){if(!nativefeedback::range(text,1024))return -1;int count=0;for(;count<512;++count){uint16_t c=0;if(!read(text+count*2,&c,2))return -1;if(!c)return count;}return -1;}
inline void preserve(nativefeedback::Rect native,const mainview::View&view,int width,int height,std::vector<nativefeedback::Rect>&out){
 auto clipped=nativefeedback::intersect(native,{0,0,view.width,view.height});if(!clipped.valid())return;
 const double sx=double(width)/view.width,sy=double(height)/view.height;int x=int(std::floor(clipped.x*sx)),y=int(std::floor(clipped.y*sy));
 int right=int(std::ceil((clipped.x+clipped.w)*sx)),bottom=int(std::ceil((clipped.y+clipped.h)*sy));auto r=nativefeedback::intersect({x,y,right-x,bottom-y},{0,0,width,height});if(r.valid())out.push_back(r);
}
template<class Read>bool buildings(Read read,const mainview::View&view,const std::vector<nativefeedback::Object>&objects,int width,int height,std::vector<nativefeedback::Rect>&out){
 if(!view.valid()||width<1||height<1||width>8192||height>8192||objects.size()>8192)return false;
 const double sx=double(width)/view.width,sy=double(height)/view.height;std::vector<nativefeedback::Rect>pending;
 for(const auto&o:objects){if(o.kind!=3||o.owner!=view.house)continue;if(!o.bounds.valid()||o.bounds.w>65536||o.bounds.h>65536||std::abs(int64_t(o.bounds.x))>1000000||std::abs(int64_t(o.bounds.y))>1000000)return false;std::array<uint8_t,0x530>data{};
  if(!nativefeedback::range(o.pointer,data.size())||!read(o.pointer,data.data(),data.size()))continue;
  if(nativefeedback::u32(data.data())!=0x7E3EBC||nativefeedback::u32(data.data()+0x10)!=o.id||nativefeedback::u32(data.data()+0x520)!=o.type||nativefeedback::u32(data.data()+0x21C)!=o.owner||data[0x81]||!data[0x74]||!data[0x90]||int32_t(nativefeedback::u32(data.data()+0x6C))<=0)continue;
  if(data[0x3D3]>1||data[0x83]>1)return false;
  if(data[0x3D3]){
   // Conservatively preserve the native factory and its labels together.
   // This avoids fabricating a CSF translation/font anchor. This small area
   // intentionally uses native pixels while the rest of the scene upgrades.
   int mx=int(std::ceil(32*sx)),my=int(std::ceil(32*sy));auto r=nativefeedback::intersect({o.bounds.x-mx,o.bounds.y-my,o.bounds.w+2*mx,o.bounds.h+2*my},{0,0,width,height});if(r.valid())pending.push_back(r);
  }
  const uint32_t target=nativefeedback::u32(data.data()+0x218);
  if(data[0x83]&&target){int16_t cell[2]{};uint32_t cells=0,actual=0;int count=0;int8_t level=0;
   auto bounds=mapbounds::sample(read);
   if(!nativefeedback::range(target,0x11C)||!read(target+0x24,cell,sizeof cell)||!bounds.legal(cell[0],cell[1]))continue;
   const int index=int(cell[1])*512+cell[0];if(!read(mapbounds::Map+0x13C,&cells,4)||!read(mapbounds::Map+0x140,&count,4)||index<0||index>=count||count>512*512||!nativefeedback::range(cells,size_t(count)*4)||!read(cells+index*4,&actual,4)||actual!=target||!read(target+0x11B,&level,1))continue;
   const int x=(int(cell[0])-cell[1])*30-view.cameraX,y=(int(cell[0])+cell[1]+1)*15-int(level)*15-view.cameraY;
   // Covers the native animated rally flag, including a bridge-height pole.
   preserve({x-32,y-128,64,160},view,width,height,pending);
   const int fromX=int((o.bounds.x+o.bounds.w/2)/sx),fromY=int((o.bounds.y+o.bounds.h/2)/sy);
   if(!nativeeffects::beamCorridor(fromX,fromY,x,y,2,view,width,height,pending))return false;
  }
  uint32_t id=0,type=0,owner=0,archive=0;uint8_t primary=0;
  if(!read(o.pointer+0x10,&id,4)||id!=o.id||!read(o.pointer+0x520,&type,4)||type!=o.type||!read(o.pointer+0x21C,&owner,4)||owner!=o.owner||!read(o.pointer+0x218,&archive,4)||archive!=target||!read(o.pointer+0x3D3,&primary,1)||primary!=data[0x3D3])return false;
 }
 out.insert(out.end(),pending.begin(),pending.end());return true;
}
template<class Read>bool capture(Read read,const mainview::View&view,int width,int height,std::vector<nativefeedback::Rect>&out){
 if(!view.valid()||width<1||height<1)return false;
 std::array<uint32_t,10> message{},again{};if(!read(0xA8BC60,message.data(),sizeof message))return false;
 std::vector<nativefeedback::Rect>pending;Font f;bool haveFont=false;auto needFont=[&]{return haveFont||(haveFont=font(read,f));};
 uint32_t label=message[0];std::set<uint32_t>seen;
 while(label){if(seen.size()>=256||!seen.insert(label).second||!nativefeedback::range(label,0x4C))return false;
  std::array<uint8_t,0x4C> data{},verify{};if(!read(label,data.data(),data.size())||!read(label,verify.data(),verify.size())||data!=verify||nativefeedback::u32(data.data())!=0x7F5B44)return false;
  if(!data[0x40]){int n=length(read,nativefeedback::u32(data.data()+0x30));if(n<0||!needFont())return false;
   if(n){int64_t x=int32_t(nativefeedback::u32(data.data()+0xC))-int64_t(view.x),y=int32_t(nativefeedback::u32(data.data()+0x10))-int64_t(view.y);
    if(std::abs(int64_t(x))>32768||std::abs(int64_t(y))>32768)return false;
    // Includes left/center/right text styles, native shadow and wide glyphs.
    const int span=std::min(32768,n*2*f.width+4);preserve({int(x)-span,int(y)-2,2*span+4,f.height+5},view,width,height,pending);
   }
  }label=nativefeedback::u32(data.data()+4);
 }
 std::array<uint32_t,6> supers{},supersAfter{};if(!read(0xA83D50,supers.data(),sizeof supers)||supers[4]>supers[2]||supers[2]>8192||(supers[4]&&!nativefeedback::range(supers[1],supers[4]*4)))return false;
 int rows=0,maxChars=0;std::vector<uint32_t>items(supers[4]);if(!items.empty()&&!read(supers[1],items.data(),items.size()*4))return false;
 for(auto p:items){uint32_t vt=0,type=0,title=0;int start=-1,duration=0;
  if(!nativefeedback::range(p,0x80)||!read(p,&vt,4)||vt!=0x7F3FE8||!read(p+0x28,&type,4)||!nativefeedback::range(type,0x64)||!read(type+0x60,&title,4)||!read(p+0x30,&start,4)||!read(p+0x38,&duration,4))return false;
  int n=length(read,title);if(n<0)return false;if(duration<=0)continue;++rows;maxChars=std::max(maxChars,n+14);
 }
 // Mission countdown uses the same DrawTimer stack and precedes SW rows.
 int missionStart=-1,missionDuration=0;uint32_t missionTitle=0;
 if(nativefeedback::range(view.scenario,0x11F8)&&read(view.scenario+0x11E8,&missionStart,4)&&missionStart!=-1&&read(view.scenario+0x11F0,&missionDuration,4)&&missionDuration>0){
  if(!read(view.scenario+0x11F4,&missionTitle,4))return false;int n=length(read,missionTitle);if(n<0)return false;++rows;maxChars=std::max(maxChars,n+14);
 }
 if(rows){if(!needFont())return false;int span=std::min(view.width,maxChars*2*f.width+6),bandHeight=std::min(view.height,rows*(f.height+2)+4);preserve({view.width-span,view.height-bandHeight,span,bandHeight},view,width,height,pending);}
 if(!read(0xA8BC60,again.data(),sizeof again)||again!=message||!read(0xA83D50,supersAfter.data(),sizeof supersAfter)||supersAfter!=supers)return false;
 out.insert(out.end(),pending.begin(),pending.end());return true;
}
}}
