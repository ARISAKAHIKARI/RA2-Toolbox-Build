#include <atomic>
#include "../../shared/source/overlay_compositor.h"
#include <cstdio>
#include <cstdlib>
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
int main(){
 HWND game=CreateWindowExW(0,L"STATIC",L"native feedback compositor test",WS_OVERLAPPEDWINDOW,10,10,160,160,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);CHECK(game);
 overlaycompositor::Host host;auto c=host.configure(uintptr_t(game),GetCurrentProcessId(),1,100,100,0,0,true,true,60);
 for(int n=0;n<200&&!host.window();++n)Sleep(10);CHECK(host.window());
 auto one=std::make_shared<const std::vector<uint32_t>>(1,0xffffffff);
 auto make=[&](uint64_t id,int order){return overlaycompositor::Surface{id,order,c,{0,0,1,1},true,255,one};};
 CHECK(host.publish(make(1,10)));CHECK(host.replaceGroup({3,4,5},{make(3,110),make(4,200),make(5,300)}));CHECK(host.layerCount()==4);
 // Camera preparation must leave the old complete group valid until its
 // translated replacement is ready; reject an invalid proposal atomically.
 auto oldScene=make(3,110);
 auto future=host.configure(uintptr_t(game),GetCurrentProcessId(),1,100,100,12,19,true,false,60,true);
 auto committed=host.capture(uintptr_t(game),GetCurrentProcessId());
 CHECK(future.token==c.token&&future.cameraX==12&&future.cameraY==19);
 CHECK(committed.cameraX==0&&committed.cameraY==0&&overlaycomposition::valid(oldScene,committed));
 auto invalid=future;invalid.token++;
 CHECK(!host.replaceGroupAt(invalid,{3,4,5},{}));CHECK(host.capture(uintptr_t(game),GetCurrentProcessId()).cameraX==0&&host.layerCount()==4);
 c=future;CHECK(host.replaceGroupAt(c,{3,4,5},{make(3,5),make(4,200),make(5,300)}));
 committed=host.capture(uintptr_t(game),GetCurrentProcessId());
 CHECK(committed.cameraX==12&&committed.cameraY==19&&!overlaycomposition::valid(oldScene,committed)&&host.layerCount()==4);
 CHECK(host.replaceGroup({3,4,5},{make(3,110),make(5,300)}));CHECK(host.layerCount()==3);
 CHECK(!host.replaceGroup({3,3},{make(3,110)}));CHECK(host.layerCount()==3);
 auto old=make(5,300);old.context.token++;CHECK(!host.replaceGroup({3,4,5},{make(3,110),old}));CHECK(host.layerCount()==1);
 CHECK(host.replaceGroup({3,4,5},{make(3,110),make(5,300)}));CHECK(host.replaceGroup({3,4,5},{}));CHECK(host.layerCount()==1);
 CHECK((GetWindowLongPtrW(host.window(),GWL_EXSTYLE)&(WS_EX_TRANSPARENT|WS_EX_NOACTIVATE))==(WS_EX_TRANSPARENT|WS_EX_NOACTIVATE));
 c=host.configure(uintptr_t(game),GetCurrentProcessId(),2,100,100,0,0,true,true,60);CHECK(host.layerCount()==0);
 host.shutdown();CHECK(!host.window());DestroyWindow(game);
 std::printf("PASS: %u real-Windows group checks; atomic replace/remove, invalid group withdrawal, independent world layer, passive window and shutdown\n",checks);
}
