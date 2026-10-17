#include "../source/type_hotkey.h"
#include <cassert>
#include <initializer_list>
#include <string>
#include <vector>
int main(){
 struct Binding{std::string id;uint16_t key;};struct Type{const char*id;int kind;};
 std::vector<Binding> bindings={{"HTNK",0x641},{"E1",0x641},{"ORCA",0x642},{"MISSING",0x641},{"HTNK",0xe41},{"POWER",0x641}};
 Type types[]={{"HTNK",40},{"E1",16},{"ORCA",3},{"POWER",7}};uint8_t mask[8]{};
 assert(typekeys::firstForKey(bindings,0)&&!typekeys::firstForKey(bindings,1)&&typekeys::firstForKey(bindings,2));
 assert(!typekeys::firstForKey(bindings,4));
 assert(typekeys::groupMask(bindings,0x641,types,4,mask,8)==2&&mask[0]&&mask[1]&&!mask[2]&&!mask[3]&&!mask[7]);
 assert(typekeys::groupMask(bindings,0x642,types,4,mask,8)==1&&!mask[0]&&!mask[1]&&mask[2]);
 assert(typekeys::groupMask(bindings,0,types,4,mask,8)==0&&!mask[2]);
 bindings[1].key=0;assert(typekeys::groupMask(bindings,0x641,types,4,mask,8)==1);
 typekeys::TypeSet set;for(unsigned i=1;i<=512;++i)set.add(0x10000+i*16384);
 for(unsigned i=1;i<=512;++i)assert(set.contains(0x10000+i*16384));
 assert(!set.contains(0)&&!set.contains(0x12345));set.clear();assert(!set.contains(0x14000));
 typekeys::Tap t;
 assert(!t.press(1100,1000,1,350));assert(t.press(1100,1200,1,350));
 assert(!t.press(1100,1300,1,350)); // third press starts a new pair
 assert(!t.press(1101,1400,1,350));assert(!t.press(1100,1450,1,350));
 assert(!t.press(1100,1900,1,350));assert(!t.press(1100,1950,2,350));
 t.clear();assert(!t.press(1100,0xFFFFFFF0,2,350));assert(t.press(1100,50,2,350));
 assert(typekeys::inView(0,0,640,480));assert(typekeys::inView(639,479,640,480));
 assert(!typekeys::inView(640,100,640,480));assert(!typekeys::inView(-1,0,640,480));
 assert(typekeys::csMission(2,2,true,true,true,true));
 for(int m:{1,7,11,13,29})assert(!typekeys::csMission(m,2,true,true,true,true));
 assert(!typekeys::csMission(2,0,true,true,true,true));assert(!typekeys::csMission(2,3,true,true,true,true));
 assert(!typekeys::csMission(2,2,false,true,true,true));assert(!typekeys::csMission(2,2,true,false,true,true));
 assert(!typekeys::csMission(2,2,true,true,false,true));assert(!typekeys::csMission(2,2,true,true,true,false));
}
