#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../../external_reader/source/camera_assets.h"
#include "../../external_reader/source/radar_scene.h"
#include RA2_REFERENCE_LIGHTING
#include RA2_REFERENCE_SCENE
#include <cassert>
#include <cstdio>
using namespace external;
static std::vector<uint8_t> memory(0x2000000);
static uint64_t reads=0,metadataReads=0;static uint32_t failAt=0;
template<class T>static void put(uint32_t p,const T&v){std::memcpy(memory.data()+p,&v,sizeof v);}
struct FixtureRead {bool operator()(uint32_t at,void*out,size_t n)const{
 ++reads;if((at>=0x710000&&at<0x710200)||at==0x8A0DD0)++metadataReads;
 if(uint64_t(at)+n>memory.size()||(failAt>=at&&failAt<uint64_t(at)+n))return false;std::memcpy(out,memory.data()+at,n);return true;
 }bool readFresh(uint32_t at,void*out,size_t n){return (*this)(at,out,n);}bool imagePath(wchar_t*,DWORD*){return false;}};
using BaseAssets=cameraassets::Context<FixtureRead>;
struct Assets:BaseAssets{using BaseAssets::BaseAssets;
 bool palette(bool,std::array<uint32_t,256>&out){out.fill(0xff2266aa);return true;}
 bool tile(uint32_t,unsigned,const std::array<uint32_t,256>&p,voxel::Image&out){out.width=60;out.height=30;out.x=-30;out.y=-15;out.pixels.assign(1800,p[1]);out.indices.assign(1800,1);out.nativeDepth.enabled=true;out.nativeDepth.gradient=nativesurface::Gradient::Flat;return true;}
};
static double now(){return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();}
static bool sameLight(const lighting::State&a,const lighting::State&b){return a.ambient==b.ambient&&a.tint==b.tint&&a.intensity==b.intensity&&a.paletteRows==b.paletteRows&&a.scalar==b.scalar&&a.rgb555==b.rgb555&&((!a.table&&!b.table)||(a.table&&b.table&&*a.table==*b.table));}
int main(){
 FixtureRead read;put(0xA8B230,uint32_t(0x600000));put(0x60352C,100);put(0x603534,std::array<int,3>{{100,100,100}});
 put(0x710000,uint32_t(0x7E5358));put(0x710004,2);put(0x71016C,1);put(0x710170,uint32_t(0x720000));put(0x710198,std::array<int,3>{{1000,1000,1000}});put(0x720002,uint16_t(0xf800));put(0xA8ED84,1);
 auto terrain=std::make_shared<radar::Terrain>();auto&g=terrain->geo;g.width=g.height=100;g.offsetX=g.offsetY=100;g.rw=g.rh=g.rawWidth=g.rawHeight=200;g.factor=1;g.cells=0x100000;g.house=0x400000;g.count=512*512;g.bounds={100,100,0,0,100,100};assert(g.valid());
 terrain->reveal=true;terrain->colors.resize(512*512);terrain->baseColors.resize(512*512);terrain->art.resize(512*512);
 unsigned count=0;std::vector<size_t>indices;
 for(int y=1;y<512&&count<10000;++y)for(int x=1;x<512&&count<10000;++x){mapbounds::TileOrigin at;if(!g.tileOrigin(x,y,0,at))continue;
  const size_t i=y*512+x;auto&c=terrain->art[i];c.cell=0x1000000+count*0x200;c.type=0x250000;terrain->colors[i]=terrain->baseColors[i]=0x193347;
  put(c.cell+0x104,uint32_t(65536));put(c.cell+0x34,uint32_t(0x710000));put(c.cell+0x10A,std::array<int16_t,3>{{1000,1000,1000}});indices.push_back(i);++count;
 }
 assert(count==10000);radar::Frame frame;frame.terrain=terrain;frame.frame=1;frame.tick=1;frame.markerScanValid=true;
 Assets oldAssets(read),assets(read);radar::BeforeSceneRenderer<Assets>before;radar::SceneRenderer<Assets>after;before.outputSized(true);after.outputSized(true);xbrzadapter::gpuPresentation(false);
 auto oldRender=[&]{return before.render(frame,600,300,read,oldAssets,{},1,false,true,false,true);};
 auto newRender=[&]{return after.render(frame,600,300,read,assets,{},1,false,true,false,true);};
 std::shared_ptr<const radar::Scene>oldImage,newImage;
 for(int n=0;n<10&&(!oldImage||!newImage);++n){oldImage=oldRender();newImage=newRender();}
 assert(oldImage&&newImage&&oldImage->pixels==newImage->pixels);
 double oldMs=0,newMs=0,oldTerrain=0,newTerrain=0;uint64_t oldReads=0,newReads=0,oldMetadata=0,newMetadata=0;
 for(int n=0;n<24;++n){++frame.frame;++frame.tick;put(0xA8ED84,frame.frame);
  auto oldRun=[&]{reads=metadataReads=0;double start=now();oldImage=oldRender();oldMs+=now()-start;oldTerrain+=before.stageStats().terrainMs;oldReads+=reads;oldMetadata+=metadataReads;};
  auto newRun=[&]{reads=metadataReads=0;double start=now();newImage=newRender();newMs+=now()-start;newTerrain+=after.stageStats().terrainMs;newReads+=reads;newMetadata+=metadataReads;};
  if(n&1){oldRun();newRun();}else{newRun();oldRun();}
  assert(oldImage&&newImage&&oldImage->pixels==newImage->pixels);assert(after.stageStats().terrainLightCells==10000&&after.stageStats().terrainGeometryChecks==0&&after.stageStats().terrainProbeCopies==0);
 }
 assert(newMetadata*100<oldMetadata&&newReads*2<oldReads);
 std::printf("PASS full production SceneRenderer 10000 cells x 24 new game frames: exact output; reads=%llu->%llu metadata_reads=%llu->%llu total_mean_ms=%.6f->%.6f terrain_mean_ms=%.6f->%.6f geometry_checks=10000->0 probe_copies=10000->0 light_cells=10000->10000\n",(unsigned long long)(oldReads/24),(unsigned long long)(newReads/24),(unsigned long long)(oldMetadata/24),(unsigned long long)(newMetadata/24),oldMs/24,newMs/24,oldTerrain/24,newTerrain/24);
 // Same-frame cell intensity, converter identity/tint/format and fallback
 // changes are validated again in the next presentation, not held by frame.
 unsigned cases=0;lighting::State fallback;fallback.paletteRows=7;fallback.scalar=true;fallback.rgb555=true;
 for(int variant=0;variant<12;++variant){
  const uint32_t cell=terrain->art[indices[0]].cell;failAt=0;
  put(cell+0x10A,std::array<int16_t,3>{{int16_t(900+variant),int16_t(700+variant),int16_t(600+variant)}});
  put(0x710198,std::array<int,3>{{1000,900+variant,800}});put(0x8A0DD0,variant&1?std::array<int,6>{{10,3,0,3,5,3}}:std::array<int,6>{});
  if(variant>=3&&variant<=8){const uint32_t failures[]={0x71016C,0x710000,0x710004,0x710170,0x8A0DD0,cell+0x10A};failAt=failures[variant-3];}
  lighting::TintCache oldCache,newCache;
  for(int repeat=0;repeat<3;++repeat){fallback.paletteRows=7+repeat;fallback.rgb555=repeat&1;auto a=lighting::cell_before(read,cell,fallback,lighting::Layer::Terrain,true,&oldCache),b=lighting::cell(read,cell,fallback,lighting::Layer::Terrain,true,&newCache);assert(sameLight(a,b));++cases;}
 }
 failAt=0;put(0x8A0DD0,std::array<int,6>{});put(0x710198,std::array<int,3>{{1000,1000,1000}});
 auto settled=[&]{oldImage.reset();newImage.reset();for(int n=0;n<12&&(!oldImage||!newImage);++n){oldImage=oldRender();newImage=newRender();}assert(oldImage&&newImage&&oldImage->pixels==newImage->pixels);};
 settled();put(0x720002,uint16_t(0x07e0));put(0xA8ED84,++frame.frame);settled();
 auto changed=std::make_shared<radar::Terrain>(*terrain);changed->art[indices[100]].level=2;changed->baseColors[indices[900]]=0x223344;frame.terrain=changed;settled();
 changed=std::make_shared<radar::Terrain>(*changed);changed->art[indices[500]].type=0;changed->art[indices[500]].sourceColor=0x445566;frame.terrain=changed;settled();
 std::printf("PASS metadata fallback parity=%u; same-frame live light/tint; next-frame converter rows; new terrain height/base/tile removal invalidate. No viewport or refresh reduction.\n",cases);
}
