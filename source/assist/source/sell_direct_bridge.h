#pragma once
// Explicit manual Sell only. This private list is NOT native OutList. The
// packet is encoded here and handed directly to the game's active transport.
struct DirectSell {intent::EventOrder order{};OrderAuthority authority=OrderAuthority::Local;uint32_t object=0,owner=0,ownerID=0,epoch=0,revision=0,at=0,retryAt=0;};
static DirectSell directSells[16];static unsigned directSellCount=0;
static bool directSellPending(){return directSellCount!=0;}
static void resetDirectSell(){directSellCount=0;if(sh)sh->sellDirectPending=0;}
static bool queueDirectSell(const intent::EventOrder&order,OrderAuthority authority,uint32_t object){
 if(!sh||!simulationReady()||snap.chat||!eventdelivery::isMultiplayer(get<int>(0xA8B238))||directSellCount>=16||
  order.domain!=vanilla::Domain::Object||order.eventKind!=0x16||!object||!live(object)||!sellAllowed(object)||!authorizePlayerOrder(order,authority))return false;
 const uint32_t owner=get<uint32_t>(object+0x21C);
 if(!sellHumanHouse(owner)||get<int>(owner+0x30)!=order.player)return false;
 for(unsigned i=0;i<directSellCount;++i)if(directSells[i].object==object&&directSells[i].order.a==order.a)return false;
 directSells[directSellCount++]={order,authority,object,owner,get<uint32_t>(owner+0x10),match,manualVersions.get(uint32_t(order.a)),sampleClock(sh)};
 sh->sellDirectPending=directSellCount;sh->sellDirectStatus=2;return true;
}
static void serviceDirectSell(){
 if(!directSellCount||!sh)return;
 if(!simulationReady()||snap.chat)return;
 const auto request=directSells[0];const uint32_t object=worldByID(uint32_t(request.order.a));
 auto pop=[&](int status){for(unsigned i=1;i<directSellCount;++i)directSells[i-1]=directSells[i];--directSellCount;sh->sellDirectPending=directSellCount;sh->sellDirectStatus=status;if(status<0)++sh->sellDirectRejected;};
 if(!eventdelivery::isMultiplayer(get<int>(0xA8B238))||request.epoch!=match||DWORD(sampleClock(sh)-request.at)>3000||object!=request.object||!object||!live(object)||get<uint32_t>(object+0x10)!=uint32_t(request.order.a)||!sellAllowed(object)||
  get<uint32_t>(object+0x21C)!=request.owner||get<uint32_t>(request.owner+0x10)!=request.ownerID||!sellHumanHouse(request.owner)||
  manualVersions.get(uint32_t(request.order.a))!=request.revision||!authorizePlayerOrder(request.order,request.authority)){pop(-7);return;}
 if(request.retryAt&&int32_t(sampleClock(sh)-request.retryAt)<0)return;
 const int protocol=get<int>(0xA8B24C),frame=get<int>(0xA8ED84),delay=get<int>(0xA8B550),interval=get<int>(0xA8B554);
 if(protocol<0||protocol>2||delay<1||delay>255||interval<1||interval>255){pop(-2);return;}
 // Use the existing global CRC only after the native per-frame history has
 // published it. Never recompute CRC (that advances the scenario RNG).
 const uint32_t crc=get<uint32_t>(0xAC51FC);
 if(get<uint32_t>(0xB04474+(uint32_t(frame)&255)*4)!=crc||(protocol==2&&frame%interval))return;
 sellpacket::Wire wire;const uint16_t serial=get<uint16_t>(0xAFA400);
 if(!sellpacket::encode(wire,protocol,request.order.player,uint32_t(request.order.a),uint32_t(frame),unsigned(delay),unsigned(interval),crc,serial)){pop(-2);return;}
 constexpr uint32_t transport=0xA8E9C0;const uint32_t table=get<uint32_t>(transport);
 const uint32_t send=get<uint32_t>(table+8),flush=get<uint32_t>(table+4);
 // This is the native transport ABI used by both LAN/Internet. Its current
 // lower-level connection objects retain platform tunnelling and reliability.
 if(table!=0x7EC16C||send!=0x5414C0||flush!=0x541820||!nativeExecutableRange(send,1)||!nativeExecutableRange(flush,1)||
  !get<uint8_t>(transport+4)||!get<uint8_t>(transport+5)||get<int>(transport+0x44)<1||get<int>(transport+0x44)>8){pop(-8);return;}
 const int count=get<int>(0x8B41F8),tail=get<int>(0x8B4200);
 if(!eventdelivery::singleRoom(count,tail)||tail>=16384||outgoingBudget<=0)return;
 uint32_t timing=0;if(!gameSendTiming(timing))return;
 if(!simulationReady()||get<int>(0xA8ED84)!=frame||get<uint16_t>(0xAFA400)!=serial||get<int>(0x8B41F8)!=count||get<int>(0x8B4200)!=tail||get<int>(0xA8B24C)!=protocol||get<int>(0xA8B550)!=delay||get<int>(0xA8B554)!=interval||get<uint32_t>(0xAC51FC)!=crc||
  get<uint32_t>(request.owner+0x10)!=request.ownerID||get<uint32_t>(object+0x10)!=uint32_t(request.order.a)||!sellHumanHouse(request.owner)||
  manualVersions.get(uint32_t(request.order.a))!=request.revision||!sellAllowed(object)||!authorizePlayerOrder(request.order,request.authority)||worldByID(uint32_t(request.order.a))!=object||get<uint32_t>(object+0x21C)!=request.owner){pop(-7);return;}
 // The native Send copies to reliable per-peer queues synchronously. No
 // native event constructor, ClickedEvent, OutList or native packer is called.
 const bool sent=call<int>(send,transport,wire.bytes,int(wire.size),1,-1)!=0;
 if(!sent){directSells[0].retryAt=sampleClock(sh)+50;sh->sellDirectStatus=2;return;}
 // Append the identical local event before Flush can receive peer traffic.
 memcpy(reinterpret_cast<void*>(uintptr_t(0x8B4204+unsigned(tail)*sizeof(Event))),&wire.local,sizeof(Event));
 put<uint32_t>(0xA70204+unsigned(tail)*4,timing);put<int>(0x8B4200,(tail+1)&16383);put<int>(0x8B41F8,count+1);
 if(protocol)put<uint16_t>(0xAFA400,uint16_t(serial+1));
 put<uint32_t>(0xB0487C,get<uint32_t>(0xB0487C)+1);--outgoingBudget;++snap.queueSent;++snap.queuedEvents;++sh->sellDirectSent;
 pop(1); // Retire before Flush; never retry a transport-accepted command.
 call<void>(flush,transport);
}
