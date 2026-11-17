#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <type_traits>
#include <cstddef>

// Diagnostic-only, one game-thread writer. The hot path performs no allocations,
// locks, file I/O, printf, sorting or extra game-memory reads. Quantile sorting
// is exclusively a Host-side operation on a copied, sealed publication.
namespace criticalpath {
inline constexpr uint32_t Version=1, IntervalMs=1000, Samples=128;
enum Stage : unsigned {
 MessageHook, CursorWork, TacticalDispatch, WorldCapture, WorldIndex,
 SceneScan, NativePip, HardwareHit, HardwareAudit, StageCount
};
inline constexpr const char* Names[StageCount]={
 "message-hook","cursor-work","tactical-dispatch","world-capture",
 "world-index","scene-scan","native-pip","hardware-hit","hardware-audit"
};
// Sampled calls, not all invocations. The reported percentile always describes
// measured calls, and the stride is printed so nobody interprets it as an
// exact full-population latency percentile.
inline constexpr uint32_t Stride[StageCount]={16,16,8,4,8,8,1,64,1};
struct Row {
 uint32_t sampled=0,filled=0,next=0,maxUs=0;
 uint64_t sumUs=0;
 uint32_t samples[Samples]{};
 void add(uint32_t us){
  ++sampled;sumUs+=us;maxUs=std::max(maxUs,us);
  samples[next]=us;next=(next+1)%Samples;
  if(filled<Samples)++filled;
 }
};
struct Snapshot {
 uint32_t version=Version,serial=0,match=0,tick=0,intervalMs=0,pad=0;
 Row rows[StageCount]{};
};
static_assert(std::is_standard_layout<Snapshot>::value&&std::is_trivially_copyable<Snapshot>::value,"critical path IPC payload must be a POD");
static_assert(offsetof(Row,sumUs)==16&&offsetof(Row,samples)==24&&sizeof(Row)==536&&offsetof(Snapshot,rows)==24&&sizeof(Snapshot)==4848,"Critical timing layout differs between x86 and x64");
class Recorder {
 Row rows_[StageCount]{};
 uint32_t since_=0,match_=0,serial_=0;
public:
 void reset(uint32_t now,uint32_t match){
  for(auto&row:rows_)row={};since_=now;match_=match;
 }
 void add(unsigned stage,uint32_t us){
  if(stage<StageCount)rows_[stage].add(us);
 }
 bool publish(uint32_t now,uint32_t match,Snapshot&out){
  if(!since_||match!=match_){reset(now,match);return false;}
  const uint32_t elapsed=now-since_;if(elapsed<IntervalMs)return false;
  out.version=Version;out.serial=++serial_;if(!out.serial)out.serial=++serial_;
  out.match=match;out.tick=now;out.intervalMs=elapsed;
  for(unsigned i=0;i<StageCount;++i)out.rows[i]=rows_[i];
  reset(now,match);return true;
 }
};
struct Statistics {
 uint32_t sampled=0,stored=0,p95Us=0,p99Us=0,maxUs=0;
 uint64_t sumUs=0;
 double meanUs=0;
};
inline Statistics summarize(const Row&row){
 Statistics s{};s.sampled=row.sampled;s.stored=std::min(row.filled,Samples);
 s.sumUs=row.sumUs;s.maxUs=row.maxUs;
 s.meanUs=row.sampled?double(row.sumUs)/row.sampled:0;
 if(!s.stored)return s;
 std::array<uint32_t,Samples> values{};
 for(uint32_t i=0;i<s.stored;++i)values[i]=row.samples[i];
 std::sort(values.begin(),values.begin()+s.stored);
 // Nearest-rank empirical quantiles: 1-based ceil(p*n), clamped to [1,n].
 s.p95Us=values[(95u*s.stored+99u)/100u-1];
 s.p99Us=values[(99u*s.stored+99u)/100u-1];
 return s;
}
}
