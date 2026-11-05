#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <new>
#include <stdexcept>
#include <vector>

namespace external { namespace phasecsr {
// Optional host-side acceleration. Shader programs, dispatch domains, phase
// flags and original CSR remain unchanged. All offsets below are uint words.
constexpr size_t MaximumPhases=65;
constexpr uint64_t MaximumExtraWords=8ull*1024*1024/sizeof(uint32_t);
struct Phase {uint32_t first=0,last=0;bool serial=false;};
struct Binding {uint32_t binsOffset=0,refsOffset=0;};
struct Slice {bool original=true;std::vector<uint32_t>counts,starts,refs;};
struct Plan {
 bool selected=false;const char*reason="original CSR";std::vector<Slice>slices;
 uint64_t extraWords=0,sourceVisits=0,oldParallelRefs=0,newParallelRefs=0,crossRunBins=0;
};
#ifdef RA2_PHASE_CSR_TEST_ACCESS
struct TestState {bool disabled=false;uint64_t builds=0,selectedBuilds=0,selectedAppends=0,appendedWords=0;};
inline TestState&testState(){static thread_local TestState state;return state;}
inline void testDisable(bool disabled){testState()={};testState().disabled=disabled;}
#endif
inline Plan reject(const char*reason) noexcept {Plan p;p.reason=reason;return p;}

// Match the existing split exactly, including its empty initial/final parallel
// dispatches. Allocation failure only disables this optional optimization.
template<class Displaced>std::vector<Phase> partition(uint32_t draws,Displaced displaced) noexcept {
 try {
  std::vector<Phase>out;uint32_t first=0;
  for(uint32_t n=0;n<draws;++n)if(displaced(n)){
   if(n>first||out.empty())out.push_back({first,n,false});
   out.push_back({n,n+1,true});first=n+1;
   if(out.size()>=MaximumPhases)return {};
  }
  out.push_back({first,draws,false});return out;
 }catch(const std::bad_alloc&){return {};}catch(const std::length_error&){return {};}
}
namespace detail {
using Mask=std::array<uint64_t,2>;
inline bool marked(const Mask&m,size_t p){return (m[p/64]>>(p%64))&1u;}
inline void mark(Mask&m,size_t p){m[p/64]|=uint64_t(1)<<(p%64);}
inline void unmark(Mask&m,size_t p){m[p/64]&=~(uint64_t(1)<<(p%64));}
inline Plan build(const std::vector<uint32_t>&refs,const std::vector<uint32_t>&starts,uint32_t draws,const std::vector<Phase>&phases,uint64_t maximumWords){
 if(starts.empty()||starts.front()!=0||starts.back()!=refs.size()||draws>65536||phases.size()<2||phases.size()>MaximumPhases)return reject("shape/phase limit");
 const size_t bins=starts.size()-1;
 for(size_t b=0;b<bins;++b)if(starts[b]>starts[b+1])return reject("bad CSR starts");
 uint32_t previous=0,parallel=0;
 for(const auto&p:phases){
  if(p.first!=previous||p.first>p.last||p.last>draws||(p.serial&&p.last-p.first!=1))return reject("invalid phase partition");
  previous=p.last;parallel+=!p.serial;
 }
 if(previous!=draws||!parallel||refs.empty())return reject("empty phase/input");
 maximumWords=std::min(maximumWords,MaximumExtraWords);
 Plan out;out.extraWords=uint64_t(parallel)*uint64_t(bins)*2;out.oldParallelRefs=uint64_t(parallel)*refs.size();
 if(out.extraWords>maximumWords)return reject("metadata table limit");
 std::vector<uint8_t>owner(draws);
 for(size_t p=0;p<phases.size();++p)for(uint32_t n=phases[p].first;n<phases[p].last;++n)owner[n]=uint8_t(p);
 out.slices.resize(phases.size());
 for(size_t p=0;p<phases.size();++p)if(!phases[p].serial){out.slices[p].original=false;out.slices[p].counts.assign(bins,0);}
 std::vector<Mask>fallback(bins);
 // Scan the existing ordered references, rather than all draws per phase.
 for(size_t b=0;b<bins;++b){const size_t first=starts[b],end=starts[b+1];uint32_t old=0;bool seen=false;
  for(size_t n=first;n<end;++n){
   ++out.sourceVisits;const uint32_t draw=refs[n]&65535u;
   if(draw>=draws||(seen&&draw<=old))return reject("unordered/invalid draw reference");
   old=draw;seen=true;const size_t p=owner[draw];if(!out.slices[p].original)++out.slices[p].counts[b];
   if(refs[n]>>16){
    const size_t tail=std::min(end,n+size_t(1)+(refs[n]>>16));const uint32_t tailDraw=refs[tail-1]&65535u;
    if(tailDraw>=draws)return reject("invalid encoded run");
    const size_t a=owner[draw],z=owner[tailDraw];if(z<a)return reject("unordered encoded run");
    // The legacy shader seeks an opaque pixel BEFORE checking first/last.
    // Preserve the entire original bin whenever a run crosses this split.
    if(a!=z)for(size_t phase=a;phase<=z;++phase)mark(fallback[b],phase);
   }
  }
  for(size_t p=0;p<phases.size();++p)if(!out.slices[p].original&&marked(fallback[b],p)){
   if(out.slices[p].counts[b]){out.slices[p].counts[b]=uint32_t(end-first);++out.crossRunBins;}
   else unmark(fallback[b],p);
  }
 }
 for(auto&s:out.slices)if(!s.original){
  s.starts.resize(bins+1);uint64_t n=0;
  for(size_t b=0;b<bins;++b){s.starts[b]=uint32_t(n);n+=s.counts[b];if(n>std::numeric_limits<uint32_t>::max())return reject("reference overflow");}
  s.starts[bins]=uint32_t(n);out.newParallelRefs+=n;out.extraWords+=n;
  if(out.extraWords>maximumWords)return reject("metadata/reference limit");
 }
 if(out.newParallelRefs*4>=out.oldParallelRefs*3)return reject("insufficient reference reduction");
 std::vector<std::vector<uint32_t>>positions(phases.size());
 for(size_t p=0;p<phases.size();++p)if(!out.slices[p].original){auto&s=out.slices[p];s.refs.resize(s.starts.back());positions[p]=s.starts;}
 for(size_t b=0;b<bins;++b){const auto first=starts[b],end=starts[b+1];
  for(size_t p=0;p<phases.size();++p)if(!out.slices[p].original&&marked(fallback[b],p))
   std::copy(refs.begin()+first,refs.begin()+end,out.slices[p].refs.begin()+out.slices[p].starts[b]);
  for(uint32_t n=first;n<end;++n){++out.sourceVisits;const size_t p=owner[refs[n]&65535u];
   if(!out.slices[p].original&&!marked(fallback[b],p))out.slices[p].refs[positions[p][b]++]=refs[n];
  }
 }
 out.selected=true;out.reason="parallel phase CSR selected";return out;
}
} // namespace detail

inline Plan build(const std::vector<uint32_t>&refs,const std::vector<uint32_t>&starts,uint32_t draws,const std::vector<Phase>&phases,uint64_t maximumWords=MaximumExtraWords) noexcept {
#ifdef RA2_PHASE_CSR_TEST_ACCESS
 ++testState().builds;if(testState().disabled)return reject("test forced original CSR");
#endif
 try {
  auto out=detail::build(refs,starts,draws,phases,maximumWords);
#ifdef RA2_PHASE_CSR_TEST_ACCESS
  testState().selectedBuilds+=out.selected;
#endif
  return out;
 }catch(const std::bad_alloc&){return reject("optional CSR allocation failed");}
 catch(const std::length_error&){return reject("optional CSR allocation limit");}
}

// Append only after all original commands have been assembled and before GPU
// allocation/flush. Reserve both outputs first: failure leaves metadata intact.
inline bool append(const Plan&plan,std::vector<uint32_t>&metadata,Binding original,std::vector<Binding>&bindings,uint64_t maximumTotalWords=std::numeric_limits<uint32_t>::max()) noexcept {
 bindings.clear();if(!plan.selected||plan.slices.size()>MaximumPhases)return false;
 try {
  uint64_t extra=0;
  for(const auto&s:plan.slices)if(!s.original){
   if(s.starts.size()!=s.counts.size()+1||s.starts.empty()||s.starts.front()!=0||s.starts.back()!=s.refs.size())return false;
   for(size_t b=0;b<s.counts.size();++b)if(s.starts[b+1]<s.starts[b]||s.starts[b+1]-s.starts[b]!=s.counts[b])return false;
   extra+=uint64_t(s.counts.size())*2+s.refs.size();if(extra>MaximumExtraWords)return false;
  }
  const uint64_t total=uint64_t(metadata.size())+extra;
  if(total>maximumTotalWords||total>std::numeric_limits<uint32_t>::max()||total>metadata.max_size())return false;
  std::vector<Binding>next(plan.slices.size(),original);metadata.reserve(size_t(total));
  for(size_t p=0;p<plan.slices.size();++p){const auto&s=plan.slices[p];if(s.original)continue;
   next[p].binsOffset=uint32_t(metadata.size());next[p].refsOffset=uint32_t(metadata.size()+s.counts.size()*2);
   for(size_t b=0;b<s.counts.size();++b){metadata.push_back(s.starts[b]);metadata.push_back(s.counts[b]);}
   metadata.insert(metadata.end(),s.refs.begin(),s.refs.end());
  }
  bindings.swap(next);
#ifdef RA2_PHASE_CSR_TEST_ACCESS
  ++testState().selectedAppends;testState().appendedWords+=extra;
#endif
  return true;
 }catch(const std::bad_alloc&){return false;}catch(const std::length_error&){return false;}
}
}}
