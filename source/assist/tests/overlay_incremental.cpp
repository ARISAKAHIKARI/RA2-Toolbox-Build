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
 auto verify=[&](std::vector<Surface>layers){assert(compose(c,layers,expected));assert(cache.update(c,std::move(layers)));assert(cache.pixels()==expected);
  assert(cache.dirtyRegions().size()<=Incremental::MaximumDirtyRegions);uint64_t work=0;for(const auto&r:cache.dirtyRegions()){assert(r.w>0&&r.h>0&&r.x>=0&&r.y>=0&&r.x+r.w<=c.width&&r.y+r.h<=c.height);work+=uint64_t(r.w)*r.h;}
  const auto all=cache.dirty();assert(work<=uint64_t(all.w)*all.h);
 };
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
 // Perpendicular thin dirty strips may overlap. Replaying each from all
 // current layers is exact and avoids expanding their cross to a full view.
 auto linePixels=std::make_shared<const std::vector<uint32_t>>(80,0x80400040);
 Surface row{40,100,c,{0,29,80,1},false,255,linePixels};
 auto columnPixels=std::make_shared<const std::vector<uint32_t>>(60,0x80800000);
 Surface column{41,101,c,{39,0,1,60},false,255,columnPixels};
 verify({row,column});row.opacity=135;column.opacity=200;verify({row,column});assert(cache.dirtyRegions().size()==2);
 // Extreme fragmentation remains bounded and conservatively collapses to
 // the union. Full-composition correctness also covers removed last layers.
 auto dot=std::make_shared<const std::vector<uint32_t>>(1,0xff345678);
 std::vector<Surface> fragments;for(unsigned n=0;n<70;++n)fragments.push_back({100+n,150,c,{int(n%35)*2,int(n/35)*2,1,1},false,255,dot});
 verify(fragments);dot=std::make_shared<const std::vector<uint32_t>>(1,0xff987654);for(auto&s:fragments)s.pixels=dot;
 verify(fragments);assert(cache.dirtyRegions().size()==1);verify(fragments);assert(cache.dirtyRegions().empty());verify({});
 // Many mixed overlaps exercise coalescing decisions independently of one
 // producer's layout; compare every output against the full painter oracle.
 uint32_t randomState=0x87654321u;auto random=[&](){randomState=randomState*1664525u+1013904223u;return randomState;};
 std::vector<Surface> mixed;
 for(unsigned n=0;n<48;++n){const int w=1+int(n%5),h=1+int((n*3)%7);auto source=std::make_shared<const std::vector<uint32_t>>(size_t(w)*h,0x80603020u+n);
  mixed.push_back({1000+n,int(n%9),c,{int(random()%100)-10,int(random()%80)-10,w,h},false,uint8_t(random()),source});}
 for(unsigned frame=0;frame<96;++frame){for(unsigned n=0;n<mixed.size();++n)if((n+frame)%7==0){auto&s=mixed[n];s.rect.x=int(random()%100)-10;s.rect.y=int(random()%80)-10;s.opacity=uint8_t(random());s.order=int(random()%20);}
  auto present=mixed;if(frame%5==0)present.erase(present.begin()+frame%present.size());verify(std::move(present));}
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
 // A counter at the opposite corner changes alongside the cursor. The old
 // single dirty bounding box scanned millions of unchanged scene pixels.
 Surface far{9,200,large,{3700,2000,32,32},false,255,tip};assert(moving.update(large,{base,mouse,far}));
 ++mouse.rect.x;far.opacity=180;assert(moving.update(large,{base,mouse,far}));
 assert(moving.dirtyRegions().size()==2);uint64_t separated=0;for(const auto&r:moving.dirtyRegions())separated+=uint64_t(r.w)*r.h;
 assert(separated==33*32+32*32);const auto unionBounds=moving.dirty();const uint64_t unionPixels=uint64_t(unionBounds.w)*unionBounds.h;assert(unionPixels>6000000);
 assert(compose(large,{base,mouse,far},expected)&&moving.pixels()==expected);
 base.cutouts=std::make_shared<const std::vector<Rect>>(std::vector<Rect>{{10,10,1,1},{3800,2100,2,1}});
 assert(moving.update(large,{base,mouse,far}));assert(moving.dirtyRegions().size()==2);
 uint64_t holesPixels=0;for(const auto&r:moving.dirtyRegions())holesPixels+=uint64_t(r.w)*r.h;assert(holesPixels==3);
 assert(compose(large,{base,mouse,far},expected)&&moving.pixels()==expected);
 assert(moving.update(large,{base,mouse}));assert(compose(large,{base,mouse},expected)&&moving.pixels()==expected);
 std::printf("PASS: destination clip, sparse holes, crossing strips, bounded fragmentation and full-composition equivalence; 4K cursor %llu pixels/30 frames (%.4f ms/frame CPU); distant changes %llu pixels vs %llu union; sparse holes3 pixels\n",(unsigned long long)repainted,milliseconds/30,(unsigned long long)separated,(unsigned long long)unionPixels);
}
