#pragma once
#include <array>
#include <cstdint>
namespace assist {namespace crcshadow {
// Only MapIsClear is modeled. Never compute a replacement whole-game digest.
struct Identity {uint32_t epoch=0,pointer=0,id=0;unsigned index=32;
 constexpr bool valid()const{return epoch&&pointer&&id&&index<32;}
 constexpr bool operator==(const Identity&o)const{return epoch==o.epoch&&pointer==o.pointer&&id==o.id&&index==o.index;}
};
struct Entry {Identity identity;bool known=false,active=false,blocked=false;uint8_t original=0,display=0;};
class Model {
 std::array<Entry,32>entries_{};uint32_t epoch_=0;int frame_=-1;
 constexpr Entry* entry(const Identity&i){if(!i.valid()||i.epoch!=epoch_||frame_<0)return nullptr;
  auto&e=entries_[i.index];if(!(e.identity==i))e=Entry{i};return &e;}
public:
 constexpr void clear(){entries_={};epoch_=0;frame_=-1;}
 constexpr void frame(uint32_t epoch,int frame){if(epoch!=epoch_){entries_={};epoch_=epoch;}
  else if(frame<frame_)for(auto&e:entries_){e.known=e.active=false;e.blocked=true;}frame_=frame;}
 constexpr bool beginTool(const Identity&i,uint8_t before,uint8_t display){auto*e=entry(i);
  if(!e||e->blocked||before>1||display>1)return false;
  if(!e->known){e->original=before;e->known=true;}e->display=display;e->active=true;return true;}
 constexpr void naturalWrite(const Identity&i,uint8_t value){auto*e=entry(i);if(!e)return;
  if(value>1){e->known=e->active=false;e->blocked=true;return;}
  e->original=e->display=value;e->known=true;e->blocked=false;}
 constexpr void unknownWrite(const Identity&i){if(auto*e=entry(i)){e->known=e->active=false;e->blocked=true;}}
 constexpr void invalidatePointer(uint32_t p){for(auto&e:entries_)if(e.identity.pointer==p)e=Entry{};}
 constexpr const Entry*find(const Identity&i)const{if(!i.valid()||i.epoch!=epoch_||frame_<0)return nullptr;
  const auto&e=entries_[i.index];return e.identity==i?&e:nullptr;}
 constexpr uint8_t crcValue(const Identity&i,uint8_t live)const{const auto*e=find(i);
  return e&&e->known&&e->active&&!e->blocked&&live<=1&&live==e->display?e->original:live;}
 constexpr const auto&entries()const{return entries_;}
};
struct Site {uint32_t address;uint8_t size,original[7];};
inline constexpr Site Sites[]={
 {0x64DCCA,6,{0x8A,0x99,0x41,0x02,0x00,0x00}},
 {0x4F576B,6,{0x88,0x9D,0x41,0x02,0x00,0x00}},
 {0x4FC328,7,{0xC6,0x81,0x41,0x02,0x00,0x00,0x01}},
 {0x577ACA,6,{0x88,0x99,0x41,0x02,0x00,0x00}},
 {0x577BC7,7,{0xC6,0x81,0x41,0x02,0x00,0x00,0x00}},
 {0x577CA7,7,{0xC6,0x81,0x41,0x02,0x00,0x00,0x00}},
 {0x577DB0,6,{0x88,0x99,0x41,0x02,0x00,0x00}},
 {0x577F48,7,{0xC6,0x81,0x41,0x02,0x00,0x00,0x01}}};
inline const Site*findSite(uint32_t at){for(const auto&s:Sites)if(s.address==at)return &s;return nullptr;}
constexpr unsigned SiteCount=sizeof(Sites)/sizeof(Sites[0]);
}}
