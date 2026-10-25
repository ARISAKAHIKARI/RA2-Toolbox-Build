// Real read-only snapshot -> production slots/assets/poses/raster. No game files
// or private snapshot bytes belong in this repository. Run through the Python
// driver to supply the Win32 file-boundary stubs and a validity-checked capture.
#include <windows.h>
#include <fstream>
#include <iostream>
#include <map>
#include <cassert>
#include <cstring>
#include <vector>
DWORD GetTickCount(){return 1000;}uint64_t GetTickCount64(){return 1000;}
bool GetFileAttributesExW(const wchar_t*,unsigned,WIN32_FILE_ATTRIBUTE_DATA*){return false;}
HANDLE CreateFileW(const wchar_t*,unsigned,unsigned,void*,unsigned,unsigned,void*){return INVALID_HANDLE_VALUE;}
bool SetFilePointerEx(HANDLE,LARGE_INTEGER,LARGE_INTEGER*,unsigned){return false;}
bool ReadFile(HANDLE,void*,DWORD,DWORD*,void*){return false;}bool CloseHandle(HANDLE){return true;}
HANDLE FindFirstFileW(const wchar_t*,WIN32_FIND_DATAW*){return INVALID_HANDLE_VALUE;}
bool FindNextFileW(HANDLE,WIN32_FIND_DATAW*){return false;}bool FindClose(HANDLE){return true;}
#include "../../external_reader/source/camera_assets.h"
#include "../../external_reader/source/radar_scene.h"
struct Reader {
 struct Region {std::vector<uint8_t> data,valid;};std::map<uint32_t,Region> regions;
 bool imagePath(wchar_t*,DWORD*){return false;}
 bool readFresh(uint32_t p,void*out,size_t n){
  while(n){auto it=regions.upper_bound(p);if(it==regions.begin())return false;--it;
   size_t off=p-it->first;if(off>=it->second.data.size())return false;size_t take=std::min(n,it->second.data.size()-off);
   if(std::find(it->second.valid.begin()+off,it->second.valid.begin()+off+take,0)!=it->second.valid.begin()+off+take)return false;
   memcpy(out,it->second.data.data()+off,take);p+=take;out=static_cast<uint8_t*>(out)+take;n-=take;
  }return true;
 }
 explicit Reader(const char*path){std::ifstream f(path,std::ios::binary);auto word=[&](){uint32_t n;f.read(reinterpret_cast<char*>(&n),4);assert(f);return n;};auto count=word();assert(count<4096);
  for(unsigned i=0;i<count;++i){auto p=word(),n=word();assert(n<=128*1024*1024);auto&r=regions[p];r.data.resize(n);r.valid.resize(n);f.read(reinterpret_cast<char*>(r.data.data()),n);f.read(reinterpret_cast<char*>(r.valid.data()),n);assert(f);}}
};
static void ppm(const std::string&path,const external::camera::Surface&s){std::ofstream f(path,std::ios::binary);f<<"P6\n"<<s.width()<<" "<<s.height()<<"\n255\n";for(auto p:s.color){char b[3]={char(p>>16),char(p>>8),char(p)};f.write(b,3);}}
int main(int argc,char**argv){
 assert(argc>=2);using namespace external;Reader reader(argv[1]);cameraassets::Context<Reader> assets(reader);auto read=assets.read;GameView g(read);
 int restoredBodyPixels=0;int slots=0,captured=0,decoded=0,unloaded=0,pixels=0,oldRejected=0,painted=0,visible=0,minIntensity=2001,maxIntensity=-1;unsigned palettePeak=0;auto sceneLight=lighting::sample(read);std::map<std::string,int> names;std::set<std::string> pictured;
 for(auto b:g.vec(0xA8EB40)){if(!g.live(b))continue;uint32_t id=g.get<uint32_t>(b+0x10),type=g.type(b,3);auto name=assets.text(type+0x24,24);
  std::set<uint32_t> pointers;for(uint32_t offset:{0x55Cu,0x5C8u})for(int i=0;i<(offset==0x55C?21:8);++i)if(auto a=g.get<uint32_t>(b+offset+i*4))pointers.insert(a);
  slots+=int(pointers.size());auto states=animations::attached(read,b,id);captured+=int(states.size());
  camera::Surface surface;surface.reset(384,320);cameraassets::Context<Reader>::ActorArt body;voxel::Image bodyImage;
  bool bodyReady=assets.captureActor(b,type,id,body);visual::Pose pose;pose.frame=assets.buildingFrame(b,type);if(bodyReady)bodyReady=assets.renderActor(body,pose,bodyImage);
  int bx=g.get<int>(b+0x9C),by=g.get<int>(b+0xA0),bz=g.get<int>(b+0xA4);auto baseline=float((int64_t(bx)+by)*15/256.);
  auto bodyLight=lighting::at(read,bx,by,bz,sceneLight);if(bodyLight.intensity>=0){minIntensity=std::min(minIntensity,bodyLight.intensity);maxIntensity=std::max(maxIntensity,bodyLight.intensity);}
  if(bodyReady)for(auto p:body.colors)for(int shift:{0,8,16})palettePeak=std::max(palettePeak,(p>>shift)&255u);
  // Both the body and native animation have the same render anchor when their
  // per-slot offset is zero. Compare actual colors, not a synthetic test sprite.
  // Compose actual captured TMP cells before the loaded body. This exercises
  // the native in-memory Z mask against terrain, not only a blank canvas.
  if(bodyReady){auto nearby=watch::sample(read,{b,id},true,8);
   for(int diagonal=0;diagonal<nearby.side*2;++diagonal)for(int row=0;row<nearby.side;++row){int col=diagonal-row;if(col<0||col>=nearby.side)continue;int n=row*nearby.side+col;if(!nearby.cells[n])continue;
    const auto&t=nearby.tiles[n];int x=nearby.cx+col-nearby.radius,y=nearby.cy+row-nearby.radius;voxel::Image tile;std::array<uint32_t,256> palette{};
    float px=192+(x-y)*30-float((int64_t(bx)-by)*30/256.),py=200+(x+y+1)*15-t.level*15-float((int64_t(bx)+by)*15/256.)+screenHeight(bz,projectionZScale(read));
    if(assets.palette(true,palette)&&assets.tile(t.type,t.sub,palette,tile))surface.blit(tile,px,py,1,1,float((x+y)*15),t.light,0);
    else surface.diamond(px,py,1,1,float((x+y)*15),nearby.cells[n],t.light);
   }
  }
  auto prior=surface;auto oldBody=bodyImage;if(bodyReady&&body.depthMask&&body.depthMaskBiased)camera::buildingDepth(oldBody,*body.depthMask,body.fw,body.fh,body.moveX,body.moveY,false);
  prior.blit(oldBody,192,200+camera::anchorY(3),1,1,baseline-15+camera::anchorY(3),bodyLight,2);
  surface.blit(bodyImage,192,200+camera::anchorY(3),1,1,baseline-15+camera::anchorY(3),bodyLight,2);
  if(bodyReady)for(size_t i=0;i<surface.color.size();++i)restoredBodyPixels+=surface.color[i]!=prior.color[i];
  std::stable_sort(states.begin(),states.end(),[](const auto&a,const auto&b){return camera::orderKey(a.layer,5,a.x,a.y,a.sort)<camera::orderKey(b.layer,5,b.x,b.y,b.sort);});
  for(const auto&a:states){oldRejected+=g.get<uint32_t>(a.object+0xCC)!=b;++names[assets.text(a.type+0x24,24)];
   cameraassets::Context<Reader>::ActorArt art;voxel::Image image;
   if(!assets.captureAnimation(a.object,a.type,a.id,b,id,art)){if(assets.failure=="animation-memory:reference-unloaded"){++unloaded;continue;}std::cerr<<"capture "<<assets.text(a.type+0x24,24)<<": "<<assets.failure<<"\n";continue;}
   pose.frame=a.frame;if(!assets.renderActor(art,pose,image)){std::cerr<<"decode "<<assets.text(a.type+0x24,24)<<": "<<assets.failure<<"\n";continue;}
   ++decoded;auto n=std::count_if(image.pixels.begin(),image.pixels.end(),[](uint32_t p){return p>>24;});pixels+=int(n);
   float px=192+float((int64_t(a.x)-a.y-bx+by)*30/256.);float py=200+float((int64_t(a.x)+a.y-bx-by)*15/256.)-screenHeight(a.z,projectionZScale(read))+screenHeight(bz,projectionZScale(read));
   auto animLight=a.normalLight?lighting::State{}:lighting::at(read,a.x,a.y,a.z,sceneLight);
   auto before=surface.color;surface.blit(image,px,py,1,1,float((int64_t(a.x)+a.y)*15/256.)-15-a.zAdjust,animLight,2);
   int changes=0;for(size_t i=0;i<before.size();++i)changes+=before[i]!=surface.color[i];painted+=changes;visible+=changes>0;
   if(n&&changes==0)std::cerr<<"occluded "<<assets.text(a.type+0x24,24)<<" frame="<<a.frame<<" pixels="<<n<<" z="<<a.zAdjust<<"\n";
  }
  if(argc>2&&bodyReady&&!states.empty()&&pictured.insert(name).second)ppm(std::string(argv[2])+"-"+name+".ppm",surface);
 }
 // Exercise the real sampling path used by the minimap as well as the direct
 // slot reader. Snapshot replay does not advance simulation or call game code.
 radar::Sampler sampler(read);for(int n=0;n<64;++n)sampler.step(1,16384,true);auto frame=sampler.snapshot(1000);int markers=0;for(const auto&m:frame.markers)markers+=m.kind==5;
 radar::SceneRenderer<decltype(assets)> renderer;std::shared_ptr<const radar::Scene> scene;for(int n=0;n<80;++n){scene=renderer.render(frame,512,512,read,assets);frame.tick+=16;}
 std::cout<<"{\"restored_body_pixels\":"<<restoredBodyPixels<<",\"slots\":"<<slots<<",\"captured\":"<<captured<<",\"old_parent_rejections\":"<<oldRejected<<",\"decoded\":"<<decoded<<",\"unloaded_shape_references\":"<<unloaded<<",\"opaque_pixels\":"<<pixels<<",\"visible_anims\":"<<visible<<",\"painted_pixels\":"<<painted<<",\"radar_markers\":"<<markers<<",\"scene_ready\":"<<(scene?"true":"false")<<",\"types\":"<<names.size()<<",\"min_object_intensity\":"<<minIntensity<<",\"max_object_intensity\":"<<maxIntensity<<",\"palette_peak\":"<<palettePeak<<"}"<<std::endl;
 assert(slots>0&&captured==slots&&decoded+unloaded==captured&&markers>0&&scene);
}
