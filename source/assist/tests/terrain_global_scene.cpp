#define main terrain_partial_fixture_main
#include "terrain_partial_scene.cpp"
#undef main
#include <chrono>
int main(){
 Memory memory;Read read{&memory};Assets assets;Renderer renderer;radar::Frame frame;auto t=makeTerrain();
 t->geo={64,64,64,64,128,128,1,0x10000,0x20000,512*512,{64,64,0,0,64,64},128,128};
 for(int y=1;y<512;++y)for(int x=1;x<512;++x)if(t->geo.legal(x,y)){size_t n=size_t(y)*512+x;t->colors[n]=0x304820;t->art[n].type=1;t->art[n].sourceColor=0x304820;}
 const size_t ore=60*512+61;t->art[ore].cell=Cell;t->art[ore].overlayType=0x50000;frame.terrain=t;frame.tick=100;frame.frame=1;
 auto draw=[&](Renderer&r,Assets&a){frame.tick+=16;return r.render(frame,384,192,read,a,{},1,false,true,false,true);};
 std::shared_ptr<const radar::Scene>out;for(int n=0;n<32;++n){out=draw(renderer,assets);if(out&&renderer.materialsReady())break;}assert(out&&renderer.materialsReady());
 auto before=renderer.nativeStats();assert(before.cells>4096);uint64_t compared=0,redrawn=0;double elapsed=0;
 for(int i=0;i<6;++i){auto start=std::chrono::steady_clock::now();auto next=draw(renderer,assets);elapsed+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();assert(next&&renderer.materialsReady());assert(renderer.nativeStats().cells==before.cells);}
 for(int phase=1;phase<=5;++phase){auto next=std::make_shared<radar::Terrain>(*frame.terrain);next->art[ore].overlayFrame=uint8_t(phase);frame.terrain=next;
  out=draw(renderer,assets);assert(out&&renderer.materialsReady());auto after=renderer.nativeStats();assert(after.partialBanks==before.partialBanks+1&&after.cells-before.cells<before.cells/4);redrawn+=after.cells-before.cells;
  Renderer full;Assets other;std::shared_ptr<const radar::Scene>expected;for(int i=0;i<32;++i){expected=draw(full,other);if(expected&&full.materialsReady())break;}assert(expected&&out->pixels==expected->pixels);compared+=out->pixels.size();before=after;
 }
 std::printf("PASS whole-map: 6/6 unchanged draws publish immediately, 5 ore changes use local pages (%llu redrawn cells), %llu exact pixels; mean unchanged %.3f ms\n",(unsigned long long)redrawn,(unsigned long long)compared,elapsed/6);
}
