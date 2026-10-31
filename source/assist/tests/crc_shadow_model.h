#pragma once
#include <array>
#include <cstdint>
// Research model ONLY. No production includes, game-memory writes or hooks.
// Natural writes must be supplied by proven instrumentation. Polling the live
// overridden value cannot identify a natural write of the same value.
namespace crcshadowresearch {
struct Identity {
 uint32_t epoch=0,pointer=0,id=0;unsigned index=32;
 bool valid()const{return epoch&&pointer&&id&&index<32;}
 bool operator==(const Identity&o)const{return epoch==o.epoch&&pointer==o.pointer&&id==o.id&&index==o.index;}
};
class Model {
 struct Entry {Identity identity;bool known=false,active=false,blocked=false;uint8_t original=0,display=0;};
 std::array<Entry,32>entries_{};uint32_t epoch_=0;int frame_=-1;
 Entry* entry(const Identity&i){
  if(!i.valid()||i.epoch!=epoch_||frame_<0)return nullptr;
  auto&e=entries_[i.index];if(!(e.identity==i))e=Entry{i};return &e;
 }
public:
 void frame(uint32_t epoch,int frame){
  if(epoch!=epoch_){entries_={};epoch_=epoch;}
  else if(frame<frame_)for(auto&e:entries_){e.known=e.active=false;e.blocked=true;}
  frame_=frame;
 }
 bool beginTool(const Identity&i,uint8_t before,uint8_t display){
  auto*e=entry(i);if(!e||e->blocked||before>1||display>1)return false;
  if(!e->known){e->original=before;e->known=true;} // Capture once, never from an already overridden repeat.
  e->display=display;e->active=true;return true;
 }
 void naturalWrite(const Identity&i,uint8_t value){
  auto*e=entry(i);if(!e)return;
  if(value>1){e->known=e->active=false;e->blocked=true;return;}
  e->original=value;e->known=true;e->blocked=false;
  // The test models separate display data; the actual native write still wins.
 }
 void unknownWrite(const Identity&i){if(auto*e=entry(i)){e->known=e->active=false;e->blocked=true;}}
 uint8_t crcValue(const Identity&i,uint8_t live)const{
  if(!i.valid()||i.epoch!=epoch_||frame_<0||live>1)return live;const auto&e=entries_[i.index];
  return e.identity==i&&e.known&&e.active&&!e.blocked?e.original:live;
 }
};
}
