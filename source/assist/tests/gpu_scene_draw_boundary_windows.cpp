#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../../external_reader/source/gpu_scene_record.h"
#include <cassert>
#include <cstring>
#include <cstdio>
int main(){
 using namespace external;
 auto base=std::make_shared<const std::vector<uint32_t>>(256*256,0xff000000u);
 auto depth=std::make_shared<const std::vector<float>>(256*256,-100000.f);
 auto white=std::make_shared<voxel::Image>();white->width=white->height=1;white->pixels={0xffffffffu};
 auto green=std::make_shared<voxel::Image>();green->width=green->height=1;green->pixels={0xff00ff00u};
 camera::GpuSceneRecord record;record.begin(256,256,base,depth,colorprecision::Mode::SmoothRGB);lighting::State light;
 for(unsigned n=0;n<65536;++n)assert(record.append(*white,white,float(n%256),float(n/256),1,1,float(n),light,1,0,false));
 vulkancompute::FrameResolve gpu;std::vector<uint32_t>pixels;
 if(!gpu.renderFrame(record.frame(),pixels)){std::fprintf(stderr,"65536 GPU admission failed: %s\n",gpu.diagnostics().reason.c_str());return 2;}
 assert(pixels.size()==256*256&&pixels.front()==0xffffffffu&&pixels.back()==0xffffffffu);
 const auto completed=gpu.diagnostics().frames;
 assert(record.append(*green,green,255,255,1,1,65536.f,light,1,0,false));
 assert(record.frame().draws.size()==65537);assert(!gpu.renderFrame(record.frame(),pixels));
 assert(gpu.diagnostics().frames==completed);
 assert(gpu.diagnostics().reason.find("16-bit")!=std::string::npos);
 camera::Surface surface;camera::cpureplay::Stats stats;
 assert(record.replay(surface,&stats));assert(surface.color.size()==256*256&&surface.color.front()==0xffffffffu&&surface.color.back()==0xff00ff00u);
 assert(stats.serialCalls==65537);
 std::printf("{\"passed\":true,\"gpu_65536_accepted\":true,\"gpu_65537_rejected_before_submit\":true,\"cpu_replayed_calls\":%llu,\"final_pixel\":\"ff00ff00\",\"game_process_accessed\":false}\n",static_cast<unsigned long long>(stats.serialCalls));
}
