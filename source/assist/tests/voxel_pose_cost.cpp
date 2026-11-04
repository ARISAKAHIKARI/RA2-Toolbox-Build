#include "gpu_voxel_cases.h"
#include <chrono>
#include <cstdio>
// Build this same fixture against the old and new trees: digest covers actual
// image/depth/index values and prepared GPU triangles, without loading a GPU.
int main(){
 using namespace external;uint64_t digest=1469598103934665603ull,pixels=0,triangles=0;
 auto hash=[&](const void*data,size_t bytes){auto p=static_cast<const uint8_t*>(data);for(size_t i=0;i<bytes;++i)digest=(digest^p[i])*1099511628211ull;};
 for(const auto&test:gpuvoxeltest::cases(256)){
  const auto&i=test.expected;for(int v:{i.width,i.height,i.x,i.y,i.depthBottom,i.rasterScale})hash(&v,sizeof v);
  hash(i.pixels.data(),i.pixels.size()*4);hash(i.depth.data(),i.depth.size()*4);hash(i.indices.data(),i.indices.size());
  hash(test.prepared.triangles.data(),test.prepared.triangles.size()*sizeof(gpuvoxel::Triangle));
  pixels+=i.pixels.size();triangles+=test.prepared.triangles.size();
 }
 auto small=gpuvoxeltest::model(),geometry=small;for(int n=0;n<63;++n)geometry.faces.insert(geometry.faces.end(),small.faces.begin(),small.faces.end());
 auto vpl=gpuvoxeltest::vpl();std::array<uint32_t,256> palette;palette.fill(0xff80b060);
 std::vector<double>times;uint64_t prepared=0;
 for(int repeat=0;repeat<5;++repeat){auto start=std::chrono::steady_clock::now();
  for(int n=0;n<64;++n){gpuvoxel::Prepared p;assert(gpuvoxel::prepare(geometry,n*.023,palette,p,n*.051,3.5f,&vpl,255,.13,true,1));prepared+=p.triangles.size();}
  times.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());
 }
 std::sort(times.begin(),times.end());
 std::printf("{\"status\":\"PASS\",\"digest\":\"%016llx\",\"pixels\":%llu,\"triangles\":%llu,\"faces_per_model\":%zu,\"poses\":64,\"median_prepare_ms\":%.3f,\"prepared\":%llu,\"scope\":\"CPU pose preparation; no Vulkan\"}\n",(unsigned long long)digest,(unsigned long long)pixels,(unsigned long long)triangles,geometry.faces.size(),times[2],(unsigned long long)prepared);
}
