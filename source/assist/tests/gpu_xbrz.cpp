#include "../../external_reader/source/vulkan_scene.h"
#include "../../external_reader/source/gpu_xbrz_reference.h"
#include "../../external_reader/source/gpu_xbrz_selfcheck.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <new>
#include <string>

using namespace external::vulkancompute;
namespace oracle=external::gpuxbrztest;
static void require(bool condition,const char* message){
 if(!condition){std::fprintf(stderr,"FAIL GPU xBRZ: %s\n",message);std::exit(2);}
}
static TileJob job(const oracle::Case& c,int factor){
 TileJob j;j.pixels=c.source;j.sourceWidth=c.width;j.sourceHeight=c.height;
 j.left=j.top=0;j.width=c.width*factor;j.height=c.height*factor;
 j.sourceX=j.sourceY=0;j.stepX=j.stepY=1;j.reconstructionFactor=factor;
 return j;
}
static void compare(const oracle::Case& c,int factor,const std::vector<uint32_t>& expected,const std::vector<uint32_t>& actual,uint64_t& checked){
 require(actual.size()==expected.size(),"output pixel count");
 for(size_t i=0;i<expected.size();++i){
  if(actual[i]!=expected[i]){
   std::fprintf(stderr,"FAIL GPU xBRZ %s %dx%d factor%d output(%zu,%zu) expected=%08x actual=%08x\n",c.label.c_str(),c.width,c.height,factor,i%size_t(c.width*factor),i/size_t(c.width*factor),expected[i],actual[i]);std::exit(2);
  }
  const unsigned alpha=actual[i]>>24;
  require((actual[i]&255)<=alpha&&((actual[i]>>8)&255)<=alpha&&((actual[i]>>16)&255)<=alpha,"premultiplied output invariant");
  ++checked;
 }
}
int main(int argc,char** argv){
 const bool required=argc>1&&std::string(argv[1])=="--require";
 {
  using namespace external::gpuxbrz;
  require(sizeof(SelfCheckSource)/sizeof(uint32_t)==size_t(SelfCheckWidth)*SelfCheckHeight,"startup source dimensions");
  const std::vector<uint32_t> source(SelfCheckSource,SelfCheckSource+SelfCheckWidth*SelfCheckHeight);
  for(int factor:{2,3,4}){const auto expected=oracle::upstream(source,SelfCheckWidth,SelfCheckHeight,factor);
   const uint32_t* golden=factor==2?SelfCheck2:factor==3?SelfCheck3:SelfCheck4;
   const size_t count=factor==2?sizeof(SelfCheck2)/sizeof(uint32_t):factor==3?sizeof(SelfCheck3)/sizeof(uint32_t):sizeof(SelfCheck4)/sizeof(uint32_t);
   require(count==expected.size(),"startup golden dimensions");
   require(std::equal(expected.begin(),expected.end(),golden),"startup self-check golden differs from this platform's unmodified upstream");
  }
  std::puts("PASS: startup Float64 xBRZ device-check goldens match this platform's unmodified upstream at2/3/4x");
 }
 FrameResolve gpu;std::vector<uint32_t> output{0x12345678};uint64_t checked=0,frames=0,builds=0,warmFrames=0;
 const auto cases=oracle::cases();
 // The host must reject invalid native reconstruction descriptors without
 // changing caller output or attempting a partial readback.
 auto invalid=job(cases.front(),2);invalid.reconstructionFactor=1;
 require(!gpu.render(invalid.width,invalid.height,{invalid},output)&&output==std::vector<uint32_t>{0x12345678},"factor1 rejection preserves output");
 invalid=job(cases.front(),2);invalid.sourceWidth=0;
 require(!gpu.render(invalid.width,invalid.height,{invalid},output)&&output==std::vector<uint32_t>{0x12345678},"zero native width rejection preserves output");
 invalid=job(cases.front(),2);invalid.reconstructionFactor=5;
 require(!gpu.render(invalid.width,invalid.height,{invalid},output)&&output==std::vector<uint32_t>{0x12345678},"factor5 rejection preserves output");
 for(size_t n=0;n<cases.size();++n)for(int factor:{2,3,4}){
  const auto& c=cases[n];const auto j=job(c,factor);
  const auto expected=oracle::upstream(*c.source,c.width,c.height,factor);
  if(!gpu.render(j.width,j.height,{j},output)){
   if(frames==0){std::printf("GPU_XBRZ_UNAVAILABLE float64=%d reason=%s\n",gpu.diagnostics().xbrzAvailable,gpu.diagnostics().reason.c_str());return required?1:0;}
   std::fprintf(stderr,"FAIL GPU xBRZ mid-run unavailable %s factor%d reason=%s\n",c.label.c_str(),factor,gpu.diagnostics().reason.c_str());return 2;
  }
  require(gpu.diagnostics().xbrzAvailable,"native reconstruction did not use Float64 xBRZ pipeline");
  compare(c,factor,expected,output,checked);++frames;
  require(gpu.diagnostics().readbacks==1,"one final readback per completed frame");
  require(gpu.diagnostics().xbrzBuilds==1,"fresh raw owner/factor reconstructs exactly once");
  require(gpu.diagnostics().uploadedBytes==(factor==2?c.source->size()*4:0),"immutable raw source uploads once across reconstruction factors");
  builds+=gpu.diagnostics().xbrzBuilds;
  if(n%47==0||(n+2>=cases.size()&&factor==4)){
   require(gpu.render(j.width,j.height,{j},output),"warm native tile resolve");compare(c,factor,expected,output,checked);++frames;++warmFrames;
   require(gpu.diagnostics().uploads==0&&gpu.diagnostics().uploadedBytes==0,"warm native tile uploads zero bytes");
   require(gpu.diagnostics().xbrzBuilds==0,"warm native tile does not reconstruct again");
   require(gpu.diagnostics().readbacks==1,"warm frame has one final readback");
  }
 }
 // Reusing an owner's raw vector address with different shared ownership must
 // not reuse its old reconstructed atlas entry. The old vector is destroyed
 // before placement-new reconstructs the object at the same address.
 alignas(std::vector<uint32_t>) unsigned char storage[sizeof(std::vector<uint32_t>)];
 auto owner=[&](uint32_t color){auto* p=new(storage) std::vector<uint32_t>(9,color);return std::shared_ptr<const std::vector<uint32_t>>(p,[](const std::vector<uint32_t>*v){const_cast<std::vector<uint32_t>*>(v)->~vector();});};
 oracle::Case reused{"reused-owner",3,3,owner(0xff204060)};auto reusedJob=job(reused,3);const void* address=reused.source.get();
 require(gpu.render(9,9,{reusedJob},output),"first reused address");compare(reused,3,oracle::upstream(*reused.source,3,3,3),output,checked);++frames;
 reusedJob.pixels.reset();reused.source.reset();reused.source=owner(0xffc08122);require(reused.source.get()==address,"forced raw vector address reuse");reusedJob=job(reused,3);
 require(gpu.render(9,9,{reusedJob},output),"second reused address");compare(reused,3,oracle::upstream(*reused.source,3,3,3),output,checked);++frames;
 require(gpu.diagnostics().xbrzBuilds==1&&gpu.diagnostics().uploadedBytes==36,"new owner rebuilds despite identical address");
 const auto previous=output;invalid=reusedJob;invalid.sourceHeight=4;
 require(!gpu.render(9,9,{invalid},output)&&output==previous,"bad native source storage preserves output");
 invalid=reusedJob;invalid.stepX=std::numeric_limits<double>::infinity();
 require(!gpu.render(9,9,{invalid},output)&&output==previous,"bad coordinate preserves output");
 require(gpu.render(9,9,{reusedJob},output),"invalid descriptor does not poison backend");
 require(gpu.diagnostics().xbrzBuilds==0&&gpu.diagnostics().uploads==0,"retained entry survives rejected descriptor");
 compare(reused,3,oracle::upstream(*reused.source,3,3,3),output,checked);++frames;
 std::printf("PASS GPU xBRZ actual compute cases=%zu factors2,3,4 frames=%llu exact_upstream_pixels=%llu fresh_builds=%llu warm_frames=%llu warm_uploads=0/warm_builds=0 final_readbacks=1 software=%d device=%s; not hardware performance proof\n",cases.size(),(unsigned long long)frames,(unsigned long long)checked,(unsigned long long)builds,(unsigned long long)warmFrames,gpu.diagnostics().softwareDevice,gpu.diagnostics().device.c_str());
}
