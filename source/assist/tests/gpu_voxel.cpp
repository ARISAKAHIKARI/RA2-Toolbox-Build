#include "gpu_voxel_cases.h"
#include <cstdio>
int main(){
 using namespace external;
 uint64_t pixels=0,triangles=0,candidates=0;
 for(auto&test:gpuvoxeltest::cases()){
  auto&p=test.prepared;auto&e=test.expected;voxel::Image actual;assert(gpuvoxel::rasterReference(p,actual));
  assert(actual.width==e.width&&actual.height==e.height&&actual.x==e.x&&actual.y==e.y&&actual.depthBottom==e.depthBottom&&actual.rasterScale==e.rasterScale&&actual.objectPalette==e.objectPalette);
  assert(actual.pixels==e.pixels&&actual.indices==e.indices&&actual.depth==e.depth);
  for(size_t n=0;n<p.triangles.size();++n)assert(p.triangles[n].material[2]==n);
  pixels+=e.pixels.size();triangles+=p.triangles.size();candidates+=p.candidatePixels;
 }
 uint32_t previous=0;for(float z:{-10000.f,-2.f,-1.f,-0.f,0.f,.5f,1.f,10000.f}){auto key=gpuvoxel::orderedDepth(z);assert(key>=previous);previous=key;assert(gpuvoxel::decodedDepth(key)==z);}
 assert(gpuvoxel::orderedDepth(-0.f)==gpuvoxel::orderedDepth(0.f));
 voxel::Model empty;std::array<uint32_t,256> palette{};gpuvoxel::Prepared bad;
 assert(!gpuvoxel::prepare(empty,0,palette,bad));
 auto m=gpuvoxeltest::model();voxelposture::State unavailable;unavailable.available=false;
 assert(!gpuvoxel::prepare(m,0,palette,bad,0,0,nullptr,255,0,false,1,&unavailable));
 std::printf("PASS: GPU voxel packet/native CPU oracle 96 poses, pixels=%llu triangles=%llu candidatepixels=%llu; VPL/fallback/projectile/turret/posture/scales1-4/alpha/ties, ordered depth\n",(unsigned long long)pixels,(unsigned long long)triangles,(unsigned long long)candidates);
}
