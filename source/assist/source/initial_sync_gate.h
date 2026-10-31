#pragma once
#include <stdint.h>
#include <string.h>
#include "event_delivery.h"

// Read-only observation, not another network protocol or an OOS suppressor.
// A candidate must be seen pending and then executed in the SAME physical
// DoList slot. An old executed slot, elapsed time or an advancing frame alone
// cannot authenticate a new attachment. No engine calls, writes or RNG here.
namespace assist { namespace initialsync {
constexpr uint32_t EventBytes=111,Capacity=16384,Mask=Capacity-1;
constexpr uint32_t Events=0x8B4204,History=0xB04474;
constexpr unsigned ScanBudget=128;
enum Status { Waiting=2, SignatureWaiting=3, Mismatch=4, Unsupported=5, Ready=1 };
struct Identity {
 uint32_t epoch=0,scenario=0,items=0,house=0,id=0;int mode=-1;
 constexpr bool operator==(const Identity&o)const{return epoch==o.epoch&&scenario==o.scenario&&items==o.items&&house==o.house&&id==o.id&&mode==o.mode;}
};
struct Ring {int count,head,tail;};
constexpr bool valid(const Ring&r){return r.count>=0&&r.count<=int(Capacity)&&r.head>=0&&r.head<int(Capacity)&&r.tail>=0&&r.tail<int(Capacity)&&((unsigned(r.head)+unsigned(r.count))&Mask)==unsigned(r.tail);}
struct Header {uint8_t bytes[14]{};
 constexpr uint32_t word(unsigned at)const{return uint32_t(bytes[at])|(uint32_t(bytes[at+1])<<8)|(uint32_t(bytes[at+2])<<16)|(uint32_t(bytes[at+3])<<24);}
 constexpr uint32_t frame()const{return word(3);}
 constexpr uint32_t crc()const{return word(7);}
 constexpr unsigned house()const{return bytes[2];}
 constexpr bool executed()const{return (bytes[1]&1)!=0;}
 constexpr bool frameInfo()const{return bytes[0]==0x1c;}
 constexpr bool sameEvent(const Header&o)const{for(unsigned i=0;i<14;++i)if(i!=1&&bytes[i]!=o.bytes[i])return false;return true;}
};
struct Pending {bool active=false;uint32_t slot=0;Header header{};};
struct Gate {
 Identity identity{};Pending pending[32]{};uint32_t peers=0,confirmed=0,cursor=0;
 int firstFrame=-1,lastFrame=-1,state=Waiting;bool bound=false,ready=false;
 constexpr void reset(){*this=Gate{};}
 constexpr bool hasProof(const Identity&next,int frame)const{return bound&&ready&&identity==next&&frame>=lastFrame;}
 constexpr bool eligible(const Header&h,uint32_t peerMask,int frame)const{
  if(!h.frameInfo()||h.house()>=32||!(peerMask&(1u<<h.house())))return false;
  const int64_t comparison=int64_t(h.frame())-h.bytes[13];
  return h.frame()<0x7fffffffu&&comparison>firstFrame&&int64_t(frame)-comparison<256;
 }
 template<class Read> constexpr bool update(const Identity&next,int frame,uint32_t peerMask,bool outOfSync,Read read){
  if(!eventdelivery::isMultiplayer(next.mode)){reset();state=eventdelivery::isSingleplayer(next.mode)?Ready:Unsupported;return state==Ready;}
  if(frame<0||!next.epoch||!next.scenario||!next.items||!next.house||!next.id){reset();return false;}
  if(!bound||!(identity==next)||frame<lastFrame){reset();bound=true;identity=next;firstFrame=frame;}
  lastFrame=frame;
  if(outOfSync||state==Mismatch){ready=false;state=Mismatch;return false;}
  // Added peers require their own proof; vanished peers cannot retain a
  // pending candidate. A solo multiplayer/AI session has nobody to compare.
  confirmed&=peerMask;for(unsigned i=0;i<32;++i)if(!(peerMask&(1u<<i)))pending[i]={};peers=peerMask;
  if(!peerMask){ready=true;state=Ready;return true;}
  if((confirmed&peerMask)==peerMask){ready=true;state=Ready;return true;}
  ready=false;state=Waiting;
  for(unsigned i=0;i<32;++i){auto&p=pending[i];if(!p.active)continue;
   Header now{};
   if(!read(Events+p.slot*EventBytes,&now,sizeof now)||!now.sameEvent(p.header)||!eligible(now,peerMask,frame)){p={};continue;}
   if(!now.executed()||int64_t(now.frame())>=frame)continue;
   const uint32_t compared=now.frame()-now.bytes[13];uint32_t local=0;
   if(!read(History+(compared&255u)*4,&local,4))continue;
   if(local!=now.crc()){p={};state=Mismatch;return false;}
   confirmed|=1u<<i;p={};
  }
  if((confirmed&peerMask)==peerMask){ready=true;state=Ready;return true;}
  Ring ring{};if(!read(eventdelivery::DoListAddress,&ring,sizeof ring)||!valid(ring)||!ring.count)return false;
  const unsigned count=unsigned(ring.count),n=count<ScanBudget?count:ScanBudget;cursor%=count;
  for(unsigned j=0;j<n;++j){const unsigned slot=(unsigned(ring.head)+(cursor+j)%count)&Mask;Header h{};
   if(!read(Events+slot*EventBytes,&h,sizeof h))continue;
   if(!h.executed()&&eligible(h,peerMask,frame)&&!pending[h.house()].active)pending[h.house()]={true,slot,h};
  }
  cursor=(cursor+n)%count;return false;
 }
};
}}
