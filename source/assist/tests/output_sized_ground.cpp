#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../../external_reader/source/viewport_canvas.h"
#include "../../external_reader/source/resolution_sampling.h"
#include "../../external_reader/source/radar_rank.h"
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstring>
using namespace external;
static double now(){return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();}
static std::shared_ptr<const voxel::Image> tile(){
 auto out=std::make_shared<voxel::Image>();out->width=60;out->height=30;out->x=-30;out->y=-15;out->pixels.resize(1800);out->depth.resize(1800);out->nativeDepth.enabled=true;out->nativeDepth.gradient=nativesurface::Gradient::Flat;
 for(int y=0;y<30;++y)for(int x=0;x<60;++x)if(std::abs((x+.5-30)/30)+std::abs((y+.5-15)/15)<=1)out->pixels[y*60+x]=0xff80b040;
 return out;
}
static void build(camera::NativeCanvas&out,bool retained,int width,int height,const std::shared_ptr<const voxel::Image>&image){
 assert(out.reset(width,height));out.retainGround(retained);size_t id=0;
 for(int y=0;y<height;y+=30)for(int x=0;x<width;x+=60){out.underlayRect(x,y,x+60,y+30,0x193347);out.terrainSource(id++,image,float(x+30),float(y+15),float(y+15),{},0);camera::Material material;material.nativeZ=true;out.blit(*image,float(x+30),float(y+15),1,1,float(y+15),{},0,0,material);}
}
static std::vector<uint32_t> render(camera::NativeCanvas&ground,int w,int h,double x,double y,double sx,double sy){
 std::vector<uint32_t> base,result;assert(ground.presentRegion(w,h,base,x,y,sx,sy,1));
 camera::ViewportCanvas canvas;assert(canvas.prepare(ground,base,w,h,x,y,sx,sy,1,colorprecision::Mode::NativeSteps,1));
 assert(canvas.finish(result));assert(result.size()==size_t(w)*h);return result;
}
int main(){
 xbrzadapter::gpuPresentation(false);auto image=tile();
 assert(resolutionsampling::choose(6000,3000,600,300).valid);assert(resolutionsampling::choose(32768,32768,128,128).valid);
 assert(!resolutionsampling::choose(32769,100,128,128).valid);assert(!resolutionsampling::choose(6000,3000,8192,8192).valid);assert(!resolutionsampling::choose(10,10,100,100).valid);
 camera::NativeCanvas old,retained;build(old,false,240,120,image);build(retained,true,240,120,image);
 assert(old.pageCount()>0&&retained.pageCount()==0&&retained.terrainSources().size()==old.terrainSources().size());
 // Full-size and integer half-size agree for opaque native TMP coverage. The
 // new path reconstructs Z/A from retained terrain, not from an empty world.
 auto native=render(old,240,120,0,0,240,120),direct=render(retained,240,120,0,0,240,120);assert(native==direct);
 std::vector<uint32_t> base,masked;assert(retained.presentRegion(240,120,base,0,0,240,120,1));camera::ViewportCanvas depth;
 assert(depth.prepare(retained,base,240,120,0,0,240,120,1,colorprecision::Mode::NativeSteps,1));
 auto recorded=depth.recordedGpuFrame();assert(recorded&&recorded->baseDepth&&recorded->baseDepth->size()==240*120&&recorded->baseDepthWidth==0&&recorded->nativeScene);
 auto priorDepth=recorded->baseDepth;assert(depth.prepare(retained,base,240,120,0,0,240,120,1,colorprecision::Mode::NativeSteps,1));assert(depth.recordedGpuFrame()->baseDepth==priorDepth);
 auto fog=std::make_shared<const std::vector<uint8_t>>(240*120,0);assert(depth.finish(masked,false,fog));
 assert(depth.nativeScene()&&depth.gpuMasked()&&depth.nativeAlpha()&&depth.nativeAlpha()->size()==240*120);
 assert(std::all_of(masked.begin(),masked.end(),[](auto p){return p==0xff000000u;}));
 assert(std::any_of(depth.nativeDepth().begin(),depth.nativeDepth().end(),[](auto z){return z!=65535;}));
 auto crop=render(retained,40,30,-20,-10,80,60);assert(crop.front()==0);assert(crop.back()!=0);
 auto first=retained.fork();retained.underlayRect(0,0,10,10,0xffffff);assert(first.retainedGround()&&first.pageCount()==0);assert(render(first,240,120,0,0,240,120)==direct);
 const int worldW=6000,worldH=3000,outW=600,outH=300;
 double start=now();camera::NativeCanvas largeOld;build(largeOld,false,worldW,worldH,image);double oldMs=now()-start;size_t oldPages=largeOld.pageCount();
 start=now();camera::NativeCanvas large;build(large,true,worldW,worldH,image);double newMs=now()-start;
 assert(large.pageCount()==0&&large.terrainSources().size()==10000);start=now();auto small=render(large,outW,outH,0,0,worldW,worldH);double rasterMs=now()-start;
 assert(small.size()==180000);assert(std::any_of(small.begin(),small.end(),[](auto p){return p==0xff80b040;}));
 // A >8192 source axis also remains legal when only a tiny output is allocated.
 camera::NativeCanvas giant;build(giant,true,12000,3000,image);assert(render(giant,120,30,0,0,12000,3000).size()==3600);
 unsigned reads=0;float value=2;uint32_t disguise=0;bool failRank=false,failDisguise=false;
 auto read=[&](uint32_t at,void*out,size_t n){++reads;if(at==0x10150&&!failRank){std::memcpy(out,&value,n);return true;}if(at==0x10518&&!failDisguise){std::memcpy(out,&disguise,n);return true;}return false;};
 assert(radar::captureRank(read,0x10000,1,1)==15);value=1;assert(radar::captureRank(read,0x10000,1,1)==14);value=.99f;assert(radar::captureRank(read,0x10000,1,1)==-1);value=-1;assert(radar::captureRank(read,0x10000,1,1)==19);
 value=2;disguise=0x20000;assert(radar::captureRank(read,0x10000,1,2)==-1);assert(radar::captureRank(read,0x10000,1,1)==15);disguise=0;assert(radar::captureRank(read,0x10000,1,2)==15);
 failRank=true;assert(radar::captureRank(read,0x10000,1,1)==-1);failRank=false;failDisguise=true;assert(radar::captureRank(read,0x10000,1,2)==-1);value=std::numeric_limits<float>::quiet_NaN();assert(radar::captureRank(read,0x10000,1,1)==-1);
 std::printf("PASS production retained ground: native parity, crop, immutable fork, large-world output, rank admission. Synthetic source benchmark: world=%dx%d output=%dx%d native_pages=%zu retained_pages=%zu native_build_ms=%.3f retained_build_ms=%.3f output_compose_ms=%.3f\n",worldW,worldH,outW,outH,oldPages,large.pageCount(),oldMs,newMs,rasterMs);
}
