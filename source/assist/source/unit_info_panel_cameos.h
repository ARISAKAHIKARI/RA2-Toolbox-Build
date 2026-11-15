#pragma once
#include <windows.h>
#include <array>
#include <map>
#include <cstdint>
#include <cstring>
#include "../../unit_monitor/source/cameo_reader.h"
#include "../../external_reader/source/unit_info_panel_model.h"

// Native TypeClass cameo SHP / the game's own converter palette.
// Only immutable image data is read through readLive(), outside WorldFrameScope.
// Decode at most one new type per 180ms; no reads or decoding in WM_PAINT.
// No files/images are copied from MO Enhancement.
namespace unitpanelcameo {
struct Icon {
 std::array<uint32_t,um::IW*um::IH> argb{};
 DWORD retryAt=0;
 bool ready=false;
};
class Cache {
 uint32_t pid_=0,match_=0;DWORD nextReadAt_=0,paletteRetryAt_=0;
 bool paletteReady_=false;
 std::array<uint32_t,256> palette_{};
 std::map<uint32_t,Icon> icons_;
 uint32_t revision_=0;
 template<class Read>bool palette(Read&&read,DWORD now){
  if(paletteReady_)return true;
  if(paletteRetryAt_&&DWORD(now-paletteRetryAt_)<4000)return false;
  paletteRetryAt_=now;
  uint32_t converter=0,at=0;int mode=0;uint16_t pixels[256]{};
  if(!read(0x87F6B0,&converter,4)||converter<0x10000||
     !read(converter+4,&mode,4)||mode!=2||
     !read(converter+0x174,&at,4)||at<0x10000||
     !read(at,pixels,sizeof pixels))return false;
  for(int i=0;i<256;++i){
   uint32_t p=pixels[i],r=((p>>11)&31)*255/31,g=((p>>5)&63)*255/63,b=(p&31)*255/31;
   palette_[i]=i?0xff000000u|(r<<16)|(g<<8)|b:0;
  }
  paletteReady_=true;return true;
 }
 template<class Read>bool readIcon(Read&&read,uint32_t type,Icon&out){
  // A native TypeClass cameo pointer has the first-frame SHP at +0x6F0.
  // The decoder handles TS SHP's clipped frame and RLE (flags 2/3).
  if(type<0x10000||type>=0x80000000)return false;
  um::Type t{};t.ptr=type;
  uint8_t buffer[65536]{};
  if(!um::readCameo(read,[](uint32_t){return uint32_t(0);},t,buffer,sizeof buffer))return false;
  for(size_t i=0;i<out.argb.size();++i)out.argb[i]=palette_[t.icon[i]];
  out.ready=true;return true;
 }
public:
 void clear(){pid_=match_=0;nextReadAt_=paletteRetryAt_=0;paletteReady_=false;icons_.clear();++revision_;}
 uint32_t revision()const{return revision_;}
 const Icon* get(uint32_t type)const{
  auto it=icons_.find(type);return it!=icons_.end()&&it->second.ready?&it->second:nullptr;
 }
 template<class Read>void refresh(const external::unitpanel::Model&model,Read read,uint32_t pid,uint32_t match,DWORD now){
  if(pid!=pid_||match!=match_){clear();pid_=pid;match_=match;}
  if(!pid||!match||!model.selected||!model.groupCount||!model.match||
     (nextReadAt_&&DWORD(now-nextReadAt_)<180))return;
  auto load=[&](uint32_t type)->bool{
   if(type<0x10000)return false;
   auto it=icons_.find(type);
   if(it!=icons_.end()&&(it->second.ready||DWORD(now-it->second.retryAt)<5000))return false;
   if(!palette(read,now))return false;
   if(icons_.size()>=128)icons_.clear(); // bounded across very large mods
   Icon&icon=icons_[type];icon.retryAt=now;
   bool ok=readIcon(read,type,icon);
   if(ok)++revision_;
   nextReadAt_=now;return true;
  };
  for(int i=0;i<model.groupCount;++i)if(load(model.groups[i].type))return;
  if(model.selected==1){
   if(load(model.single.type))return;
   for(int i=0;i<model.passengerCount;++i)if(load(model.passenger[i].type))return;
  }
 }
};
}
