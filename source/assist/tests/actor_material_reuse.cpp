#ifndef CAMERA_ASSETS_HEADER
#define CAMERA_ASSETS_HEADER "../../external_reader/source/camera_assets.h"
#endif
#include CAMERA_ASSETS_HEADER
#include <cassert>
#include <chrono>
#include <cstdio>
#include <set>

// Synthetic, redistributable SHP bytes exercise the actual Context decoder
// and shared bank. No game process, whole-scene reuse or frame skipping.
struct Reader {
 bool fail=false,race=false;unsigned calls=0,tables=0;uint32_t count=32;
 std::vector<uint8_t> table=std::vector<uint8_t>(8192);
 Reader(){for(unsigned i=0;i<table.size();++i)table[i]=uint8_t(i+i/256);}
 bool imagePath(wchar_t*,DWORD*){return false;}
 bool readFresh(uint32_t p,void*out,size_t n){++calls;if(fail)return false;
  if(p==0xB2FB58&&n==4){memcpy(out,&count,4);return true;}
  if(p==0xB41178&&n==size_t(count)*256&&n<=table.size()){
   memcpy(out,table.data(),n);if(race&&(++tables&1)==0)static_cast<uint8_t*>(out)[17]^=1;return true;}
  return false;
 }
};
using Context=external::cameraassets::Context<Reader>;
using Image=external::voxel::Image;
using Pose=external::visual::Pose;
template<class T>void put(std::vector<uint8_t>&b,size_t off,T value){memcpy(b.data()+off,&value,sizeof value);}
static Context::ActorArt art(unsigned id,int kind=5){
 Context::ActorArt a;a.id=id;a.object=0x10000+id*128;a.type=0x25000;a.pointer=0x30000;a.kind=kind;
 const unsigned w=48,h=48,frames=16,header=8+24*frames;
 a.shape.resize(header+frames*w*h);put<uint16_t>(a.shape,2,w);put<uint16_t>(a.shape,4,h);put<uint16_t>(a.shape,6,frames);
 for(unsigned f=0;f<frames;++f){size_t off=8+f*24;put<uint16_t>(a.shape,off+4,w);put<uint16_t>(a.shape,off+6,h);put<uint32_t>(a.shape,off+20,header+f*w*h);
  for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x)a.shape[header+f*w*h+y*w+x]=(x+y+f)%7?uint8_t(1+(x*3+y+f)%254):0;}
 for(unsigned i=0;i<256;++i)a.colors[i]=0xff000000u|(i*73%256)<<16|(i*37%256)<<8|i;
 return a;
}
static void same(const Image&a,const Image&b){
 assert(a.pixels==b.pixels&&a.indices==b.indices&&a.depth==b.depth&&a.nativeShape==b.nativeShape&&a.nativeShapeCoverage==b.nativeShapeCoverage);
 assert(a.width==b.width&&a.height==b.height&&a.x==b.x&&a.y==b.y&&a.rasterScale==b.rasterScale&&a.canvasWidth==b.canvasWidth&&a.canvasHeight==b.canvasHeight);
 assert(a.nativeBlend==b.nativeBlend&&a.nativeExtended==b.nativeExtended&&a.intrinsicAlpha==b.intrinsicAlpha&&a.objectPalette==b.objectPalette&&a.flat==b.flat&&a.writeDepth==b.writeDepth&&a.shadow==b.shadow&&a.shadowsAfter==b.shadowsAfter&&a.incomplete==b.incomplete&&a.depthBias==b.depthBias);
 assert(a.nativeDepth.enabled==b.nativeDepth.enabled&&a.nativeDepth.gradient==b.nativeDepth.gradient&&a.nativeDepth.adjust==b.nativeDepth.adjust);
 assert(bool(a.blendSource)==bool(b.blendSource));if(a.blendSource)assert(*a.blendSource==*b.blendSource);
 assert(a.attachments.size()==b.attachments.size());for(size_t i=0;i<a.attachments.size();++i)same(a.attachments[i],b.attachments[i]);
}
static std::shared_ptr<const Image> shared(Context&c,const Context::ActorArt&a,const Pose&p){std::shared_ptr<const Image>o;assert(c.renderSharedActor(a,p,o));return o;}
static volatile uint64_t checksum=0;
int main(){
 Reader r;Context c(r);c.identity(101,1);Pose pose;pose.variant=2;
 unsigned checked=0;
 for(int kind:{5,6,7})for(int f=0;f<8;++f)for(int alpha:{0,1,2,3}){
  auto a=art(1,kind),b=art(2,kind);pose.frame=f;pose.variant=alpha;
  Image direct;assert(c.renderActor(a,pose,direct));external::camera::opaque(direct);
  auto x=shared(c,a,pose),y=shared(c,b,pose);same(direct,*x);same(direct,*y);++checked;
#ifndef MATERIAL_BASELINE
  assert(x==y);
#endif
 }
 // Every material-affecting value remains independent. Liveness is checked
 // upstream; id=0/live-pointer inputs still bypass the shared cache.
 auto a=art(100);pose={};auto original=shared(c,a,pose);
 auto b=a;b.colors[1]^=0xff;assert(shared(c,b,pose)!=original);
 b=a;b.owner=33;assert(shared(c,b,pose)!=original);
 b=a;b.sourceKnown=false;b.sourceSignature={};b.shape.back()^=1;assert(shared(c,b,pose)!=original);
 b=a;b.drawY=3;b.sourceKnown=false;b.sourceSignature={};assert(shared(c,b,pose)->y==original->y+3);
 b=a;b.parent=42;assert(shared(c,b,pose)!=original);
 b=a;b.id=0;assert(shared(c,b,pose)!=shared(c,b,pose));
 b=a;b.shape.clear();b.sourceKnown=false;std::shared_ptr<const Image> missing;assert(!c.renderSharedActor(b,pose,missing));
 c.identity(102,1);assert(shared(c,a,pose)!=original);
 c.identity(102,2);assert(shared(c,a,pose)!=original);
 // Revalidate native VPL on EVERY presentation, including failure/race. The
 // derived expansion may be reused only after that live verification.
 c.presentationGPU(true);c.beginPresentation();assert(c.voxelLightingAvailable());auto old=c.voxelLightingRevision();unsigned reads=r.calls;
 c.beginPresentation();assert(c.voxelLightingAvailable()&&c.voxelLightingRevision()==old&&r.calls==reads+5);
 r.table[17]^=1;c.beginPresentation();assert(c.voxelLightingAvailable()&&c.voxelLightingRevision()!=old);old=c.voxelLightingRevision();
 r.fail=true;c.beginPresentation();assert(!c.voxelLightingAvailable()&&c.voxelLightingRevision()==old);r.fail=false;
 r.race=true;r.tables=0;c.beginPresentation();assert(!c.voxelLightingAvailable()&&c.voxelLightingRevision()==old);r.race=false;
 c.beginPresentation();assert(c.voxelLightingAvailable());
 constexpr unsigned actors=128,frames=8,trials=5;std::vector<double> times;uint64_t misses=0,hits=0;
 for(unsigned trial=0;trial<trials;++trial){c.identity(200+trial,1);std::vector<Context::ActorArt> objects;for(unsigned i=0;i<actors;++i)objects.push_back(art(1+i));
  // Fingerprint cost is common to both versions, and each capture is immutable.
  pose={};for(auto&a:objects){a.sourceKnown=false;shared(c,a,pose);}auto before=external::nativeimages::bank().stats();
  auto start=std::chrono::steady_clock::now();for(unsigned f=0;f<frames;++f){pose.frame=f;pose.variant=int(f&3);for(auto&a:objects){auto image=shared(c,a,pose);checksum+=image->pixels[f%image->pixels.size()];}}
  auto end=std::chrono::steady_clock::now();auto after=external::nativeimages::bank().stats();misses+=after.misses-before.misses;hits+=after.hits-before.hits;
  times.push_back(std::chrono::duration<double,std::milli>(end-start).count()/frames);
 }
 std::sort(times.begin(),times.end());printf("SHP dynamic %u instances/%u frames median %.6f ms/frame; builds=%llu hits=%llu; exact cases=%u\n",actors,frames,times[times.size()/2],(unsigned long long)misses,(unsigned long long)hits,checked);
 times.clear();for(unsigned trial=0;trial<trials;++trial){auto start=std::chrono::steady_clock::now();for(unsigned i=0;i<1000;++i){c.beginPresentation();assert(c.voxelLightingAvailable());checksum+=c.voxelLightingRevision();}auto end=std::chrono::steady_clock::now();times.push_back(std::chrono::duration<double,std::micro>(end-start).count()/1000);}
 std::sort(times.begin(),times.end());printf("VPL fresh capture+GPU expansion median %.6f us/presentation; checksum=%llu\n",times[times.size()/2],(unsigned long long)checksum);
 puts("PASS exact production SHP bytes, shadows, intrinsic alpha, palette/source/owner/frame/epoch separation and live VPL freshness/failure/race");
}
