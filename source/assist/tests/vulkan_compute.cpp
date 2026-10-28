#include "../../external_reader/source/vulkan_compute.h"
#include <cassert>
#include <iostream>
#include <random>
#include <string>
using namespace external::vulkancompute;
static uint32_t reference(uint32_t a,uint32_t b,uint32_t c,uint32_t d,unsigned wx,unsigned wy){
 uint32_t result=0;for(unsigned shift=0;shift<32;shift+=8){unsigned top=((a>>shift)&255)*(256-wx)+((b>>shift)&255)*wx,bottom=((c>>shift)&255)*(256-wx)+((d>>shift)&255)*wx;result|=((top*(256-wy)+bottom*wy+32768)>>16)<<shift;}return result;
}
static uint32_t expected(const TileJob&j,int x,int y){
 double px=std::clamp(j.sourceX+x*j.stepX,0.,double(j.sourceWidth-1)),py=std::clamp(j.sourceY+y*j.stepY,0.,double(j.sourceHeight-1));
 int ax=int(px),ay=int(py),bx=std::min(ax+1,j.sourceWidth-1),by=std::min(ay+1,j.sourceHeight-1);
 const auto&p=*j.pixels;return reference(p[size_t(ay)*j.sourceWidth+ax],p[size_t(ay)*j.sourceWidth+bx],p[size_t(by)*j.sourceWidth+ax],p[size_t(by)*j.sourceWidth+bx],unsigned((px-ax)*256+.5),unsigned((py-ay)*256+.5));
}
static std::shared_ptr<const std::vector<uint32_t>> image(int w,int h,std::mt19937&r){auto p=std::make_shared<std::vector<uint32_t>>(size_t(w)*h);for(auto&v:*p){unsigned a=r()%256;v=(a<<24)|((r()%(a+1))<<16)|((r()%(a+1))<<8)|(r()%(a+1));}return p;}
int main(int argc,char**argv){
 bool required=argc>1&&std::string(argv[1])=="--require";Resolve gpu;std::vector<uint32_t> output{0x12345678};std::mt19937 random(20261029);
 assert(!gpu.render(0,1,{},output)&&output==std::vector<uint32_t>{0x12345678});
 auto first=image(79,63,random),second=image(113,91,random);
 TileJob a{first,79,63,0,0,127,95,-1.4,.013,79./127,63./95},b{second,113,91,127,0,129,95,.503,91.2,113./129,-91./95};
 std::vector<TileJob> jobs{a,b};
 std::cerr<<"BEGIN first Vulkan render"<<std::endl;
 if(!gpu.render(256,95,jobs,output)){
  std::cout<<"VULKAN_UNAVAILABLE "<<gpu.diagnostics().reason<<std::endl;return required?1:0;
 }
 uint64_t checked=0,frames=0;
 auto check=[&](){for(const auto&j:jobs)for(int y=0;y<j.height;++y)for(int x=0;x<j.width;++x){uint32_t actual=output[size_t(j.top+y)*256+j.left+x];assert(actual==expected(j,x,y));unsigned alpha=actual>>24;assert((actual&255)<=alpha&&((actual>>8)&255)<=alpha&&((actual>>16)&255)<=alpha);++checked;}assert(gpu.diagnostics().readbacks==1&&gpu.diagnostics().dispatches==jobs.size());++frames;};
 check();assert(gpu.diagnostics().uploads==2&&gpu.diagnostics().uploadedBytes==(79*63+113*91)*4);
 for(int n=0;n<48;++n){
  jobs[0].sourceX=(int(random()%200)-50)/32.;jobs[0].sourceY=(int(random()%200)-50)/37.;jobs[1].sourceX=(int(random()%200)-50)/29.;jobs[1].sourceY=(int(random()%400)-50)/3.;
  assert(gpu.render(256,95,jobs,output));assert(gpu.diagnostics().uploads==0&&gpu.diagnostics().uploadedBytes==0);check();
 }
 // Changing one immutable owner uploads just that tile; preserving another
 // owner across address churn must retain its GPU contents without an upload.
 jobs[0].pixels=image(79,63,random);first.reset();
 assert(gpu.render(256,95,jobs,output));check();assert(gpu.diagnostics().uploads==1&&gpu.diagnostics().uploadedBytes==79*63*4);
 for(int n=0;n<8;++n){jobs[0].pixels=image(79,63,random);assert(gpu.render(256,95,jobs,output));check();assert(gpu.diagnostics().uploads==1);}
 const auto previous=output;auto invalid=jobs;invalid[1].left=126;assert(!gpu.render(256,95,invalid,output)&&output==previous);
 invalid=jobs;invalid[0].stepX=std::numeric_limits<double>::infinity();assert(!gpu.render(256,95,invalid,output)&&output==previous);
 invalid=jobs;invalid[0].sourceWidth=80;assert(!gpu.render(256,95,invalid,output)&&output==previous);
 // Rejected input does not poison the initialized backend.
 assert(gpu.render(256,95,jobs,output));check();assert(gpu.diagnostics().uploads==0);
 std::cout<<"PASS Vulkan actual compute frames="<<frames<<" exact_Q8_pixels="<<checked<<" warm_uploads=0 final_readbacks=1 software="<<gpu.diagnostics().softwareDevice<<" device="<<gpu.diagnostics().device<<" last_frame_ms="<<gpu.diagnostics().milliseconds<<"\n";
}
