#include "../../shared/source/overlay_composition.h"
#include <cassert>
#include <chrono>
#include <climits>
#include <cstdio>
using namespace overlaycomposition;
int main(){
 Context c{1,101,22,3,80,60,100,200};
 auto ground=std::make_shared<const std::vector<uint32_t>>(100*80,0xff123456);
 auto hud=std::make_shared<const std::vector<uint32_t>>(20*12,0x80800040);
 auto pointer=std::make_shared<const std::vector<uint32_t>>(4*4,0xffffffff);
 Surface scene{3,5,c,{-10,-10,100,80},true,255,ground,{5,4,60,50}};
 Surface information{1,10,c,{8,8,20,12},true,200,hud};
 Surface cursor{8,300,c,{10,10,4,4},false,255,pointer};
 Incremental cache;std::vector<uint32_t> expected;
 auto verify=[&](std::vector<Surface>layers){assert(compose(c,layers,expected));assert(cache.update(c,std::move(layers)));assert(cache.pixels()==expected);};
 verify({scene,information,cursor});assert(expected[0]==0&&expected[4*80+5]==0xff123456);
 verify({scene,information,cursor});assert(cache.dirty()==Rect{});
 verify({scene,information,information,cursor});verify({scene,information,cursor}); // defensive duplicate-id equivalence
 auto holes=std::make_shared<const std::vector<Rect>>(std::vector<Rect>{{20,20,4,1},{18,20,4,1},{20,21,2,1}});
 scene.cutouts=holes;verify({scene,information,cursor});
 // Holes are local to this layer: the higher cursor/HUD still draw, and the
 // transparent area outside a HUD reveals the native surface below the HWND.
 assert(expected[10*80+10]==0xffffffff);
 scene.clip={0,0,80,60};information.rect={40,40,20,12};cursor.rect={50,45,4,4};
 verify({scene,information,cursor});assert(expected[10*80+9]==0&&expected[11*80+10]==0);
 assert(expected[10*80+8]==0&&expected[10*80+14]==0xff123456);
 for(int n=0;n<160;++n){
  cursor.rect.x=(n*7)%95-10;cursor.rect.y=(n*11)%75-10;
  information.opacity=uint8_t(n*29);information.order=n%3?10:400;
  scene.clip={n%9-3,n%7-2,70-n%11,55-n%13};
  if(n%5==0)scene.cutouts=std::make_shared<const std::vector<Rect>>(std::vector<Rect>{{15+n%5,20+n%7,10,1}});
  if(n%17==0)scene.cutouts.reset();
  if(n%23==0){++c.token;scene.context=information.context=cursor.context=c;}
  if(n%13==0)verify({information,cursor});else if(n%19==0)verify({scene,cursor});else verify({scene,information,cursor});
 }
 // Invalid/overflowing clips, cutouts and coordinates cannot corrupt output.
 scene.clip={INT_MAX,INT_MAX,INT_MAX,INT_MAX};verify({scene});
 scene.clip={};scene.rect.x=INT_MIN;verify({scene});scene.rect.x=-10;
 scene.cutouts=std::make_shared<const std::vector<Rect>>(std::vector<Rect>{{0,0,1,2}});assert(!valid(scene,c));verify({scene});
 scene.cutouts=std::make_shared<const std::vector<Rect>>(std::vector<Rect>{{99,0,2,1}});assert(!valid(scene,c));verify({scene});
 scene.cutouts=std::make_shared<const std::vector<Rect>>(std::vector<Rect>(16385,{0,0,1,1}));assert(!valid(scene,c));verify({scene});
 // Only feedback moves on a 4K static background. The changed destination
 // area is at most old+new cursor bounds, not the 8,294,400-pixel scene.
 Context large{2,201,33,4,3840,2160,0,0};
 auto backdrop=std::make_shared<const std::vector<uint32_t>>(3840*2160,0xff234567);
 auto tip=std::make_shared<const std::vector<uint32_t>>(32*32,0x80800040);
 Surface base{3,5,large,{0,0,3840,2160},true,255,backdrop};
 Surface mouse{8,300,large,{100,100,32,32},false,255,tip};Incremental moving;
 assert(moving.update(large,{base,mouse}));uint64_t repainted=0;
 auto start=std::chrono::steady_clock::now();
 for(int n=1;n<=30;++n){mouse.rect.x=100+n;assert(moving.update(large,{base,mouse}));auto d=moving.dirty();assert(d.w==33&&d.h==32);repainted+=uint64_t(d.w)*d.h;}
 double milliseconds=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
 assert(compose(large,{base,mouse},expected)&&moving.pixels()==expected);
 assert(repainted==30*33*32&&ground->front()==0xff123456&&backdrop->front()==0xff234567);
 std::printf("PASS: destination clip and sparse native-feedback holes; 160 full-composition equivalence cases; 4K cursor updates repaint %llu pixels/30 frames (%.4f ms/frame CPU composition)\n",(unsigned long long)repainted,milliseconds/30);
}
