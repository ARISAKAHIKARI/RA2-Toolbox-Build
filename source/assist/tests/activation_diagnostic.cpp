#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../source/activation_diagnostic.h"
#include "../source/control_link.h"
#include <cassert>
#include <cwchar>
#include <cstdio>
#include <initializer_list>
using namespace assist;
int main(){
 for(int gate:{0,1,2,3,4,5,6,7,8,9,10,11}){
  assert(controllink::ready({true,true,false,1,gate,4})==(gate==activation::Ready));
  if(gate>=5)assert(activation::message(gate)&&std::wcscmp(activation::name(gate),L"legacy-or-unknown"));
 }
 assert(activation::Signature==3&&activation::Ready==1);
 activation::LogCadence c;assert(c.due(1,1000,false));
 for(unsigned t=1001;t<1250;++t)assert(!c.due(2,t,true));
 assert(c.due(2,1250,true));assert(!c.due(2,6249,true));assert(c.due(2,6250,true));
 assert(!c.due(2,100000,false));assert(c.due(3,100001,false));
 activation::LogCadence wrap;assert(wrap.due(1,0xffffff00,true));assert(!wrap.due(1,200,true));assert(wrap.due(1,5000,true));
 std::puts("PASS: activation reason codes never grant command readiness; changed-state logging coalesces at 250ms and unchanged blocked logs at 5s, including DWORD rollover");
}
