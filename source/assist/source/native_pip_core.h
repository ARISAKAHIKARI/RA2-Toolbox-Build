#pragma once
#include <array>
#include <cstdint>
#include <cstddef>
#include <algorithm>

// Local presentation only. Source reference: MO_ExperienceEnhancements PiP at
// a529eeea932ac778df6b30516fe3b068926833ac. The adapter owns native execution;
// this transaction owns the exact bytes borrowed from the main camera.
namespace nativepip {
constexpr uint32_t SurfaceSlots[]={0x88731C,0x8872FC,0x887314,0x887644,0x87E8A4};
constexpr unsigned CameraOffsets[]={0xB0,0xB4,0xB8,0xBC,0xD64,0xD68,0xD6C,0xD70,0xD74,0xD78};
constexpr uint32_t Bounds=0x886FA0,Cache=0xB0CE28,DirtyCount=0xB0CE88,FullRedraw=0xB0E63C,Selectable=0xB0CEC8;
constexpr unsigned SelectionCapacity=500,VisibleCapacity=800;
constexpr unsigned DirtyCapacity=1024,DirtyStride=20; // YRpp Drawing::DirtyAreaStruct
constexpr int DisplayWidth=288,DisplayHeight=192,RasterWidth=576,RasterHeight=384;
struct Rect {int32_t x=0,y=0,w=0,h=0;};
struct Saved {
 uint32_t tactical=0,surfaces[5]{};int32_t camera[10]{},cache[4]{},selected=0,visible=0,dirty=0,redraw=0;Rect bounds;
 uint8_t zoom[2]{};std::array<uint8_t,SelectionCapacity*12> selection{};
 std::array<uint32_t,VisibleCapacity> cells{};
 std::array<uint8_t,DirtyCapacity*DirtyStride> dirtyItems{};
};
class Transaction {
 Saved saved_{};bool borrowed_=false,captured_=false;
public:
 bool borrowed()const{return borrowed_;}
 const Saved&saved()const{return saved_;}
 template<class IO>bool capture(IO&io,uint32_t tactical){
  if(borrowed_)return false;captured_=false;
  if(tactical<0x10000)return false;
  Saved&v=saved_;v={};v.tactical=tactical;
  auto read=[&](uint32_t p,auto&out){return io.read(p,&out,sizeof out);};
  if(!read(tactical+0xDB0,v.selected)||!read(tactical+0xE0,v.visible)||
     v.selected<0||v.selected>int(SelectionCapacity)||v.visible<0||v.visible>int(VisibleCapacity))return false;
  for(unsigned n=0;n<10;++n)if(!read(tactical+CameraOffsets[n],v.camera[n]))return false;
  for(unsigned n=0;n<5;++n)if(!read(SurfaceSlots[n],v.surfaces[n])||v.surfaces[n]<0x10000)return false;
  if(!read(Bounds,v.bounds)||!read(Cache,v.cache)||!read(tactical+0xD7C,v.zoom)||
     !read(DirtyCount,v.dirty)||!read(FullRedraw,v.redraw)||v.dirty<0||v.dirty>int(DirtyCapacity))return false;
  if(v.dirty){uint32_t items=0;int32_t capacity=0;
   if(!read(0xB0CE7C,items)||!read(0xB0CE80,capacity)||items<0x10000||capacity<v.dirty||
      !io.read(items,v.dirtyItems.data(),size_t(v.dirty)*DirtyStride))return false;
  }
  return captured_=
   (!v.selected||io.read(Selectable,v.selection.data(),size_t(v.selected)*12))&&
   (!v.visible||io.read(tactical+0xE4,v.cells.data(),size_t(v.visible)*4));
 }
 template<class IO>bool enter(IO&io,const uint32_t surfaces[5],int cx,int cy){
  if(borrowed_||!captured_||cx< -134217728||cx>134217728||cy< -134217728||cy>134217728)return false;
  for(unsigned n=0;n<5;++n)if(surfaces[n]<0x10000)return false;
  // Mark before the first write. SEH/C++ failures must restore partial entry.
  borrowed_=true;const uint32_t t=saved_.tactical;const Rect view{0,0,RasterWidth,RasterHeight};
  auto write=[&](uint32_t p,const auto&value){return io.write(p,&value,sizeof value);};
  if(!write(Bounds,view)||!write(Cache,view))return false;
  for(unsigned n=0;n<10;++n){const int32_t value=n<4?(n&1?cy-RasterHeight/2:cx-RasterWidth/2):(n&1?cy:cx);if(!write(t+CameraOffsets[n],value))return false;}
  const uint8_t zoom[2]{};if(!write(t+0xD7C,zoom))return false;
  for(unsigned n=0;n<5;++n)if(!write(SurfaceSlots[n],surfaces[n]))return false;
  const int32_t zero=0,one=1;
  return write(t+0xDB0,zero)&&write(t+0xE0,zero)&&write(DirtyCount,zero)&&write(FullRedraw,one);
 }
 template<class IO>bool restore(IO&io){
  if(!borrowed_)return true;bool ok=true;const auto&v=saved_;
  auto write=[&](uint32_t p,const auto&value){ok=io.write(p,&value,sizeof value)&&ok;};
  // Attempt every restore even if one write fails; retain ownership on failure.
  for(unsigned n=0;n<5;++n)write(SurfaceSlots[n],v.surfaces[n]);
  write(Bounds,v.bounds);write(Cache,v.cache);
  for(unsigned n=0;n<10;++n)write(v.tactical+CameraOffsets[n],v.camera[n]);
  write(v.tactical+0xD7C,v.zoom);
  bool dirtyRestored=true;
  if(v.dirty){uint32_t items=0;int32_t capacity=0;
   // A MOD can append a dirty rectangle during drawing and reallocate the
   // vector. Restore into its CURRENT backing; never resurrect a freed one.
   const bool valid=io.read(0xB0CE7C,&items,4)&&io.read(0xB0CE80,&capacity,4)&&items>=0x10000&&capacity>=v.dirty;
   dirtyRestored=valid&&io.write(items,v.dirtyItems.data(),size_t(v.dirty)*DirtyStride);ok=dirtyRestored&&ok;
  }
  // An invalid/replaced backing cannot safely advertise the saved count.
  // Retain the transaction for retry without publishing count > capacity.
  if(dirtyRestored)write(DirtyCount,v.dirty);write(FullRedraw,v.redraw);
  if(v.selected)ok=io.write(Selectable,v.selection.data(),size_t(v.selected)*12)&&ok;
  if(v.visible)ok=io.write(v.tactical+0xE4,v.cells.data(),size_t(v.visible)*4)&&ok;
  write(v.tactical+0xDB0,v.selected);write(v.tactical+0xE0,v.visible);
  if(ok)borrowed_=captured_=false;return ok;
 }
};
// A native render call is synchronous and cannot be preempted. Measure its
// completed cost and defer the NEXT one; never promise a hard time limit.
class Cadence {
 uint32_t next_=0;int frame_=-1;bool have_=false;
public:
 void reset(){next_=0;frame_=-1;have_=false;}
 bool due(uint32_t now,int frame,bool force=false)const{return !have_||force||(frame!=frame_&&int32_t(now-next_)>=0);}
 void completed(uint32_t now,int frame,uint32_t micros){frame_=frame;have_=true;next_=now+std::clamp<uint32_t>((micros+99)/100,17,250);}
};
}
