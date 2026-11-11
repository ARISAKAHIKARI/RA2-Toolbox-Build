#pragma once
#include "vanilla_constructor_catalog.h"
#include <cstdint>
#include <cstring>

namespace assist::privateout {
enum Mode : uint32_t { Native=0, Hook=1, Breakpoint=2 };
inline constexpr unsigned Capacity=4096, BatchCapacity=24, WireCapacity=4096;
inline constexpr uint32_t BreakpointTags=128u|256u|512u;
// Gameplay commands only. Connection, CRC, timing, chat, save/exit and lobby
// records retain the original network scheduler and never enter this queue.
inline int payloadBytes(unsigned kind){
 switch(kind){
 case 1:case 2:case 6:case 7:case 9:case 10:case 17:case 21:case 22:case 44:return 5;
 case 3:case 13:case 23:case 24:case 45:return 4;
 case 4:return 23;case 5:return 24;
 case 8:case 43:return 0;
 case 11:return 16;
 case 14:case 15:case 16:case 46:return 12;
 case 18:return 8;
 case 30:case 42:return 10;
 default:return -1;
 }
}
inline bool objectKind(unsigned kind){return kind==1||kind==2||kind==6||kind==7||kind==9||kind==10||kind==17||kind==21||kind==22;}
inline uint32_t actor(const vanilla::Packet&e){
 uint32_t id=0;if(e.kind==4||e.kind==5||objectKind(e.kind)||e.kind==30||e.kind==42||e.kind==44)std::memcpy(&id,e.payload,4);return id;
}
inline bool canonical(vanilla::Packet&out,const vanilla::Packet&in){
 const int n=payloadBytes(in.kind);if(n<0||int(in.house)<0||int(in.house)>=32)return false;
 out={};out.kind=in.kind;out.house=in.house;out.frame=in.frame;
 if(n)std::memcpy(out.payload,in.payload,unsigned(n));
 if(out.kind==4)out.payload[6]=0; // dictionary slot metadata, never user data
 return true;
}
struct Stamp {uint32_t epoch=0,house=0,scenario=0,at=0,actorPointer=0,actorID=0,owner=0,ownerID=0,revision=0,targetHouse=0,targetID=0;};
struct Pending {vanilla::Packet event{};Stamp stamp{};bool manual=false;};
struct Queue {
 Pending entries[Capacity]{};unsigned head=0,count=0;
 bool push(const Pending&p){if(count==Capacity)return false;entries[(head+count)%Capacity]=p;++count;return true;}
 Pending&at(unsigned n){return entries[(head+n)%Capacity];}
 const Pending&at(unsigned n)const{return entries[(head+n)%Capacity];}
 bool pop(unsigned n=1){if(n>count)return false;head=(head+n)%Capacity;count-=n;if(!count)head=0;return true;}
 void clear(){head=count=0;}
};
struct Wire {
 uint8_t bytes[WireCapacity]{};unsigned size=0,count=0;bool mega=false;
 vanilla::Packet local[BatchCapacity]{};
};
inline bool encode(Wire&out,const vanilla::Packet*events,unsigned count,int protocol,
 uint32_t frame,unsigned delay,unsigned interval,uint32_t crc,uint16_t serial){
 out={};if(!events||!count||count>BatchCapacity||protocol<0||protocol>2||
  !delay||delay>255||!interval||interval>255||frame>uint32_t(INT32_MAX)-delay)return false;
 uint32_t execute=frame+delay;
 if(protocol==2){if(frame%interval||delay%interval)return false;execute=((execute+interval-1)/interval)*interval;}
 if(execute>uint32_t(INT32_MAX))return false;
 vanilla::Packet header{};header.kind=0x1C;header.house=events[0].house;header.frame=execute;
 std::memcpy(header.payload,&crc,4);std::memcpy(header.payload+4,&serial,2);header.payload[6]=uint8_t(delay);
 const unsigned headerBytes=protocol?14:111;
 std::memcpy(out.bytes,&header,headerBytes);out.size=headerBytes;
 for(unsigned i=0;i<count;++i){
  auto&e=out.local[i];if(events[i].house!=header.house||!canonical(e,events[i]))return false;e.frame=execute;
  const unsigned payload=unsigned(payloadBytes(e.kind));out.mega|=e.kind==4;
  const unsigned need=protocol?1+payload+(e.kind==4?1:0):111;
  if(need>WireCapacity-out.size)return false;
  auto*p=out.bytes+out.size;
  if(!protocol)std::memcpy(p,&e,111);
  else if(e.kind==4){
   // A full one-actor group, slot zero, never a cached dictionary reference.
   // The caller invalidates its native send cache after transport acceptance.
   p[0]=4;p[1]=1;std::memcpy(p+2,e.payload,23);
  }else{p[0]=e.kind;if(payload)std::memcpy(p+1,e.payload,payload);}
  out.size+=need;
 }
 out.count=count;return true;
}
inline const wchar_t* statusText(int status){
 switch(status){
 case 1:return L"已接管";case 2:return L"等待原生待发队列排空";
 case 3:return L"等待游戏线程";case -1:return L"命令入口不兼容";
 case -2:return L"硬件断点槽位不足（需要 3 槽）";
 case -3:return L"入口冲突或安装失败";case -4:return L"正在恢复原实现";
 case -5:return L"传输接口不兼容";case -6:return L"私有队列已满";
 case -7:return L"命令已失效或取消";case -8:return L"发送失败，等待重试";
 case -9:return L"跨所属方任务不支持私有压缩字典";
 default:return L"原生实现";
 }
}
}
