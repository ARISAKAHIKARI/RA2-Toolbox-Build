#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../../external_reader/source/radar_scene.h"
#include <cassert>
#include <cstdio>
#include <cstring>

using namespace external;
namespace {
constexpr uint32_t Scenario=0x600000,Convert=0x710000,Rows=0x720000,Cells=0x1000000;
struct Memory {
 std::vector<uint8_t> bytes=std::vector<uint8_t>(0x1600000);
 template<class T>void put(uint32_t at,const T&value){std::memcpy(bytes.data()+at,&value,sizeof value);}
 bool read(uint32_t at,void*out,size_t n)const{if(uint64_t(at)+n>bytes.size())return false;std::memcpy(out,bytes.data()+at,n);return true;}
 worldframe::Token seal(uint32_t serial,int frame=1,uint32_t match=1,uint32_t missing=0)const{
  auto out=std::make_shared<worldframe::Snapshot>();worldframe::Key key;
  key.pid=7;key.owner=9;key.match=match;key.serial=serial;key.frame=frame;key.scenario=Scenario;key.cells=0x100000;key.house=0x400000;key.cellCount=512*512;key.session=0;
  out->reset(key);auto live=[&](uint32_t at,void*dest,size_t n){return !(missing>=at&&missing<uint64_t(at)+n)&&read(at,dest,n);};
  out->prefetch(live,0x600000,0x600000);out->prefetch(live,Cells,0x500000);assert(out->seal());return out;
 }
};
struct Reader {
 worldframe::Token source;uint64_t reads=0;bool complete=true;
 void begin(worldframe::Token next){source=std::move(next);reads=0;complete=true;}
 worldframe::Token worldFrameToken()const{return source;}
 bool worldFrameComplete()const{return source&&complete;}
 bool read(uint32_t at,void*out,size_t n){++reads;const bool ok=source&&source->read(at,out,n);complete&=ok;return ok;}
 bool optional(uint32_t at,void*out,size_t n){++reads;const bool ok=source&&source->read(at,out,n);if(!ok&&(!source||!source->observed(at,n)))complete=false;return ok;}
};
struct Read {
 static constexpr bool SealedWorldReads=true;
 Reader*reader;
 bool operator()(uint32_t at,void*out,size_t n)const{return reader->read(at,out,n);}
 bool optional(uint32_t at,void*out,size_t n)const{return reader->optional(at,out,n);}
};
struct LiveRead {Memory*memory;bool operator()(uint32_t at,void*out,size_t n)const{return memory->read(at,out,n);}};
struct Assets {
 struct ActorArt{};uint64_t epoch=1;unsigned paletteEpoch=0;
 uint64_t revision(){return epoch;}static size_t cost(const ActorArt&){return 1;}
 bool palette(bool,std::array<uint32_t,256>&out){out.fill(0xff2266aa+paletteEpoch);return true;}
 bool tile(uint32_t,unsigned,const std::array<uint32_t,256>&p,voxel::Image&out){out.width=60;out.height=30;out.x=-30;out.y=-15;out.pixels.assign(1800,p[1]);out.indices.assign(1800,1);out.nativeDepth.enabled=true;out.nativeDepth.gradient=nativesurface::Gradient::Flat;return true;}
 bool overlay(uint32_t,uint32_t,int,voxel::Image&,bool=false){return false;}
 bool convertPalette(uint32_t,std::array<uint32_t,256>&){return false;}
 bool spritePalette(uint32_t,const std::string&,bool,uint32_t,std::array<uint32_t,256>&){return false;}
 bool particlePalette(std::array<uint32_t,256>&){return false;}
 bool debrisPalette(uint32_t,std::array<uint32_t,256>&){return false;}
 std::vector<std::string> spriteNames(uint32_t,uint32_t,bool){return {};}
 bool resourceSprite(uint32_t,uint32_t,const std::array<uint32_t,256>&,int,voxel::Image&,bool,bool){return false;}
 bool overlayDepthShape(voxel::Image&,unsigned,bool){return false;}
 int resourceFrames(uint32_t,uint32_t,bool){return 0;}
 bool shroud(bool,int,voxel::Image&out){out.width=60;out.height=30;out.x=-30;out.y=-15;out.pixels.assign(1800,0);out.indices.assign(1800,0xfe);return true;}
 void buildingPose(uint32_t,uint32_t,visual::Pose&){}
 bool captureActor(uint32_t,uint32_t,uint32_t,ActorArt&){return false;}
 bool captureAnimation(uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,ActorArt&){return false;}
 bool renderActor(const ActorArt&,const visual::Pose&,voxel::Image&){return false;}
};
using Renderer=radar::SceneRenderer<Assets>;
uint64_t hash(const radar::Scene&scene){uint64_t out=1469598103934665603ull;for(auto p:scene.pixels)out=(out^p)*1099511628211ull;return out;}
}
int main(int argc,char**argv){
 std::setvbuf(stdout,nullptr,_IONBF,0);
 const bool baseline=argc>1&&std::strcmp(argv[1],"--baseline")==0;
 Memory memory;memory.put(0xA8B230,Scenario);memory.put(Scenario+0x352C,100);memory.put(Scenario+0x3534,std::array<int,3>{{100,100,100}});
 memory.put(Convert,uint32_t(0x7E5358));memory.put(Convert+4,2);memory.put(Convert+0x16C,1);memory.put(Convert+0x170,Rows);memory.put(Convert+0x198,std::array<int,3>{{1000,1000,1000}});memory.put(Rows+2,uint16_t(0xffff));memory.put(0xA8ED84,1);
 auto terrain=std::make_shared<radar::Terrain>();auto&g=terrain->geo;g.width=g.height=100;g.offsetX=g.offsetY=100;g.rw=g.rh=g.rawWidth=g.rawHeight=200;g.factor=1;g.cells=0x100000;g.house=0x400000;g.count=512*512;g.bounds={100,100,0,0,100,100};assert(g.valid());
 terrain->reveal=true;terrain->colors.resize(512*512);terrain->baseColors.resize(512*512);terrain->art.resize(512*512);
 unsigned count=0;std::vector<size_t>indices;
 for(int y=1;y<512&&count<10000;++y)for(int x=1;x<512&&count<10000;++x){mapbounds::TileOrigin at;if(!g.tileOrigin(x,y,0,at))continue;
  const size_t i=y*512+x;auto&c=terrain->art[i];c.cell=Cells+count*0x200;c.type=0x250000;terrain->colors[i]=terrain->baseColors[i]=0x193347;
  memory.put(c.cell+0x104,uint32_t(65536));memory.put(c.cell+0x34,Convert);memory.put(c.cell+0x10A,std::array<int16_t,3>{{1000,1000,1000}});indices.push_back(i);++count;
 }
 assert(count==10000);radar::Frame frame;frame.terrain=terrain;frame.frame=1;frame.tick=1;frame.markerScanValid=true;frame.sourceFrame=memory.seal(1);
 Reader reader;Read read{&reader};Assets assets;Renderer renderer;renderer.outputSized(true);xbrzadapter::gpuPresentation(false);
 auto draw=[&](radar::SceneRegion region=radar::SceneRegion{},bool highPrecision=false){reader.begin(frame.sourceFrame);++frame.tick;return renderer.render(frame,600,300,read,assets,region,1,false,true,highPrecision,true);};
 auto settle=[&](radar::SceneRegion region=radar::SceneRegion{},bool highPrecision=false){std::shared_ptr<const radar::Scene>out;for(int n=0;n<24;++n){out=draw(region,highPrecision);if(out&&renderer.materialsReady()){out=draw(region,highPrecision);assert(out);return out;}}assert(false);return out;};
 auto initial=settle();assert(reader.worldFrameComplete());const auto initialPixels=initial->pixels;
 uint64_t lightCells=0,geometry=0,readCalls=0;double terrainMs=0;
 for(int n=0;n<8;++n){auto image=draw();assert(image&&image->pixels==initialPixels&&reader.worldFrameComplete());lightCells+=renderer.stageStats().terrainLightCells;geometry+=renderer.stageStats().terrainGeometryChecks;readCalls+=reader.reads;terrainMs+=renderer.stageStats().terrainMs;}
 std::printf("sealed_whole_map: cells=10000 repeats=8 light_cells_per_draw=%llu geometry_per_draw=%llu read_calls_per_draw=%llu terrain_mean_ms=%.6f hash=%016llx\n",(unsigned long long)(lightCells/8),(unsigned long long)(geometry/8),(unsigned long long)(readCalls/8),terrainMs/8,(unsigned long long)hash(*initial));
 // Same engine frame and converter/data addresses, but a new sealed source.
 memory.put(Rows+2,uint16_t(0x07e0));frame.sourceFrame=memory.seal(2);reader.begin(frame.sourceFrame);
 lighting::State palette;lighting::converterMetadata(read,Convert,palette);assert(palette.table);
 std::printf("same_frame_new_serial_palette: expected=07e0 actual=%04x complete=%d\n",unsigned((*palette.table)[1]),reader.worldFrameComplete());
 if(baseline)return 0;
 assert(lightCells==0&&geometry==0&&(*palette.table)[1]==0x07e0);
 auto changed=settle();assert(changed->pixels!=initialPixels&&reader.worldFrameComplete());
 // A cached table must never supply missing bytes to another publication.
 frame.sourceFrame=memory.seal(3,1,1,Rows);reader.begin(frame.sourceFrame);palette={};lighting::converterMetadata(read,Convert,palette);assert(!palette.table&&!reader.worldFrameComplete());
 for(int n=0;n<2;++n){draw();assert(renderer.stageStats().terrainLightCells>0&&!reader.worldFrameComplete());}
 frame.sourceFrame=memory.seal(4);changed=settle();assert(reader.worldFrameComplete());draw();assert(renderer.stageStats().terrainLightCells==0);
 // Token/world identity, terrain source, resource/palette, precision and crop
 // changes cannot inherit the prior verification, even at the same frame.
 auto requireWork=[&]{auto out=draw();assert(renderer.stageStats().terrainLightCells>0);settle();assert(reader.worldFrameComplete());return out;};
 frame.sourceFrame=memory.seal(1,1,2);requireWork();
 auto altered=std::make_shared<radar::Terrain>(*terrain);altered->art[indices[100]].level=2;frame.terrain=altered;requireWork();
 ++assets.epoch;requireWork();++assets.paletteEpoch;requireWork();
 draw({},true);assert(renderer.stageStats().terrainLightCells>0);settle({},true);
 renderer.prepareWorld(true);renderer.outputSized(true);radar::SceneRegion crop{1800,800,600,300,true};settle(crop,true);
 // Local presentation may publish a verified crop while the normal 4096-cell
 // offscreen probe is still finishing. Only measure the completed bank.
 for(int n=0;n<4;++n)draw(crop,true);
 uint64_t cropLights=0,cropGeometry=0;for(int n=0;n<8;++n){auto ready=draw(crop,true);assert(ready&&renderer.preparedWorld()&&reader.worldFrameComplete());cropLights+=renderer.stageStats().terrainLightCells;cropGeometry+=renderer.stageStats().terrainGeometryChecks;}
 assert(cropLights==0&&cropGeometry==0);crop.x+=128;draw(crop,true);assert(renderer.stageStats().terrainGeometryChecks>0);
 std::printf("sealed_prepared_world: cells=10000 repeats=8 light_cells_per_draw=%llu geometry_per_draw=%llu\n",(unsigned long long)(cropLights/8),(unsigned long long)(cropGeometry/8));
 // A Frame token alone is not proof that an arbitrary callback is frozen.
 Renderer liveRenderer;Assets liveAssets;liveRenderer.outputSized(true);LiveRead live{&memory};frame.terrain=terrain;
 auto liveDraw=[&]{return liveRenderer.render(frame,600,300,live,liveAssets,{},1,false,true,false,true);};
 std::shared_ptr<const radar::Scene>liveImage;for(int n=0;n<24;++n){liveImage=liveDraw();if(liveImage&&liveRenderer.materialsReady())break;}assert(liveImage);liveDraw();assert(liveRenderer.stageStats().terrainLightCells==10000);
 memory.put(Cells+0x10A,std::array<int16_t,3>{{1000,600,1000}});liveDraw();assert(liveRenderer.stageStats().terrainLightCells>0);
 std::puts("PASS exact sealed terrain reuse; same-frame new-serial palette; missing bytes remain incomplete; match/terrain/resource/palette/precision/crop invalidation; prepared-world crop reuse; live callback revalidation");
}
