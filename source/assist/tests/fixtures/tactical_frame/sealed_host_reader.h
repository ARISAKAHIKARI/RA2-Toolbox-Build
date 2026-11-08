#pragma once
#include "../../../../shared/source/world_frame.h"
#include <memory>
#include <utility>
// Legacy Host business tests supply only synthetic live I/O. They explicitly
// publish a new immutable Snapshot at each simulated game-frame boundary and
// execute the production WorldFrameScope class over this reader adapter.
// host_tactical_frame_sampling additionally executes the actual ProcessReader
// and cross-process transport with the Win32 I/O fixture.
namespace fixtureframe {
template<class Live> class SealedHostReader {
 worldframe::Token active_;bool complete_=true;
public:
 worldframe::Token available;
 void setWorldDemand(uint32_t){}
 worldframe::Token worldFrameToken()const{return active_;}
 bool worldFrameComplete()const{return bool(active_)&&complete_;}
 void markWorldIncomplete(){complete_=false;}
 bool beginWorldFrame(uint32_t match,worldframe::Token token={}){
  endWorldFrame();if(!token)token=available;
  if(!token||!token->sealed()||(match&&token->key.match!=match))return false;
  active_=std::move(token);return true;
 }
 void endWorldFrame(){active_.reset();complete_=true;}
 bool readFresh(uint32_t address,void*out,size_t size){
  if(!active_)return static_cast<Live*>(this)->readLive(address,out,size);
  if(active_->read(address,out,size))return true;complete_=false;return false;
 }
};
}
