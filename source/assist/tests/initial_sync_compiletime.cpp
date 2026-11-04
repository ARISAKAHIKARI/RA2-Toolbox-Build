#include "../source/initial_sync_gate.h"
#include <type_traits>
using namespace assist::initialsync;
constexpr Identity identity(unsigned epoch=1){return {epoch,0x20000000,0x20001000,0x20002000,100,3};}
constexpr Header packet(unsigned house,unsigned frame,unsigned delay,unsigned crc,bool executed=false){
 Header h{};h.bytes[0]=0x1c;h.bytes[1]=executed;h.bytes[2]=uint8_t(house);h.bytes[13]=uint8_t(delay);
 for(unsigned i=0;i<4;++i){h.bytes[3+i]=uint8_t(frame>>(i*8));h.bytes[7+i]=uint8_t(crc>>(i*8));}return h;
}
struct Read {
 Ring ring{1,0,1};Header a{},b{};uint32_t crc=7;bool readable=true;
 template<class T> constexpr bool operator()(uint32_t at,T*out,unsigned long long)const{
  if(!readable)return false;
  if constexpr(std::is_same<T,Ring>::value){if(at!=assist::eventdelivery::DoListAddress)return false;*out=ring;return true;}
  else if constexpr(std::is_same<T,Header>::value){
   if(at==Events+uint32_t(ring.head)*EventBytes){*out=a;return true;}
   if(at==Events+((uint32_t(ring.head)+1)&Mask)*EventBytes){*out=b;return true;}
   return false;
  }else if constexpr(std::is_same<T,uint32_t>::value){if(at<History||at>=History+1024)return false;*out=crc;return true;}
  else return false;
 }
};
constexpr bool success(){Gate g;Read m;if(g.update(identity(),10,2,false,m))return false;m.a=packet(1,20,5,7);
 if(g.update(identity(),16,2,false,m))return false;m.a.bytes[1]=1;
 if(g.update(identity(),20,2,false,m))return false;return g.update(identity(),21,2,false,m)&&g.confirmed==2;
}
constexpr bool reject(int kind){Gate g;Read m;g.update(identity(),10,2,false,m);m.a=packet(1,20,5,7);
 if(kind==0)m.a.bytes[1]=1; // old executed slot never observed pending
 if(kind==1)m.a.bytes[2]=0; // local sender
 if(kind==2)m.a.bytes[0]=4; // gameplay event, not FrameInfo
 g.update(identity(),16,2,false,m);m.a.bytes[1]=1;
 if(kind==3)m.a.bytes[7]=8; // slot reused / different CRC
 if(kind==4){m.crc=8;return !g.update(identity(),21,2,false,m)&&g.state==Mismatch;}
 if(kind==5)return !g.update(identity(),271,2,false,m); // overwritten history generation
 if(kind==6)return !g.update(identity(2),21,2,false,m); // new epoch
 if(kind==7)return !g.update(identity(),0,2,false,m); // frame rewind
 if(kind==8){m.readable=false;return !g.update(identity(),21,2,false,m);}
 if(kind==9)return !g.update(identity(),21,2,true,m)&&g.state==Mismatch;
 return !g.update(identity(),21,2,false,m);
}
constexpr bool twoPeers(){Gate g;Read m;m.ring={2,16383,1};g.update(identity(),10,6,false,m);m.a=packet(1,20,5,7);m.b=packet(2,20,5,7);
 g.update(identity(),16,6,false,m);m.a.bytes[1]=1;if(g.update(identity(),21,6,false,m))return false;
 m.b.bytes[1]=1;return g.update(identity(),22,6,false,m)&&g.confirmed==6;
}
static_assert(success(),"Fresh pending/executed remote CRC must unlock after its frame");
static_assert(reject(0),"Old executed slots must not authenticate an attachment");
static_assert(reject(1),"Local packets are not remote proof");
static_assert(reject(2),"Gameplay packets are not sync proof");
static_assert(reject(3),"Reused slots must not authenticate");
static_assert(reject(4),"CRC mismatch must remain visible and blocked");
static_assert(reject(5),"Expired history must not authenticate");
static_assert(reject(6),"Epoch change requires new proof");
static_assert(reject(7),"Frame rewind requires new proof");
static_assert(reject(8),"Unreadable records must not authenticate");
static_assert(reject(9),"Native OOS must block activation");
static_assert(twoPeers(),"Every remote peer must match, including queue wrap");
constexpr bool proofScope(){Gate g;Read m;g.update(identity(),10,2,false,m);m.a=packet(1,20,5,7);g.update(identity(),16,2,false,m);m.a.bytes[1]=1;g.update(identity(),21,2,false,m);
 return g.hasProof(identity(),22)&&!g.hasProof(identity(2),22)&&!g.hasProof(identity(),0);
}
static_assert(proofScope(),"A vanished peer cannot carry proof into a different world or frame rewind");
int main(){return 0;}
