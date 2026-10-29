#pragma once
#include <stdint.h>
#include <stddef.h>
// Private host-to-helper IPC. GameRuntime's transport and game pointers stay
// unchanged; no native-size pointer or HANDLE is placed in this packet.
namespace runtimebroker {
constexpr uint32_t Magic=0x52423236,Version=1;
enum Operation:uint32_t {Describe=0,Install=1,Remove=2,Shutdown=3};
struct Packet {
 uint32_t magic=Magic,version=Version,bytes=sizeof(Packet),owner=0;
 volatile int32_t request=0,response=0,state=0;
 uint32_t operation=Describe,pid=0,thread=0,window=0,token=0,error=0;
 uint32_t runtimeVersion=0,runtimeBytes=0,entryVersion=0,entryBytes=0;
 uint32_t assistVersion=0,assistBytes=0,runtimeBase=0,hookRva=0;
 uint16_t mappingPrefix[32]{};
};
static_assert(sizeof(Packet)==148&&offsetof(Packet,mappingPrefix)==84,"Broker transport ABI");
}
