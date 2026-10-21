#include "../source/native_hotkeys.h"
#include <vector>
#include <cassert>
#include <iostream>
int main(){std::vector<uint8_t>b(0xC00000);auto put=[&](unsigned at,unsigned v){std::memcpy(b.data()+at,&v,4);};auto read=[&](unsigned at,void*out,size_t n){if(at<0x10000||at+n>b.size())return false;std::memcpy(out,b.data()+at,n);return true;};
 const char* names[]={"StructureTab","DefenseTab","InfantryTab","UnitTab","PlanningMode"};
 put(0x87F680,0x900000);put(0x87F684,5);put(0x87F688,8);
 for(unsigned i=0;i<5;++i){unsigned object=0x910000+i*32,table=0x920000+i*32,code=0x500000+i*32,name=0x830000+i*32;
 put(0x900000+i*8,0x1000|('A'+i)|(i==4?0x200:0));put(0x900004+i*8,object);put(object,table);put(table+4,code);b[code]=0xB8;put(code+1,name);b[code+5]=0xC3;std::strcpy((char*)b.data()+name,names[i]);}
 auto keys=nativekeys::load(read);uint8_t down[256]{};
 down['C']=1;assert(keys.held(nativekeys::Infantry,down));down[16]=1;assert(!keys.held(nativekeys::Infantry,down));down[16]=0;down['E']=1;assert(!keys.held(nativekeys::Planning,down));down[17]=1;assert(keys.held(nativekeys::Planning,down));
 put(0x87F684,5000);assert(!nativekeys::load(read).held(nativekeys::Planning,down));put(0x87F684,5);b[0x500080]=0xE9;assert(!nativekeys::load(read).held(nativekeys::Planning,down));
 b[0x500080]=0xB8;int header=0;auto torn=[&](unsigned at,void*out,size_t n){bool ok=read(at,out,n);if(at==0x87F680&&++header==2)((uint32_t*)out)[1]=4;return ok;};assert(!nativekeys::load(torn).held(nativekeys::Planning,down));
 std::cout<<"PASS native command names, reassigned keys/modifiers, invalid table, unsupported getter and torn header\n";
}
