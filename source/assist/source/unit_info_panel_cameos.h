#pragma once
#include <array>
#include <map>
#include <cstdint>
#include <cstring>
#include <vector>
#include <algorithm>
#include "../../unit_monitor/source/cameo_reader.h"
#include "../../external_reader/source/unit_info_panel_model.h"

// Select the actual cameo via the game's live Rules/Art metadata and the
// existing game-assets MIX / loose-file loader. If the file is unavailable or
// malformed, preserve the native TypeClass SHP pointer as a safe fallback.
// Only immutable image data is read via readLive(), outside WorldFrameScope.
// Decode at most one new type per 180ms; no reads or decoding in WM_PAINT.
// No files/images are copied from MO Enhancement.
namespace unitpanelcameo {
struct Icon {
 std::array<uint32_t,um::IW*um::IH> argb{};
 uint32_t retryAt=0;
 bool ready=false;
};
class Cache {
 uint32_t pid_=0,match_=0,nextReadAt_=0,paletteRetryAt_=0;
 bool paletteReady_=false;
 std::array<uint32_t,256> palette_{};
 std::map<uint32_t,Icon> icons_;
 uint32_t revision_=0;
 template<class Read>bool palette(Read read,uint32_t now,um::AssetLoad files){
  if(paletteReady_)return true;
  if(paletteRetryAt_&&uint32_t(now-paletteRetryAt_)<4000)return false;
  paletteRetryAt_=now;
  // The same PAL accompanies CAMEO SHPs in MIX. Unlike an object palette,
  // these 768 bytes are unsigned 6-bit RGB; reject malformed MOD assets.
  std::vector<uint8_t> colors;
  if(files&&files("CAMEO.PAL",colors)&&colors.size()==768&&
     std::all_of(colors.begin(),colors.end(),[](uint8_t x){return x<=63;})){
   for(unsigned i=0;i<256;++i){
    const auto*rgb=colors.data()+i*3;
    palette_[i]=i?0xff000000u|(uint32_t(rgb[0])*4<<16)|
      (uint32_t(rgb[1])*4<<8)|(uint32_t(rgb[2])*4):0u;
   }
   paletteReady_=true;return true;
  }
  // Legacy live converter remains valid when a MIX/loose PAL is absent.
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
 template<class Read,class Resolve>bool readIcon(Read read,Resolve resolve,
         um::AssetLoad files,uint32_t type,Icon&out){
  if(type<0x10000||type>=0x80000000)return false;
  um::Type t{};t.ptr=type;
  // Resolve from live Type ID, the current game's Art/Rules overrides and
  // MIX directory, rather than assuming a native +0x6F0 pointer exists.
  // The shared decoder attempts the resolved file first, then the native SHP.
  if(read(type+0x24,t.id,23)&&t.id[0])resolve(t);
  uint8_t buffer[65536]{};
  if(!um::readCameo(read,[](uint32_t){return uint32_t(0);},
                    t,buffer,sizeof buffer,files))return false;
  for(size_t i=0;i<out.argb.size();++i)out.argb[i]=palette_[t.icon[i]];
  out.ready=true;return true;
 }
public:
 void clear(){pid_=match_=0;nextReadAt_=paletteRetryAt_=0;paletteReady_=false;icons_.clear();++revision_;}
 uint32_t revision()const{return revision_;}
 const Icon* get(uint32_t type)const{
  auto it=icons_.find(type);return it!=icons_.end()&&it->second.ready?&it->second:nullptr;
 }
 template<class Read,class Resolve>void refresh(const external::unitpanel::Model&model,
       Read read,Resolve resolve,um::AssetLoad files,
       uint32_t pid,uint32_t match,uint32_t now){
  if(pid!=pid_||match!=match_){clear();pid_=pid;match_=match;}
  if(!pid||!match||!model.selected||!model.groupCount||!model.match||
     (nextReadAt_&&uint32_t(now-nextReadAt_)<180))return;
  auto load=[&](uint32_t type)->bool{
   if(type<0x10000)return false;
   auto it=icons_.find(type);
   if(it!=icons_.end()&&(it->second.ready||uint32_t(now-it->second.retryAt)<5000))return false;
   if(!palette(read,now,files))return false;
   if(icons_.size()>=128)icons_.clear(); // bounded across very large mods
   Icon&icon=icons_[type];icon.retryAt=now;
   bool ok=readIcon(read,resolve,files,type,icon);
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
