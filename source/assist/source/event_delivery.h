#pragma once
#include <stdint.h>
// One command gateway selects transport centrally. MO Enhancement OrderUtil:
// verified SP modes use DoList; LAN/Internet use OutList and engine transport.
// No feature receives a queue address or independently selects a fallback.
namespace assist { namespace eventdelivery {
constexpr int SingleCapacity=16384,SingleMask=16383;
constexpr int OutListCapacity=128,OutListMask=127,OutListSafeLimit=112;
constexpr uint32_t OutListAddress=0x00A802C8u,DoListAddress=0x008B41F8u,DoListTimingsAddress=0x00A70204u;
enum class Lane : uint8_t { Unsupported, Single, Multiplayer };
// Explicitly verified vanilla modes only. Additional launcher modes require
// independent runtime ABI validation; never guess that they are singleplayer.
inline Lane classify(int mode){
 if(mode==0||mode==5)return Lane::Single;
 if(mode==3||mode==4)return Lane::Multiplayer;
 return Lane::Unsupported;
}
inline bool isMultiplayer(int mode){return classify(mode)==Lane::Multiplayer;}
inline bool isSingleplayer(int mode){return classify(mode)==Lane::Single;}
inline bool mayQueue(int mode){return classify(mode)!=Lane::Unsupported;}
inline bool unifiedOutList(int mode){return isMultiplayer(mode);}
inline bool outListRoom(int count,int tail){
 return count>=0&&count<OutListSafeLimit&&tail>=0&&tail<OutListCapacity;
}
inline bool singleRoom(int count,int next){
 return count>=0&&count<SingleCapacity&&next>=0;
}
inline void prepareSingle(unsigned char& executed,char& house,uint32_t& frame,
 char localHouse,int currentFrame){
 executed=static_cast<unsigned char>(executed&0xFEu);
 house=localHouse;frame=static_cast<uint32_t>(currentFrame)+1u;
}
}} // assist::eventdelivery
