#define RA2_SCENE_DISPATCH_TEST_ACCESS
#define main mixed_renderer_driver_main
#include "gpu_mixed_render_stress.cpp"
#undef main
#include "gpu_software_wait.h"
namespace {
void compare(validationgpu::SlowSoftware<vulkancompute::FrameResolve>&full,validationgpu::SlowSoftware<vulkancompute::FrameResolve>&local,const vulkancompute::FrameJob&frame,uint64_t&words,uint64_t&oldWork,uint64_t&newWork){
 std::vector<uint32_t>a,b;
 scenedispatch::forceFull()=true;if(!full.renderFrame(frame,a))throw std::runtime_error("full: "+full.diagnostics().reason);
 scenedispatch::forceFull()=false;if(!local.renderFrame(frame,b))throw std::runtime_error("region: "+local.diagnostics().reason);
 require(a==b,"full/region complete RGBA differs");require(full.diagnostics().readbacks==1&&local.diagnostics().readbacks==1,"readback contract changed");
 require(full.sceneInvocations()==full.fullSceneInvocations(),"full control inactive");
 require(local.sceneInvocations()<=local.fullSceneInvocations(),"region increased work");
 words+=a.size();oldWork+=full.sceneInvocations();newWork+=local.sceneInvocations();
}
void boundaries(uint64_t&words,uint64_t&oldWork,uint64_t&newWork){
 validationgpu::SlowSoftware<vulkancompute::FrameResolve> full,local;
 for(unsigned variant=0;variant<12;++variant){vulkancompute::FrameJob f;f.width=257;f.height=129;f.nativeScene=variant%2;f.clipLeft=variant%5;f.clipTop=variant%3;f.clipRight=254;f.clipBottom=127;
  auto bg=std::make_shared<std::vector<uint32_t>>(257*129);for(size_t i=0;i<bg->size();++i)(*bg)[i]=0xff000000u|uint32_t(i*77697u&0xffffffu);f.basePixels=bg;
  f.baseDepth=std::make_shared<const std::vector<float>>(bg->size(),-10.f);
  auto fog=std::make_shared<std::vector<uint8_t>>(bg->size(),127);for(size_t i=0;i<fog->size();i+=7)(*fog)[i]=31;f.fogAlpha=fog;
  for(unsigned n=0;n<90;++n){vulkancompute::DrawJob d;d.source.width=9;d.source.height=13;
   auto pixels=std::make_shared<std::vector<uint32_t>>(9*13);for(size_t j=0;j<pixels->size();++j)(*pixels)[j]=j%9?0xff000000u|(n*171723u&0xffffffu):0;d.source.pixels=pixels;
   d.left=float((n*17+variant*3)%280)-12.25f;d.top=float((n*11+variant*7)%151)-12.5f;
   d.writeDepth=n%3==0;d.skipZ=true;d.flat=true;d.depthBase=30;
   d.displace=n%9==0||n==1||n==89;d.cloakWords=d.displace?(n%2?3:-3):0;d.nativePitch=f.width;
   d.nativeDepth.enabled=f.nativeScene;d.translucency=n%4;f.draws.push_back(d);
  }
  compare(full,local,f,words,oldWork,newWork);compare(full,local,f,words,oldWork,newWork);
 }
}
void mixed(bool high,uint64_t&words,uint64_t&oldWork,uint64_t&newWork){
 mixedstress::Workload w(high);ViewPlan plan;validationgpu::SlowSoftware<vulkancompute::FrameResolve> full,local;
 const int width=high?2560:1280,height=high?1440:720;
 for(unsigned tick:{20u,21u,60u,130u}){w.update(tick);plan.build(w,width,height,tick);
  const auto a=oldWork,b=newWork;compare(full,local,plan.record.frame(),words,oldWork,newWork);
  require(local.sceneInvocations()<local.fullSceneInvocations(),"mixed workload did not reduce dispatch domain");
  std::cout<<"MIXED_EXACT tier="<<(high?"high":"medium")<<" tick="<<tick<<" size="<<width<<'x'<<height<<" draws="<<plan.record.frame().draws.size()<<" old_invocations="<<oldWork-a<<" new_invocations="<<newWork-b<<" full_ms="<<full.diagnostics().milliseconds<<" region_ms="<<local.diagnostics().milliseconds<<" software="<<local.diagnostics().softwareDevice<<std::endl;
 }
}
}
int main(){try{uint64_t words=0,oldWork=0,newWork=0;boundaries(words,oldWork,newWork);mixed(false,words,oldWork,newWork);mixed(true,words,oldWork,newWork);
 std::cout<<"PASS actual Vulkan phase regions: frames=32 exact_RGBA_words="<<words<<" old_invocations="<<oldWork<<" new_invocations="<<newWork<<"; generated terrain/SHP/VXL/animation/effects, warm frames and lighting changes; software GPU, not hardware FPS\n";return 0;
 }catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}}
