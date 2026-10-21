#pragma once
#include <cstdint>
#include <cstring>
#include <array>
// Read-only CommandClass::Hotkeys. Never call a foreign GetName vtable entry.
namespace nativekeys {
enum Command {Structure,Defense,Infantry,Unit,Planning,Count};
struct Bindings {
 std::array<std::array<uint16_t,8>,Count> keys{};
 template<class K> bool held(Command command,const K& down)const {
  for(auto key:keys[command])if(key&&down[key&255]&&bool(down[16])==bool(key&0x100)&&bool(down[17])==bool(key&0x200)&&bool(down[18])==bool(key&0x400))return true;
  return false;
 }
};
template<class R> Bindings load(R read){
 Bindings result;uint32_t header[3]{};
 if(!read(0x87F680,header,sizeof header)||header[0]<0x10000||header[1]>4096||header[2]<header[1]||header[2]>65536)return result;
 const char* names[]={"StructureTab","DefenseTab","InfantryTab","UnitTab","PlanningMode"};
 for(uint32_t i=0;i<header[1];++i){uint32_t node[2]{},vtable=0,method=0,name=0;uint8_t code[6]{};char text[24]{};
  if(!read(header[0]+i*8,node,8)||!(node[0]&255)||(node[0]&0xE800)||node[1]<0x10000)continue;
  if(!read(node[1],&vtable,4)||!read(vtable+4,&method,4)||method<0x400000||method>=0x800000||!read(method,code,6)||code[0]!=0xB8||code[5]!=0xC3)continue;
  std::memcpy(&name,code+1,4);if(name<0x800000||name>0xB00000||!read(name,text,sizeof text))continue;text[23]=0;
  for(int c=0;c<Count;++c)if(!std::strcmp(text,names[c]))for(auto&slot:result.keys[c])if(!slot){slot=uint16_t(node[0]&0x7FF);break;}
 }
 uint32_t after[3]{};if(!read(0x87F680,after,sizeof after)||std::memcmp(header,after,sizeof header))return {};
 return result;
}
}
