#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../../external_reader/source/gpu_scene_sources.h"
#include <cassert>
#include <chrono>
#include <cstdio>
#include <random>
using namespace external;
using namespace external::vulkancompute;
struct Plan {
 std::vector<immutableatlas::Source>colors,materials;
 std::vector<scenesources::Indices>sources;
 std::vector<scenesources::Bounds>bounds;
 scenesources::Stats stats;
 const char*build(const FrameJob&f){colors.clear();materials.clear();return scenesources::append(f,colors,materials,sources,bounds,stats);}
};
static DrawJob sprite(int size=8){DrawJob d;d.source.width=d.source.height=size;d.source.pixels=std::make_shared<const std::vector<uint32_t>>(size_t(size)*size,0xff123456);return d;}
static bool inside(const scenesources::Bounds&b,int x,int y){return x>=b[0]&&y>=b[1]&&x<b[2]&&y<b[3];}
static double ms(){return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();}
int main(){
 FrameJob f;f.width=f.height=32;f.draws={sprite()};Plan p;assert(!p.build(f)&&p.stats.admitted==1&&p.colors.size()==1);
 // Fractional positions/scale and the existing half-pixel clipBottom rule:
 // every pixel admitted by the old bounds AND shader clip remains admitted.
 std::mt19937 rng(17);std::uniform_real_distribution<float>pos(-12,42),scale(.05f,8);
 for(int n=0;n<2000;++n){auto&d=f.draws[0];d.left=pos(rng);d.top=pos(rng);d.scaleX=scale(rng);d.scaleY=scale(rng);d.clipBottom=pos(rng);f.clipLeft=int(rng()%40)-4;f.clipTop=int(rng()%40)-4;f.clipRight=int(rng()%40)-4;f.clipBottom=int(rng()%40)-4;
  assert(!p.build(f));const auto old=drawBounds(d,32,32),now=p.bounds[0];
  for(int y=0;y<32;++y)for(int x=0;x<32;++x){bool clip=x>=std::clamp(f.clipLeft,0,32)&&y>=std::clamp(f.clipTop,0,32)&&x<(f.clipRight<0?32:std::clamp(f.clipRight,0,32))&&y<(f.clipBottom<0?32:std::clamp(f.clipBottom,0,32));assert(inside(now,x,y)==(inside(old,x,y)&&clip));}
 }
 f={};f.width=f.height=32;auto d=sprite();d.left=100;d.sampling=Sampling::Bilinear;d.displace=true;f.draws={d};assert(!p.build(f)&&p.stats.culled==1&&!p.stats.needs64&&!p.stats.displacement&&p.colors.empty());for(auto i:p.sources[0])assert(i==scenesources::Missing);
 d.left=0;d.clipBottom=.5f;f.draws={d};assert(!p.build(f)&&p.stats.culled==1);
 d.clipBottom=.5001f;f.draws={d};assert(!p.build(f)&&p.stats.admitted==1&&p.stats.displacement);
 // Still reject malformed offscreen packets; do not make clipping a way to
 // admit NaN, negative scales, invalid palette or reconstruction dimensions.
 d.left=100;d.scaleX=-1;f.draws={d};assert(p.build(f));d.scaleX=1;d.top=std::numeric_limits<float>::quiet_NaN();f.draws={d};assert(p.build(f));d.top=0;d.palette.rows=1;d.palette.table=std::make_shared<const std::vector<uint32_t>>(255);f.draws={d};assert(p.build(f));d.palette.table.reset();d.sampling=Sampling::Reconstructed;f.draws={d};assert(p.build(f));
 // Fog is not an occlusion proof. Keep a geometrically visible sprite even
 // under a fully black mask, and preserve original IDs around omitted draws.
 f={};f.width=f.height=32;f.fogAlpha=std::make_shared<const std::vector<uint8_t>>(1024,127);f.draws={sprite(),sprite(),sprite()};f.draws[1].left=40;f.draws[2].displace=true;assert(!p.build(f)&&p.stats.admitted==2&&p.stats.culled==1&&p.sources.size()==3&&p.stats.displacement);assert(p.sources[0][0]==0&&p.sources[1][0]==scenesources::Missing&&p.sources[2][0]==1);
 std::vector<uint32_t>refs{0,2},starts{0,2};encodeReadOnlyRuns(f.draws,refs,starts);assert((refs==std::vector<uint32_t>{0,2}));
 // Production source/atlas planner: 300 unique SHP packets, 300 raw xBRZ
 // packets, 300 VXL rasters. Only 30 per class overlap the final clip.
 f={};f.width=512;f.height=128;f.clipRight=128;
 for(int kind=0;kind<3;++kind)for(int n=0;n<300;++n){auto job=sprite(64);job.left=n<30?0:256;
  if(kind==1){job.sampling=Sampling::Reconstructed;job.reconstructionSource=job.source.pixels;job.reconstructionFactor=2;job.reconstructedWidth=job.reconstructedHeight=128;}
  if(kind==2){auto packet=std::make_shared<gpuvoxel::Prepared>();packet->width=packet->height=64;job.gpuRaster=packet;job.source.pixels.reset();}
  f.draws.push_back(job);
 }
 auto workload=[&](FrameJob&frame,int rounds,size_t&colorBytes,size_t&materialBytes,size_t&voxel,size_t&xbrz){double start=ms();for(int k=0;k<rounds;++k){assert(!p.build(frame));immutableatlas::Plan cp,mp;assert(immutableatlas::build(p.colors,{},cp,128ull*1024*1024));assert(immutableatlas::build(p.materials,{},mp,96ull*1024*1024));colorBytes=cp.bytes;materialBytes=mp.bytes;voxel=xbrz=0;for(size_t i=0;i<frame.draws.size();++i)if(scenesources::active(p.bounds[i])){voxel+=bool(frame.draws[i].gpuRaster);xbrz+=bool(frame.draws[i].reconstructionSource);}}return (ms()-start)/rounds;};
 size_t oldC,oldM,oldV,oldX,newC,newM,newV,newX;FrameJob before=f;before.clipRight=-1;
 auto oldMs=workload(before,40,oldC,oldM,oldV,oldX),newMs=workload(f,40,newC,newM,newV,newX);
 assert(p.stats.admitted==90&&p.stats.culled==810&&newV==30&&newX==30&&oldV==300&&oldX==300&&newC*10==oldC&&newM==0&&oldM==0);
 // Moving the same packets into view must restore their sources; no negative
 // cache may survive the empty interval. Atlas identity dedup still applies.
 f.clipRight=-1;assert(!p.build(f)&&p.stats.admitted==900);f.draws.push_back(f.draws[0]);assert(!p.build(f));immutableatlas::Plan cp;assert(immutableatlas::build(p.colors,{},cp,128ull*1024*1024));assert(cp.offsets.back()==cp.offsets.front());
 std::printf("PASS GPU scene admission: 900 -> 90 admitted, VXL jobs %zu -> %zu, xBRZ jobs %zu -> %zu, source upload bytes %zu -> %zu; CPU source/atlas plan %.3f -> %.3f ms/frame (synthetic, no GPU execution)\n",oldV,newV,oldX,newX,oldC+oldM,newC+newM,oldMs,newMs);
}
