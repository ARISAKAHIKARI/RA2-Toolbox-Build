#pragma once
#include "overlay_composition.h"
#include <cstdint>
#include <cstring>
#include <chrono>
namespace overlaypublication {
// Revisions/new pixels wake immediately, and upload deadlines retain their
// own high-refresh cadence. Only focus/position/TTL housekeeping polls here.
constexpr uint32_t PollMilliseconds=16,RepairMilliseconds=1000;
inline std::chrono::steady_clock::duration pollDelay(std::chrono::steady_clock::time_point now,std::chrono::steady_clock::time_point uploadAt,bool scheduled){
 auto delay=std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::milliseconds(PollMilliseconds));
 return scheduled?std::min(delay,std::max(std::chrono::steady_clock::duration::zero(),uploadAt-now)):delay;
}
// Observed revision controls wakeups; successful revision belongs to the
// caller. A failed upload must retry without making the wait predicate spin.
class Submission {
 uint64_t observed_=~uint64_t(0),successAt_=0;bool successful_=false,pending_=false;
public:
 bool changed(uint64_t revision)const{return revision!=observed_;}
 void observe(uint64_t revision){observed_=revision;}
 bool repair(uint64_t now)const{return !successful_||now-successAt_>=RepairMilliseconds;}
 bool needs(uint64_t now,bool changed,bool visible,bool lost)const{return pending_||changed||!visible||lost||repair(now);}
 void failed(){pending_=true;}
 void defer(){pending_=true;}
 void completed(uint64_t now,bool success){pending_=!success;if(success){successful_=true;successAt_=now;}}
 bool pending()const{return pending_;}
};
// Portable part of the real CPU publication path. Backing supplies Win32's
// DIB resize/bits boundary; tests use the same composition and copies with a
// counted backing. A failed upload requests a complete retry, never a commit.
class CpuStage {
 overlaycomposition::Incremental composition_;overlaycomposition::Context context_{};
 bool fullCopy_=true,dirty_=false;size_t copied_=0;
public:
 void reset(){composition_={};context_={};fullCopy_=true;dirty_=false;copied_=0;}
 void lost(){fullCopy_=true;}
 bool lostContent()const{return fullCopy_;}
 bool needsCopy()const{return fullCopy_||dirty_;}
 size_t copiedPixels()const{return copied_;}
 template<class Backing>bool compose(const overlaycomposition::Context&context,std::vector<overlaycomposition::Surface>layers,Backing&dib){
  const bool resized=dib.w!=context.width||dib.h!=context.height||!dib.dc;
  if(!composition_.update(context,std::move(layers))||!dib.resize(context.width,context.height)){lost();return false;}
  context_=context;fullCopy_|=resized;dirty_=!composition_.dirtyRegions().empty();return true;
 }
 bool needsUpload(bool forced)const{return forced||fullCopy_||dirty_;}
 template<class Backing>bool copy(Backing&dib){
  copied_=0;if(!dib.bits||dib.w!=context_.width||dib.h!=context_.height){lost();return false;}
  const auto&output=composition_.pixels();
  auto region=[&](const overlaycomposition::Rect&r){for(int y=r.y;y<r.y+r.h;++y)std::memcpy(static_cast<uint32_t*>(dib.bits)+size_t(y)*context_.width+r.x,output.data()+size_t(y)*context_.width+r.x,size_t(r.w)*4);copied_+=size_t(r.w)*r.h;};
  if(fullCopy_)region({0,0,context_.width,context_.height});else if(dirty_)for(const auto&r:composition_.dirtyRegions())region(r);
  fullCopy_=dirty_=false;return true;
 }
};
}
