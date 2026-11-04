// CPU-only synthetic whole-map scene. Compile with -DHAS_STAGES for timing stages.
// Args: --whole ACTORS [--all|--one|--static]; no Vulkan presentation is requested.
#define main original_fixture_main
#include "../assist/tests/radar_scene.cpp"
#undef main
#include <chrono>
int main(int argc,char**argv){
 const bool whole=argc>1&&std::string(argv[1])=="--whole";const unsigned count=argc>2?unsigned(std::stoul(argv[2])):160;
 using namespace external::radar;Assets assets;SceneRenderer<Assets>renderer;auto read=[](uint32_t,void*,size_t){return false;};
 auto t=std::make_shared<Terrain>();t->geo={64,64,64,64,128,128,1,0x10000,0x20000,512*512,{64,64,0,0,64,64},128,128};t->colors.resize(512*512);t->art.resize(512*512);t->reveal=true;
 for(int y=42;y<=86;++y)for(int x=42;x<=86;++x){auto n=y*512+x;t->colors[n]=0xa03020;t->art[n].type=1;t->art[n].sourceColor=0xa03020;}
 Frame f;f.terrain=t;f.tick=100;f.frame=1;for(unsigned n=0;n<count;++n){Marker m;m.object=0x10000+n*0x1000;m.id=n+1;m.type=0x20000;m.kind=2;m.layer=2;m.x=(52+n%20)*256;m.y=(52+(n/20)%16*2)*256;f.markers.push_back(m);}
 std::shared_ptr<const Scene>out;for(int i=0;i<12;++i){out=renderer.render(f,whole?320:1280,whole?160:720,read,assets,whole?SceneRegion{}:SceneRegion{1280,600,1280,720,true},1,false,true,false,true,4.);if(out&&renderer.materialsReady())break;}
 if(!out){std::cerr<<"warmup failed\n";return 1;}
 std::vector<double>times;uint64_t checksum=1469598103934665603ull;
 double terrain=0,materials=0,objects=0,resolve=0,effects=0;
 for(int i=0;i<20;++i){++f.tick;++f.frame;if(argc>3&&std::string(argv[3])=="--one")f.markers[0].x+=i%2?16:-16;else if(argc<4||std::string(argv[3])!="--static")for(auto&m:f.markers){m.x+=i%2?16:-16;}auto start=std::chrono::steady_clock::now();out=renderer.render(f,whole?320:1280,whole?160:720,read,assets,whole?SceneRegion{}:SceneRegion{1280+i*2.,600,1280,720,true},1,false,true,false,true,4.);times.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());if(!out){std::cerr<<"render failed\n";return 2;}for(auto p:out->pixels)checksum=(checksum^p)*1099511628211ull;
#ifdef HAS_STAGES
  const auto&s=renderer.stageStats();terrain+=s.terrainMs;materials+=s.materialsMs;objects+=s.objectsMs;resolve+=s.resolveMs;effects+=s.effectsMs;
#endif
 }
 std::sort(times.begin(),times.end());std::cout<<"{\"width\":"<<out->width<<",\"height\":"<<out->height<<",\"actors\":"<<count<<",\"frames\":20,\"median_ms\":"<<times[10]<<",\"p95_ms\":"<<times[18]<<",\"checksum\":"<<checksum<<",\"stages_mean_ms\":{\"terrain\":"<<terrain/20<<",\"materials\":"<<materials/20<<",\"objects\":"<<objects/20<<",\"resolve\":"<<resolve/20<<",\"effects\":"<<effects/20<<"}}\n";
}
