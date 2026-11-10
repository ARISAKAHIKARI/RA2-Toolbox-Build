#pragma once
#include "vanilla_constructor_catalog.h"
#include <cstring>
namespace assist::sellpacket {
inline bool event(vanilla::Packet&out,int house,uint32_t id,uint32_t frame){
 if(house<0||house>=32||!id||id>INT32_MAX)return false;
 out={};out.kind=0x16;out.house=char(house);out.frame=frame;
 std::memcpy(out.payload,&id,4);out.payload[4]=0x34;return true;
}
struct Wire {uint8_t bytes[222]{};unsigned size=0;vanilla::Packet local{};};
inline bool encode(Wire&out,int protocol,int house,uint32_t id,uint32_t frame,unsigned delay,unsigned interval,uint32_t crc,uint16_t serial){
 out={};if(protocol<0||protocol>2||delay<1||delay>255||frame>INT32_MAX-delay||!interval||interval>255)return false;
 uint32_t execute=frame+delay;
 if(protocol==2){if(frame%interval||delay%interval)return false;execute=((execute+interval-1)/interval)*interval;}
 vanilla::Packet header{};header.kind=0x1C;header.house=char(house);header.frame=execute;
 std::memcpy(header.payload,&crc,4);std::memcpy(header.payload+4,&serial,2);header.payload[6]=uint8_t(delay);
 if(!event(out.local,house,id,execute))return false;
 if(!protocol){std::memcpy(out.bytes,&header,111);std::memcpy(out.bytes+111,&out.local,111);out.size=222;}
 else{std::memcpy(out.bytes,&header,14);out.bytes[14]=0x16;std::memcpy(out.bytes+15,out.local.payload,5);out.size=20;}
 return true;
}
}
