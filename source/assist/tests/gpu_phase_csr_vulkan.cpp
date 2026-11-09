// Actual shader regression: original CSR and filtered CSR use separate retained
// backends. A common wrong filter therefore cannot pass via warm/cold reuse.
#define RA2_PHASE_CSR_TEST_ACCESS
#define main mixed_renderer_driver_main
#include "gpu_mixed_render_stress.cpp"
#undef main
#include "gpu_software_wait.h"

namespace {
namespace csr=external::phasecsr;
void candidateReport(const char*path,bool high,const std::vector<uint32_t>&refs,const std::vector<uint32_t>&starts,const std::vector<csr::Phase>&phases,uint32_t draws,const std::vector<uint32_t>&weights){
 const auto plan=csr::build(refs,starts,draws,phases);uint64_t old=0,current=0;unsigned parallel=0;
 for(size_t p=0;p<phases.size();++p)if(!phases[p].serial){++parallel;
  for(size_t b=0;b+1<starts.size();++b){const auto count=starts[b+1]-starts[b];old+=uint64_t(count)*weights[b];current+=uint64_t(plan.selected?plan.slices[p].counts[b]:count)*weights[b];}
 }
 std::cout<<"CANDIDATE path="<<path<<" tier="<<(high?"high":"medium")<<" draws="<<draws<<" phases="<<phases.size()<<" parallel="<<parallel<<" bins="<<weights.size()<<" refs="<<refs.size()<<" selected="<<plan.selected<<" extra_bytes="<<plan.extraWords*4<<" host_ref_visits="<<plan.sourceVisits<<" old_parallel_tests="<<old<<" new_parallel_tests="<<current<<" count_ratio="<<(current?double(old)/current:0)<<" reason="<<plan.reason<<std::endl;
 require(plan.selected,"representative workload should fit bounded phase metadata");require(plan.sourceVisits==refs.size()*2,"host phase filter must scan existing refs twice");
}
void hostCandidates(bool high){
 mixedstress::Workload w(high);w.update(20);const int width=high?2560:1280,height=high?1440:720;
 ViewPlan recorder;recorder.build(w,width,height,20);const auto&f=recorder.record.frame();
 std::vector<vulkancompute::scenebins::Bounds>bounds;for(const auto&d:f.draws)bounds.push_back(vulkancompute::drawBounds(d,f.width,f.height));
 auto phases=csr::partition(uint32_t(f.draws.size()),[&](uint32_t n){return vulkancompute::scenesources::active(bounds[n])&&f.draws[n].displace;});
 vulkancompute::scenebins::Plan bins;require(vulkancompute::scenebins::build(bounds,width,height,bins,8*1024*1024),"viewport host bin build failed");
 std::vector<uint32_t>weights;
 for(size_t b=0;b<bins.counts.size();++b){const int x=int(b%bins.cost.columns)*bins.cost.size,y=int(b/bins.cost.columns)*bins.cost.size;weights.push_back(uint32_t(std::min<int>(bins.cost.size,width-x)*std::min<int>(bins.cost.size,height-y)));}
 candidateReport("viewport",high,bins.references,bins.starts,phases,uint32_t(f.draws.size()),weights);
 camera::ObjectCanvas areaRecorder;gpuarea::Plan p;require(areaRecorder.planGPU(w.base,w.draws,512,256,w.width,w.height,p),"area host plan failed");
 require(p.nativeSurface&&p.objects,"mixed area must use production native object path");
 bounds.clear();for(const auto&d:*p.objects)bounds.push_back(vulkancompute::drawBounds(d,p.nativeWidth,p.nativeHeight));
 phases=csr::partition(uint32_t(p.objects->size()),[&](uint32_t n){return (*p.objects)[n].displace;});
 const size_t count=p.pages.size()*16;std::vector<uint32_t>counts(count),starts(count+1),refs;weights.assign(count,0);
 auto visit=[&](size_t job,uint32_t index,auto fn){const auto&page=p.pages[job];const auto&b=bounds[index];const int ox=int(page.x)*128,oy=int(page.y)*128;
  const int l=std::max(0,b[0]-ox),t=std::max(0,b[1]-oy),r=std::min(128,b[2]-ox),bottom=std::min(128,b[3]-oy);if(l>=r||t>=bottom)return;
  for(int y=t/32;y<=(bottom-1)/32;++y)for(int x=l/32;x<=(r-1)/32;++x)fn(job*16+y*4+x);
 };
 for(size_t j=0;j<p.pages.size();++j){for(auto n:p.pages[j].draws)visit(j,n,[&](size_t b){++counts[b];});
  for(int y=0;y<4;++y)for(int x=0;x<4;++x){const int left=int(p.pages[j].x)*128+x*32,top=int(p.pages[j].y)*128+y*32;weights[j*16+y*4+x]=uint32_t(std::max(0,std::min(32,p.nativeWidth-left))*std::max(0,std::min(32,p.nativeHeight-top)));}
 }
 for(size_t b=0;b<count;++b)starts[b+1]=starts[b]+counts[b];refs.resize(starts.back());auto next=starts;
 for(size_t j=0;j<p.pages.size();++j)for(auto n:p.pages[j].draws)visit(j,n,[&](size_t b){refs[next[b]++]=n;});
 candidateReport("area_all_pages",high,refs,starts,phases,uint32_t(p.objects->size()),weights);
}
void exactWords(const std::vector<uint32_t>&a,const std::vector<uint32_t>&b,const char*path,unsigned frame){
 require(a.size()==b.size(),std::string(path)+" size differs");
 for(size_t n=0;n<a.size();++n)if(a[n]!=b[n])throw std::runtime_error(std::string(path)+" original/filtered RGBA differs at frame "+std::to_string(frame)+" word "+std::to_string(n));
}
void viewportQuality(mixedstress::Workload&w){
 validationgpu::SlowSoftware<vulkancompute::FrameResolve> original,filtered;ViewPlan recorder;std::vector<uint32_t>a,b;
 for(unsigned frame:{20u,60u,130u}){
  w.update(frame);recorder.build(w,1280,720,frame);const auto&plan=recorder.record.frame();
  csr::testDisable(true);if(!original.renderFrame(plan,a))throw std::runtime_error("original viewport GPU failed: "+original.diagnostics().reason);
  require(csr::testState().builds>0&&csr::testState().selectedAppends==0,"original viewport CSR override not exercised");
  csr::testDisable(false);if(!filtered.renderFrame(plan,b))throw std::runtime_error("filtered viewport GPU failed: "+filtered.diagnostics().reason);
  require(csr::testState().selectedAppends==1,"viewport did not bind filtered phase CSR");exactWords(a,b,"viewport",frame);
  require(original.diagnostics().dispatches==filtered.diagnostics().dispatches,"viewport dispatch count changed");
  std::cout<<"GPU_EXACT path=viewport frame="<<frame<<" RGBA_words="<<a.size()<<" phase_metadata_bytes="<<csr::testState().appendedWords*4<<" dispatches="<<filtered.diagnostics().dispatches<<" device="<<filtered.diagnostics().device<<" software="<<filtered.diagnostics().softwareDevice<<std::endl;
 }
}
void areaQuality(mixedstress::Workload&w){
 validationgpu::SlowSoftware<vulkancompute::AreaResolve> original,filtered;camera::ObjectCanvas recorder;gpuarea::Plan p;std::vector<uint32_t>a,b;
 for(unsigned frame:{20u,60u,130u}){
  w.update(frame);require(recorder.planGPU(w.base,w.draws,512,256,w.width,w.height,p),"area quality recording failed");p.effects=w.effects(512,256,frame);
  csr::testDisable(true);if(!original.render(p,a))throw std::runtime_error("original area GPU failed: "+original.diagnostics().reason);
  require(csr::testState().builds>0&&csr::testState().selectedAppends==0,"original area CSR override not exercised");
  csr::testDisable(false);if(!filtered.render(p,b))throw std::runtime_error("filtered area GPU failed: "+filtered.diagnostics().reason);
  require(csr::testState().selectedAppends==1,"area did not bind filtered phase CSR");exactWords(a,b,"area",frame);
  require(original.diagnostics().dispatches==filtered.diagnostics().dispatches,"area dispatch count changed");
  std::cout<<"GPU_EXACT path=area frame="<<frame<<" RGBA_words="<<a.size()<<" phase_metadata_bytes="<<csr::testState().appendedWords*4<<" native_pages="<<filtered.prefixBuilds()<<" dispatches="<<filtered.diagnostics().dispatches<<std::endl;
 }
}
void encodedViewportQuality(){
 vulkancompute::FrameJob p;p.width=192;p.height=96;p.basePixels=std::make_shared<const std::vector<uint32_t>>(size_t(p.width)*p.height,0xff264052);
 for(uint32_t n=0;n<80;++n){vulkancompute::DrawJob d;d.source.width=d.source.height=24;d.source.pixels=std::make_shared<const std::vector<uint32_t>>(24*24,0xff000000u|(n*171723u&0xffffffu));d.left=32;d.top=20;d.skipZ=true;d.writeDepth=false;d.translucency=n%3;d.displace=n==25||n==55;d.cloakWords=d.displace?3:0;d.nativePitch=p.width;p.draws.push_back(d);}
 std::vector<uint32_t>packed(80);for(uint32_t n=0;n<80;++n)packed[n]=n;vulkancompute::encodeReadOnlyRuns(p.draws,packed,{0,80});require(std::any_of(packed.begin(),packed.end(),[](uint32_t x){return x>>16;}),"encoded fixture must contain read-only runs");
 validationgpu::SlowSoftware<vulkancompute::FrameResolve> original,filtered;std::vector<uint32_t>a,b;
 csr::testDisable(true);if(!original.renderFrame(p,a))throw std::runtime_error("original encoded viewport failed: "+original.diagnostics().reason);
 csr::testDisable(false);if(!filtered.renderFrame(p,b))throw std::runtime_error("filtered encoded viewport failed: "+filtered.diagnostics().reason);
 require(csr::testState().selectedAppends==1,"encoded viewport did not select filtered CSR");exactWords(a,b,"encoded viewport",0);
 std::cout<<"GPU_EXACT path=encoded_viewport RGBA_words="<<a.size()<<" phase_metadata_bytes="<<csr::testState().appendedWords*4<<std::endl;
}
}
int main(int argc,char**argv){try{
 hostCandidates(false);hostCandidates(true);if(argc>1&&std::string(argv[1])=="--host")return 0;
 mixedstress::Workload w(false);viewportQuality(w);areaQuality(w);encodedViewportQuality();csr::testDisable(false);
 std::cout<<"PASS actual Vulkan original-vs-phase-CSR: 3 moving frames each viewport/whole-map area plus encoded non-native viewport; exact RGBA, unchanged dispatch count, shader binaries unchanged"<<std::endl;return 0;
 }catch(const std::exception&e){csr::testDisable(false);std::cerr<<"FAIL phase CSR: "<<e.what()<<std::endl;return 1;}}
