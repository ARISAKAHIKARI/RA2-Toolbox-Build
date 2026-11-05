#ifdef NDEBUG
#undef NDEBUG
#endif
#define RA2_PHASE_CSR_TEST_ACCESS
#include "../../external_reader/source/gpu_phase_csr.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <random>

// Deterministic allocation-failure injection applies only to this tiny test.
static int allocationFailure=-1;
void*operator new(std::size_t n){if(allocationFailure==0)throw std::bad_alloc();if(allocationFailure>0)--allocationFailure;if(void*p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void*operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void*p)noexcept{std::free(p);}
void operator delete[](void*p)noexcept{std::free(p);}
void operator delete(void*p,std::size_t)noexcept{std::free(p);}
void operator delete[](void*p,std::size_t)noexcept{std::free(p);}
namespace csr=external::phasecsr;

// The production shader's encoded seek occurs before its phase range check.
static uint32_t execute(const std::vector<uint32_t>&refs,size_t first,size_t end,const csr::Phase&p,uint32_t color,const std::vector<uint8_t>&alpha,const std::vector<uint8_t>&visible){
 for(size_t item=first;item<end;++item){
  const uint32_t encoded=refs[item];
  if(encoded>>16){size_t last=std::min(end,item+1+size_t(encoded>>16));while(last>item){--last;uint32_t d=refs[last]&65535u;if(visible[d]&&alpha[d]==255){item=last;break;}}}
  const uint32_t d=refs[item]&65535u;if(d<p.first||d>=p.last||!visible[d])continue;
  color=(color*(255-alpha[d])+(d*37%256)*alpha[d])/255;
 }
 return color;
}
static void legacySeekAcrossPhaseBoundary(){
 const std::vector<csr::Phase>phases{{0,30,false},{30,31,true},{31,80,false}};
 std::vector<uint32_t>refs{10u|(2u<<16),20u,40u},starts{0,3};
 for(unsigned b=0;b<20;++b){for(uint32_t n=0;n<80;++n)refs.push_back(n);starts.push_back(uint32_t(refs.size()));}
 const auto plan=csr::build(refs,starts,80,phases);assert(plan.selected&&plan.crossRunBins==2);
 std::vector<uint8_t>alpha(80,127),visible(80,1);alpha[40]=255;
 const uint32_t legacy=execute(refs,0,3,phases[0],89,alpha,visible);assert(legacy==89);
 for(size_t p:{size_t(0),size_t(2)}){
  const auto&s=plan.slices[p];assert(s.counts[0]==3);
  assert(std::equal(refs.begin(),refs.begin()+3,s.refs.begin()+s.starts[0]));
  assert(execute(s.refs,s.starts[0],s.starts[1],phases[p],89,alpha,visible)==execute(refs,0,3,phases[p],89,alpha,visible));
 }
 std::puts("LEGACY_SEEK cross-phase bin preserved; out-of-phase opaque seek remains unchanged");
}
static void properties(){
 std::mt19937 rng(20261105);uint64_t checks=0,selected=0,encodedCases=0;
 for(unsigned test=0;test<3000;++test){
  const uint32_t draws=40+rng()%216,bins=1+rng()%12,a=draws/3,b=draws*2/3;
  const std::vector<csr::Phase>phases{{0,a,false},{a,a+1,true},{a+1,b,false},{b,b+1,true},{b+1,draws,false}};
  std::vector<uint32_t>refs,starts(1);
  for(uint32_t bin=0;bin<bins;++bin){
   const size_t first=refs.size();for(uint32_t n=0;n<draws;++n)if(rng()%3)refs.push_back(n);
   if(test%2)for(size_t n=first;n<refs.size();++n)if(rng()%5==0){const auto tail=std::min(refs.size()-n-1,size_t(1+rng()%9));refs[n]|=uint32_t(tail)<<16;n+=tail;}
   starts.push_back(uint32_t(refs.size()));
  }
  const auto plan=csr::build(refs,starts,draws,phases);if(!plan.selected)continue;++selected;encodedCases+=test%2;
  std::vector<uint8_t>alpha(draws),visible(draws);
  for(uint32_t n=0;n<draws;++n){alpha[n]=std::array<uint8_t,3>{{0,127,255}}[rng()%3];visible[n]=rng()%4!=0;}
  for(size_t p=0;p<phases.size();++p)for(uint32_t bin=0;bin<bins;++bin)for(uint32_t color:{0u,89u,255u}){
   const auto expected=execute(refs,starts[bin],starts[bin+1],phases[p],color,alpha,visible);const auto&s=plan.slices[p];
   const auto actual=s.original?expected:execute(s.refs,s.starts[bin],s.starts[bin+1],phases[p],color,alpha,visible);assert(actual==expected);++checks;
  }
  assert(!csr::build(refs,starts,draws,phases,1).selected);
 }
 assert(selected>1500&&encodedCases>300);
 std::printf("PROPERTY selected=%llu encoded=%llu exact_phase_color_checks=%llu\n",(unsigned long long)selected,(unsigned long long)encodedCases,(unsigned long long)checks);
}
static void boundaries(){
 auto emptyEnds=csr::partition(2,[](uint32_t){return true;});assert(emptyEnds.size()==4&&emptyEnds.front().first==emptyEnds.front().last&&emptyEnds.back().first==emptyEnds.back().last);
 auto emptyPlan=csr::build({0,1},{0,2},2,emptyEnds);assert(emptyPlan.selected&&emptyPlan.slices.front().refs.empty()&&emptyPlan.slices.back().refs.empty());
 const std::vector<csr::Phase>wide{{0,32768,false},{32768,32769,true},{32769,65536,false}};
 auto top=csr::build({1,32768,65535},{0,3},65536,wide);assert(top.selected&&top.slices.back().refs.back()==65535);
 auto sixtyFive=csr::partition(1024,[](uint32_t n){return n%32==31;});assert(sixtyFive.size()==65);
 std::vector<uint32_t>refs;for(uint32_t n=0;n<1024;++n)refs.push_back(n);
 auto cap=csr::build(refs,{0,1024},1024,sixtyFive);assert(cap.selected&&cap.slices.size()==65);
 assert(csr::partition(1056,[](uint32_t n){return n%32==31;}).empty());
 auto invalid=sixtyFive;invalid.push_back({1024,1024,false});assert(!csr::build(refs,{0,1024},1024,invalid).selected);
 assert(!csr::build({1,0},{0,2},65536,wide).selected);
 assert(!csr::build({0,1},{0,3},2,emptyEnds).selected);
 auto badSerial=emptyEnds;badSerial[1].last=0;assert(!csr::build({0,1},{0,2},2,badSerial).selected);
 auto badRange=wide;badRange[1].first=32767;assert(!csr::build({1,32768,65535},{0,3},65536,badRange).selected);
 // A packed run reaching phase 64 must not shift a 32/64-bit mask by its width.
 refs[0]|=1023u<<16;auto cross=csr::build(refs,{0,1024},1024,sixtyFive);assert(!cross.selected);
 std::puts("BOUNDARY empty dispatches, singleton serials, draw 65535, 65 phases and phase-64 run handled");
}
static void packingAndAllocation(){
 std::vector<uint32_t>refs;for(uint32_t n=0;n<100;++n)refs.push_back(n);
 const std::vector<uint32_t>starts{0,100};const auto phases=csr::partition(100,[](uint32_t n){return n==25||n==75;});
 const auto plan=csr::build(refs,starts,100,phases);assert(plan.selected);
 std::vector<uint32_t>metadata(7,0xa5a5a5a5);std::vector<csr::Binding>bindings;const csr::Binding original{19,23};
 assert(csr::append(plan,metadata,original,bindings));assert(bindings.size()==phases.size()&&metadata.size()==7+plan.extraWords);
 for(size_t p=0;p<phases.size();++p){
  const auto&s=plan.slices[p];if(s.original){assert(bindings[p].binsOffset==19&&bindings[p].refsOffset==23);continue;}
  const auto at=bindings[p];assert(metadata[at.binsOffset]==0&&metadata[at.binsOffset+1]==s.counts[0]);
  assert(at.refsOffset==at.binsOffset+2&&std::equal(s.refs.begin(),s.refs.end(),metadata.begin()+at.refsOffset));
 }
 for(size_t n=0;n<7;++n)assert(metadata[n]==0xa5a5a5a5);
 auto intact=std::vector<uint32_t>(7,42);const auto before=intact;
 assert(!csr::append(plan,intact,original,bindings,7+plan.extraWords-1));assert(intact==before&&bindings.empty());
 assert(!csr::append(plan,intact,original,bindings,0));assert(intact==before);
 unsigned failures=0,successes=0;
 for(int n=0;n<70;++n){allocationFailure=n;const auto attempt=csr::build(refs,starts,100,phases);allocationFailure=-1;if(attempt.selected)++successes;else {++failures;assert(attempt.slices.empty()&&attempt.extraWords==0);}}
 assert(failures>5&&successes>0);
 for(int n=0;n<2;++n){auto destination=std::vector<uint32_t>(7,42);allocationFailure=n;bool appended=csr::append(plan,destination,original,bindings);allocationFailure=-1;assert(!appended&&destination==before&&bindings.empty());}
 allocationFailure=0;auto missingPhases=csr::partition(100,[](uint32_t n){return n==50;});allocationFailure=-1;assert(missingPhases.empty());
 csr::testDisable(true);assert(!csr::build(refs,starts,100,phases).selected);csr::testDisable(false);assert(csr::build(refs,starts,100,phases).selected);
 std::printf("PACKING uint-word offsets exact, strong metadata fallback; allocation failures=%u\n",failures);
}
int main(){legacySeekAcrossPhaseBoundary();properties();boundaries();packingAndAllocation();std::puts("PASS phase CSR host regression");}
