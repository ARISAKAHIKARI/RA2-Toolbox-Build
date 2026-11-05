#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../../external_reader/source/gpu_scene_bins.h"
#include <cassert>
#include <chrono>
#include <cstdio>
#include <random>
using namespace external::vulkancompute::scenebins;
static uint64_t checks=0;
static bool inside(const Bounds&b,int x,int y){return b[0]<=x&&x<b[2]&&b[1]<=y&&y<b[3];}
static void verify(const std::vector<Bounds>&bounds,int w,int h,Bounds clip={0,0,-1,-1}){
 Plan p;assert(build(bounds,w,h,p,2*1024*1024,clip));++checks;
 uint64_t work=0;clip[2]=clip[2]<0?w:clip[2];clip[3]=clip[3]<0?h:clip[3];
 for(int y=0;y<h;++y)for(int x=0;x<w;++x){const size_t bin=size_t(y/p.cost.size)*p.cost.columns+x/p.cost.size;std::vector<uint32_t>got,expected;
  for(auto at=p.starts[bin];at<p.starts[bin+1];++at){auto n=p.references[at];if(inside(bounds[n],x,y))got.push_back(n);}
  for(size_t n=0;n<bounds.size();++n)if(inside(bounds[n],x,y))expected.push_back(uint32_t(n));
  assert(got==expected);++checks;
  if(inside(clip,x,y))work+=p.counts[bin];
 }
 assert(work==p.cost.pixelVisits&&p.references.size()==p.cost.references);++checks;
 for(auto&n:p.references)n|=0x12340000u;assert(build(bounds,w,h,p,2*1024*1024,clip));for(auto n:p.references)assert(n<bounds.size());++checks;
}
static std::vector<Bounds>grid(int nx,int ny,int tw,int th){std::vector<Bounds>b;for(int y=0;y<ny;++y)for(int x=0;x<nx;++x)b.push_back({x*tw,y*th,(x+1)*tw,(y+1)*th});return b;}
static double now(){return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();}
// The exact original 32px CSR algorithm (pre-c9 optimization) for measuring
// CPU construction costs independently of shader work and driver latency.
static void before(const std::vector<Bounds>&bounds,int w,int h,Plan&out){
 const uint32_t cols=(uint32_t(w)+31)/32,rows=(uint32_t(h)+31)/32;const size_t bins=size_t(cols)*rows;
 std::vector<uint32_t>counts(bins,0),starts(bins+1,0);
 auto visit=[&](size_t n,auto&&fn){const auto&r=bounds[n];if(r[0]>=r[2]||r[1]>=r[3])return;for(int y=r[1]/32;y<=(r[3]-1)/32;++y)for(int x=r[0]/32;x<=(r[2]-1)/32;++x)fn(size_t(y)*cols+x);};
 for(size_t n=0;n<bounds.size();++n)visit(n,[&](size_t bin){++counts[bin];});
 uint64_t refs=0;for(size_t bin=0;bin<bins;++bin){starts[bin]=uint32_t(refs);refs+=counts[bin];}starts[bins]=uint32_t(refs);
 std::vector<uint32_t>references(size_t(refs),0);auto positions=starts;for(size_t n=0;n<bounds.size();++n)visit(n,[&](size_t bin){references[positions[bin]++]=uint32_t(n);});
 out.counts=std::move(counts);out.starts=std::move(starts);out.references=std::move(references);
}
int main(){
 std::mt19937 rng(19910831);for(int trial=0;trial<80;++trial){int w=1+int(rng()%69),h=1+int(rng()%55);std::vector<Bounds>b;
  for(int n=0;n<40;++n){int x=int(rng()%(w+1)),y=int(rng()%(h+1)),r=int(rng()%(w+1)),bottom=int(rng()%(h+1));b.push_back({x,y,std::max(x,r),std::max(y,bottom)});}
  verify(b,w,h);verify(b,w,h,{w/4,h/4,w-w/5,h-h/5});
 }
 Plan p;assert(!build({},0,16,p,2097152));assert(!build({{-1,0,2,2}},16,16,p,2097152));assert(!build({{0,0,17,2}},16,16,p,2097152));assert(!build({},16,16,p,0x100000000ull));checks+=4;
 auto dense=std::vector<Bounds>(65536,{0,0,1,1});assert(build(dense,1,1,p,2097152)&&p.references.back()==65535);++checks;dense.push_back({0,0,1,1});assert(!build(dense,1,1,p,2097152));++checks;
 assert(!build(std::vector<Bounds>(1000,{0,0,512,512}),512,512,p,4096));++checks;
 assert(build({{0,0,1,1}},8192,8192,p,2097152)&&p.cost.size==32);++checks;
 struct Case{const char*name;int w,h,nx,ny,tw,th,expected;};const Case cases[]={{"minimap",600,300,100,100,6,3,8},{"zoomout",960,540,34,36,26,14,16},{"fullres",1920,1080,30,30,60,30,32}};
 std::printf("{\"passed\":true,\"checks\":%llu,\"cases\":[",static_cast<unsigned long long>(checks));bool first=true;
 for(auto c:cases){auto b=grid(c.nx,c.ny,c.tw,c.th);const auto old=measure(b,c.w,c.h,32);assert(build(b,c.w,c.h,p,2097152)&&p.cost.size==uint32_t(c.expected));assert(p.cost.references<=old.references*2);++checks;
  double at=now();for(int n=0;n<100;++n)before(b,c.w,c.h,p);const double oldMs=(now()-at)/100;
  at=now();for(int n=0;n<100;++n)assert(build(b,c.w,c.h,p,2097152));const double currentMs=(now()-at)/100;
  if(!first)std::printf(",");first=false;std::printf("{\"name\":\"%s\",\"bin\":%u,\"old_references\":%llu,\"references\":%llu,\"old_pixel_visits\":%llu,\"pixel_visits\":%llu,\"before_csr_ms\":%.6f,\"current_csr_ms\":%.6f}",c.name,p.cost.size,static_cast<unsigned long long>(old.references),static_cast<unsigned long long>(p.cost.references),static_cast<unsigned long long>(old.pixelVisits),static_cast<unsigned long long>(p.cost.pixelVisits),oldMs,currentMs);
 }
 std::printf("],\"final_checks\":%llu,\"scope\":\"Actual adaptive CSR construction and exhaustive painter-order pixel lookup; CPU timings, not GPU execution\"}\n",static_cast<unsigned long long>(checks));
}
