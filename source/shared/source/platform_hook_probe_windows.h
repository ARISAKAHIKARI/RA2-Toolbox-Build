#pragma once
// Read-only collection. The production algorithm is independent of Win32 so its
// cancellation, incomplete-coverage and identity behavior can be tested directly.
#include "platform_hook_probe_core.h"
#include "platform_hook_probe_report.h"
#include "debug_entry_catalog.h"
#include "patch_context.h"
#include "../../assist/source/signatures.h"
#include <array>
#include <cstring>
#include <limits>
#include <set>
#include <utility>

namespace hookprobe {
inline constexpr uint64_t ProbeDeadlineMs=45000,ProbeTraceReserveMs=8000,ProbeMaxScanBytes=256ull*1024*1024,ProbeMaxRemoteReadBytes=768ull*1024*1024,ProbeMaxDiskReadBytes=512ull*1024*1024;
inline constexpr size_t ProbeMaxFileBytes=64u*1024u*1024u,ProbeMaxModules=512,ProbeMaxDifferences=4096,ProbeMaxChains=256;
inline constexpr size_t ProbePageBytes=4096,ProbeMaxDeclarations=8192,ProbeMaxTotalDeclarations=16384,ProbeMaxTotalExports=32768,ProbeMaxReportBytes=32u*1024u*1024u;
struct ModuleSnapshot {
 std::wstring path;std::string name;uint64_t base=0;uint32_t size=0;bool main=false,system=false;
};
struct MemoryRegion {
 uint64_t base=0,size=0,allocationBase=0;bool readable=false,executable=false,image=false;
};
// The interface deliberately has no write, suspension, debug or execution API.
struct ProbeIo {
 virtual ~ProbeIo()=default;
 virtual uint64_t nowMs()=0;
 virtual std::string utc()=0;
 virtual std::string creationTime()=0;
 virtual bool alive()=0;
 virtual bool snapshot(std::vector<ModuleSnapshot>&,std::string&)=0;
 virtual bool query(uint64_t,MemoryRegion&)=0;
 virtual size_t read(uint64_t,uint8_t*,size_t)=0;
 virtual bool readFile(const std::wstring&,size_t,std::vector<uint8_t>&,FileIdentity&,std::string&)=0;
 virtual bool fileUnchanged(const FileIdentity&)=0;
};
class ProbeSha256 {
 uint32_t h_[8]={0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
 uint8_t block_[64]{};size_t used_=0;uint64_t bytes_=0;
 static uint32_t rr(uint32_t n,unsigned b){return (n>>b)|(n<<(32-b));}
 void compress(const uint8_t*p){
  static constexpr uint32_t k[64]={0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
  uint32_t w[64];for(size_t i=0;i<16;++i)w[i]=(uint32_t(p[i*4])<<24)|(uint32_t(p[i*4+1])<<16)|(uint32_t(p[i*4+2])<<8)|p[i*4+3];
  for(size_t i=16;i<64;++i){uint32_t a=w[i-15],b=w[i-2];w[i]=w[i-16]+(rr(a,7)^rr(a,18)^(a>>3))+w[i-7]+(rr(b,17)^rr(b,19)^(b>>10));}
  uint32_t a=h_[0],b=h_[1],c=h_[2],d=h_[3],e=h_[4],f=h_[5],g=h_[6],h=h_[7];
  for(size_t i=0;i<64;++i){uint32_t t1=h+(rr(e,6)^rr(e,11)^rr(e,25))+((e&f)^(~e&g))+k[i]+w[i],t2=(rr(a,2)^rr(a,13)^rr(a,22))+((a&b)^(a&c)^(b&c));h=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2;}
  h_[0]+=a;h_[1]+=b;h_[2]+=c;h_[3]+=d;h_[4]+=e;h_[5]+=f;h_[6]+=g;h_[7]+=h;
 }
public:
 void update(const uint8_t*p,size_t n){bytes_+=n;while(n){size_t k=(std::min)(n,64-used_);std::memcpy(block_+used_,p,k);used_+=k;p+=k;n-=k;if(used_==64){compress(block_);used_=0;}}}
 std::string finish(){uint64_t bits=bytes_*8;block_[used_++]=0x80;if(used_>56){std::memset(block_+used_,0,64-used_);compress(block_);used_=0;}std::memset(block_+used_,0,56-used_);for(size_t i=0;i<8;++i)block_[63-i]=uint8_t(bits>>(8*i));compress(block_);std::ostringstream s;s<<std::hex<<std::setfill('0');for(uint32_t n:h_)s<<std::setw(8)<<n;return s.str();}
};
inline std::string sha256(const std::vector<uint8_t>&v){ProbeSha256 h;if(!v.empty())h.update(v.data(),v.size());return h.finish();}
struct ProbeBudget {
 ProbeIo&io;Progress&progress;uint64_t start=0,bytes=0;std::string reason,scanReason;uint64_t remoteReadBytes=0,diskReadBytes=0;
 bool stop(){if(!reason.empty())return true;if(progress.cancel.load(std::memory_order_relaxed))reason="cancelled";else if(!io.alive())reason="process-exited";else if(io.nowMs()-start>=ProbeDeadlineMs)reason="deadline-exceeded";return !reason.empty();}
 bool reserveRead(size_t n){if(stop())return false;if(remoteReadBytes+n>ProbeMaxRemoteReadBytes){reason="remote-read-byte-budget-exceeded";return false;}remoteReadBytes+=n;return true;}
 bool scan(size_t n){if(stop()||!scanReason.empty())return false;if(io.nowMs()-start>=ProbeDeadlineMs-ProbeTraceReserveMs){scanReason="scan-phase-time-limit-reserved-for-chain-and-final-check";return false;}if(bytes+n>ProbeMaxScanBytes){scanReason="scan-byte-budget-exceeded";return false;}bytes+=n;return true;}
};
inline uint16_t probeU16(const uint8_t*p){return uint16_t(p[0])|(uint16_t(p[1])<<8);}
inline uint32_t probeU32(const uint8_t*p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
inline uint64_t probeU64(const uint8_t*p){return uint64_t(probeU32(p))|(uint64_t(probeU32(p+4))<<32);}
inline bool probeRange(uint64_t a,uint64_t n,uint64_t base,uint64_t size){return a>=base&&a-base<=size&&n<=size-(a-base);}
inline size_t readObserved(ProbeIo&io,ProbeBudget&b,uint64_t address,uint8_t*out,size_t n,uint8_t*valid,uint64_t imageBase=0){
 std::fill(valid,valid+n,uint8_t(0));size_t readCount=0;
 for(size_t at=0;at<n;){if(b.stop())break;MemoryRegion m{};const uint64_t pos=address+at;
  if(pos<address||!io.query(pos,m)||!m.size||!probeRange(pos,1,m.base,m.size)){at+=(std::min)(n-at,ProbePageBytes-size_t(pos%ProbePageBytes));continue;}
  const size_t k=(std::min)({n-at,size_t((std::min)(uint64_t(n-at),m.size-(pos-m.base))),ProbePageBytes-size_t(pos%ProbePageBytes)});
  if(m.readable&&(!imageBase||(m.image&&m.allocationBase==imageBase))){if(!b.reserveRead(k))break;size_t got=(std::min)(io.read(pos,out+at,k),k);std::fill(valid+at,valid+at+got,uint8_t(1));readCount+=got;}at+=k;
 }return readCount;
}
inline bool readExact(ProbeIo&io,ProbeBudget&b,uint64_t at,uint8_t*out,size_t n,uint64_t owner=0){std::vector<uint8_t>v(n);return readObserved(io,b,at,out,n,v.data(),owner)==n;}
inline bool readHeader(ProbeIo&io,ProbeBudget&b,const ModuleSnapshot&m,std::vector<uint8_t>&bytes,PeImage&pe,std::string&error){
 std::array<uint8_t,64>dos{};if(!readExact(io,b,m.base,dos.data(),dos.size(),m.base)){error="memory DOS header unreadable";return false;}
 if(dos[0]!='M'||dos[1]!='Z'){error="memory DOS header invalid";return false;}uint32_t nt=probeU32(dos.data()+0x3c);
 if(nt>65536-24||uint64_t(nt)+24>m.size){error="memory NT header exceeds bounded header range";return false;}
 std::array<uint8_t,24>h{};if(!readExact(io,b,m.base+nt,h.data(),h.size(),m.base)){error="memory NT header unreadable";return false;}
 const uint32_t total=nt+24+probeU16(h.data()+20)+uint32_t(probeU16(h.data()+6))*40;
 if(total>65536||total>m.size||total<nt+24){error="memory section table exceeds bounded header range";return false;}
 bytes.assign(total,0);if(!readExact(io,b,m.base,bytes.data(),bytes.size(),m.base)){error="memory section table unreadable";return false;}
 return parsePe(bytes.data(),bytes.size(),pe,error,false);
}
struct ReferenceWindow {uint32_t canonical=0,size=0;size_t reference=0;std::string source;};
inline std::vector<ReferenceWindow> referenceWindows(ProbeReport&r){
 std::vector<ReferenceWindow>w;r.references.clear();
 for(const auto&e:debugentries::Entries){ReferenceRecord z;z.id=e.id;z.canonicalAddress=e.address;z.name=e.name;z.purpose=e.purpose;z.features=e.features;
  z.kind=e.kind==debugentries::Kind::Call?"Call":e.kind==debugentries::Kind::Patch?"Patch":e.kind==debugentries::Kind::Breakpoint?"Breakpoint":"System";
  if(e.kind==debugentries::Kind::System){z.status="system-setting-no-code-address";z.rangeSource="none";}
  else{z.size=1;z.rangeSource="entry-address-only";if(e.kind==debugentries::Kind::Patch){if(unsigned n=experimentalpatch::size(e.address)){z.size=n;z.rangeSource="modeled-patch-span";}}if(e.kind==debugentries::Kind::Call)for(const auto&s : ::Signatures)if(s.address==e.address){z.size=s.length;z.rangeSource="call-signature";break;}w.push_back({e.address,z.size,r.references.size(),z.rangeSource});}
  r.references.push_back(z);
 }
 for(const auto&p:patchcontext::Probes)for(size_t i=0;i<r.references.size();++i)if(r.references[i].canonicalAddress==p.site&&r.references[i].kind=="Patch")w.push_back({p.address,p.size,i,"dependency-probe"});
 r.referenceRangeCount=uint32_t(w.size());return w;
}
inline void annotateReferences(ProbeReport&r,const std::vector<ReferenceWindow>&w){
 const ModuleRecord*main=nullptr;size_t mainIndex=0;for(size_t i=0;i<r.modules.size();++i)if(r.modules[i].main){main=&r.modules[i];mainIndex=i;break;}
 if(!main||main->pe64||main->preferredBase!=0x400000||!main->structuralMatch)return;
 for(auto&e:r.references){if(e.kind=="System")continue;e.status="reference-outside-executable-coverage";
  if(e.canonicalAddress<0x400000)continue;
  uint32_t rva=e.canonicalAddress-0x400000;for(const auto&s:main->sections)if(probeRange(rva,e.size,s.rva,s.size)){e.status="reference-in-"+s.status+"-section;layout-not-certified";break;}}
 for(auto&d:r.differences)if(d.module==mainIndex)for(const auto&x:w){if(x.canonical<0x400000)continue;uint64_t rva=x.canonical-0x400000;if(uint64_t(d.rva)<rva+x.size&&rva<uint64_t(d.rva)+d.length){auto&e=r.references[x.reference];++e.intersections;if(d.referenceIntersections.size()>=8)continue;d.referenceIntersections.push_back(e.kind+" "+e.name+" / "+x.source+" "+hexAddress(x.canonical)+"+"+std::to_string(x.size)+" / "+e.features+" (reference mapping only)");}}
}
inline void markSectionTotals(ModuleRecord&m){
 m.comparedBytes=m.ignoredBytes=m.unreadableBytes=m.unscannedBytes=m.changedBytes=m.dynamicBytes=0;
 for(const auto&s:m.sections){m.comparedBytes+=s.comparedBytes;m.ignoredBytes+=s.ignoredBytes;m.unreadableBytes+=s.unreadableBytes;m.unscannedBytes+=s.unscannedBytes;m.changedBytes+=s.changedBytes;m.dynamicBytes+=s.dynamicBytes;}
}
inline void appendDiff(ProbeReport&r,Progress&p,Difference d){if(r.differences.size()<ProbeMaxDifferences){r.differences.push_back(std::move(d));p.findings.store(uint32_t(r.differences.size()),std::memory_order_relaxed);}else r.partial=true;}
inline void detectCandidate(Difference&d,const uint8_t*live,const uint8_t*valid,size_t n,uint32_t chunkRva,uint64_t base,bool pe64){
 if(d.rva<chunkRva||uint64_t(d.rva)-chunkRva>=n)return;
 size_t center=d.rva-chunkRva;
 // No disassembler is used: a backwards window is explicitly a byte-pattern candidate.
 for(size_t back=0;back<=15&&back<=center;++back){size_t at=center-back;Transfer t{};size_t have=0;while(at+have<n&&have<16&&valid[at+have]==1)++have;
  if(decodeTransfer(live+at,have,base+chunkRva+at,pe64,t)&&at+t.length>center){d.candidateAddress=base+chunkRva+at;d.candidateBytes.assign(live+at,live+at+t.length);d.type="transfer-candidate";d.chainStatus="candidate-not-yet-traced";return;}}
}
inline void inspectDeclarations(ProbeIo&io,ProbeBudget&b,const ModuleSnapshot&snap,const NormalizedImage&image,ModuleRecord&m,size_t remaining){
 const PeSection*section=nullptr;for(const auto&s:image.pe.sections)if(s.name==".syhks00"){section=&s;break;}if(!section)return;
 m.declarationBytes=section->mappedSize();std::vector<HookDeclaration>decls;std::string error;
 if(readHookDeclarations(image,decls,error,(std::min)(ProbeMaxDeclarations,remaining))){for(const auto&d:decls)m.declarations.push_back({d.address,d.bytes,validUtf8(d.name.size()>128?d.name.substr(0,128)+" [truncated]":d.name)});m.declarationStatus="baseline-declarations-parsed";}else{m.declarationStatus="baseline-declarations-invalid-or-limited";m.notes.push_back(error);}
 const size_t limit=(std::min)(size_t(section->mappedSize()),ProbeMaxDeclarations*16);std::array<uint8_t,ProbePageBytes>v{},valid{};
 for(size_t at=0;at<limit&&!b.stop();){size_t n=(std::min)(ProbePageBytes,limit-at);m.declarationReadableBytes+=readObserved(io,b,snap.base+section->rva+at,v.data(),n,valid.data(),snap.base);for(size_t j=0;j<n;++j)if(valid[j]&&v[j]!=image.bytes[section->rva+at+j])m.declarationMemoryDiffers=true;at+=n;}
 if(m.declarationReadableBytes<m.declarationBytes)m.declarationStatus+=";live-coverage-partial";else m.declarationStatus+=";live-readable";
 if(m.declarationMemoryDiffers)m.declarationStatus+=";live-differs";
}
struct ModuleSymbols {std::vector<ExportSymbol> exports;};
inline void collectModule(ProbeIo&io,ProbeBudget&budget,const ModuleSnapshot&snap,size_t index,const Options&o,ProbeReport&r,ModuleSymbols&symbols,size_t&exportBudget,size_t&declarationBudget){
 auto&m=r.modules[index];if(budget.stop()){m.status="unscanned-"+budget.reason;m.unscannedBytes=m.mappedSize;r.partial=true;return;}
 if(!budget.scan(0)){m.status="unscanned-"+(budget.reason.empty()?budget.scanReason:budget.reason);m.unscannedBytes=m.mappedSize;r.partial=true;return;}
 m.baselinePath=snap.main&&!o.baselineExe.empty()?o.baselineExe:snap.path;m.baselineSource=snap.main&&!o.baselineExe.empty()?"user-supplied-main-exe":"loaded-module-disk-path";
 std::vector<uint8_t>disk;std::string error;PeImage filePe{},livePe{};std::vector<uint8_t>firstHeader;
 const size_t fileBudget=size_t((std::min)(uint64_t(ProbeMaxFileBytes),ProbeMaxDiskReadBytes-budget.diskReadBytes));
 if(!fileBudget){m.status="baseline-total-byte-budget-exceeded";m.unscannedBytes=m.mappedSize;r.partial=true;return;}
 const bool fileRead=io.readFile(m.baselinePath,fileBudget,disk,m.file,error);budget.diskReadBytes+=(std::min)(uint64_t(fileBudget),m.file.readRequestedBytes);
 if(!fileRead){m.status="baseline-unavailable";m.error=error;m.unscannedBytes=m.mappedSize;r.partial=true;return;}
 if(disk.size()>fileBudget){m.status="baseline-file-limit";m.error="baseline exceeds per-file byte budget";m.unscannedBytes=m.mappedSize;r.partial=true;return;}
 m.file.sha256=sha256(disk);if(!m.file.unchangedAfterRead){m.status="baseline-changed-during-read";m.error="file identity, size or write time changed while reading";m.unscannedBytes=m.mappedSize;r.partial=true;return;}
 if(!parsePe(disk.data(),disk.size(),filePe,error,true)){m.status="baseline-invalid-pe";m.error=error;m.unscannedBytes=m.mappedSize;r.partial=true;return;}
 m.machine=filePe.machine;m.pe64=filePe.pe64;m.timestamp=filePe.timestamp;m.imageSize=filePe.imageSize;m.entryRva=filePe.entryRva;m.preferredBase=filePe.preferredBase;
 for(const auto&s:filePe.sections)if(s.executable()){SectionCoverage z;z.name=s.name;z.rva=s.rva;z.size=s.mappedSize();z.unscannedBytes=z.size;m.sections.push_back(z);}
 if(!readHeader(io,budget,snap,firstHeader,livePe,error)){m.status="memory-identity-unreadable";m.identityCheck=error;m.error=error;markSectionTotals(m);r.partial=true;return;}
 if(snap.size!=filePe.imageSize||!compatibleLayout(filePe,livePe,error)){m.status="baseline-memory-identity-mismatch";m.identityCheck=error.empty()?"module SizeOfImage differs":error;markSectionTotals(m);r.partial=true;return;}
 m.structuralMatch=true;m.identityCheck="PE structural identity matches (not proof of pristine file)";
 NormalizedImage expected{};if(!normalizeImage(disk,filePe,snap.base,expected,error)){m.status="normalization-unavailable";m.error=error;markSectionTotals(m);r.partial=true;return;}m.relocations=expected.relocations;
 disk.clear();disk.shrink_to_fit();if(!readExports(expected,symbols.exports,error,(std::min)(size_t(4096),exportBudget))){m.metadataPartial=true;r.partial=true;m.notes.push_back("metadata-partial: export labels unavailable or incomplete: "+error);}
 for(auto&e:symbols.exports)if(e.name.size()>128)e.name=validUtf8(e.name.substr(0,128))+" [truncated]";exportBudget-=symbols.exports.size();
 inspectDeclarations(io,budget,snap,expected,m,declarationBudget);declarationBudget-=m.declarations.size();
 if(m.declarationBytes&&(m.declarationReadableBytes<m.declarationBytes||m.declarationStatus.find("invalid-or-limited")!=std::string::npos)){m.metadataPartial=true;r.partial=true;m.notes.push_back("metadata-partial: declaration metadata coverage is partial");}
 std::array<uint8_t,ProbePageBytes>first{},second{},v1{},v2{},valid{};
 for(auto&s:m.sections){s.status="compared";
  for(uint32_t at=0;at<s.size;){const size_t n=(std::min)(size_t(s.size-at),ProbePageBytes-size_t((snap.base+s.rva+at)%ProbePageBytes));
   if(r.differences.size()>=ProbeMaxDifferences&&budget.scanReason.empty())budget.scanReason="difference-count-budget-exceeded";
   if(!budget.scan(n)){s.status="partial-"+(budget.reason.empty()?budget.scanReason:budget.reason);r.partial=true;break;}
   readObserved(io,budget,snap.base+s.rva+at,first.data(),n,v1.data(),snap.base);readObserved(io,budget,snap.base+s.rva+at,second.data(),n,v2.data(),snap.base);
   for(size_t j=0;j<n;++j)valid[j]=(!v1[j]||!v2[j])?0:first[j]==second[j]?1:2;
   // Loader-owned cells are excluded from both ordinary and dynamic findings.
   for(const auto&mask:expected.loaderWrites){uint64_t left=(std::max)(uint64_t(s.rva)+at,uint64_t(mask.rva)),right=(std::min)(uint64_t(s.rva)+at+n,uint64_t(mask.rva)+mask.size);for(uint64_t j=left;j<right;++j)valid[size_t(j-s.rva-at)]=0;}
   DiffResult diff{};DiffLimits limits{ProbeMaxDifferences-r.differences.size(),32};
   if(!diffExecutableChunk(expected,s.rva+at,second.data(),n,valid.data(),diff,error,limits)){s.status="diff-invalid";m.error=error;r.partial=true;break;}
   s.comparedBytes+=diff.comparedBytes;s.ignoredBytes+=diff.ignoredBytes;s.unreadableBytes+=diff.unreadableBytes;s.changedBytes+=diff.changedBytes;s.dynamicBytes+=diff.unstableBytes;s.unscannedBytes-=n;
   if(diff.truncated){r.partial=true;budget.scanReason="difference-count-budget-exceeded";}
   for(const auto&x:diff.ranges){Difference d;d.module=uint32_t(index);d.rva=x.rva;d.address=snap.base+x.rva;d.length=x.length;d.section=s.name;d.expected=hexBytes(x.expected.data(),x.expected.size());d.actual=hexBytes(x.actual.data(),x.actual.size());detectCandidate(d,second.data(),valid.data(),n,s.rva+at,snap.base,m.pe64);appendDiff(r,budget.progress,std::move(d));}
   for(size_t j=0;j<n;){if(valid[j]!=2){++j;continue;}size_t end=j+1;while(end<n&&valid[end]==2)++end;Difference d;d.module=uint32_t(index);d.rva=s.rva+at+uint32_t(j);d.address=snap.base+d.rva;d.length=uint32_t(end-j);d.section=s.name;d.type="dynamic-sample";d.stability="changed-between-two-reads";d.expected=hexBytes(expected.bytes.data()+d.rva,end-j);d.actual=hexBytes(second.data()+j,end-j);d.chainStatus="not-traced-unstable-bytes";appendDiff(r,budget.progress,std::move(d));j=end;}
   at+=uint32_t(n);
  }
  if(s.unreadableBytes||s.dynamicBytes||s.unscannedBytes){r.partial=true;if(s.status=="compared")s.status="partial-readability-or-dynamic";}
 }
 markSectionTotals(m);std::vector<uint8_t>lastHeader;PeImage lastPe{};error.clear();m.stableIdentity=readHeader(io,budget,snap,lastHeader,lastPe,error)&&lastHeader==firstHeader;
 m.file.unchangedAtFinish=io.fileUnchanged(m.file);
 if(!m.stableIdentity||!m.file.unchangedAtFinish){r.partial=true;m.notes.push_back("module or baseline identity could not be revalidated after scan");}
 m.status=(m.unreadableBytes||m.unscannedBytes||m.dynamicBytes||!m.stableIdentity||!m.file.unchangedAtFinish||m.metadataPartial)?"partial":m.changedBytes?"compared-with-differences":"no-differences-observed";
 if(m.sections.empty())m.status="no-executable-sections";
}
inline size_t ownerOf(const ProbeReport&r,uint64_t address){for(size_t i=0;i<r.modules.size();++i)if(probeRange(address,1,r.modules[i].base,r.modules[i].mappedSize))return i;return r.modules.size();}
inline void traceCandidates(ProbeIo&io,ProbeBudget&b,ProbeReport&r,const std::vector<ModuleSymbols>&symbols){
 size_t traced=0;for(auto&d:r.differences){if(!d.candidateAddress)continue;if(b.stop()){d.chainStatus="not-traced-"+b.reason;r.partial=true;continue;}if(traced++>=ProbeMaxChains){d.chainStatus="chain-count-budget-exceeded";r.partial=true;continue;}
  uint64_t at=d.candidateAddress;bool pe64=r.modules[d.module].pe64;std::set<uint64_t>visited;d.chainStatus="terminal-no-recognized-transfer";
  for(size_t depth=0;depth<8;++depth){if(b.stop()){d.chainStatus="partial-"+b.reason;r.partial=true;break;}if(!visited.insert(at).second){d.chainStatus="cycle";break;}MemoryRegion mem{};
   if(!io.query(at,mem)||!mem.executable){d.chainStatus="unreadable-or-nonexecutable-target";break;}
   std::array<uint8_t,16>one{},two{},v1{},v2{};size_t n1=readObserved(io,b,at,one.data(),one.size(),v1.data()),n2=readObserved(io,b,at,two.data(),two.size(),v2.data());size_t have=0;while(have<16&&v1[have]&&v2[have]&&one[have]==two[have])++have;
   if(depth==0){bool readable=true,same=true;for(size_t j=0;j<d.candidateBytes.size();++j){readable=readable&&v1[j]&&v2[j];same=same&&one[j]==d.candidateBytes[j]&&two[j]==d.candidateBytes[j];}if(!readable||!same){d.chainStatus=readable?"changed-since-scan":"candidate-unreadable-since-scan";r.partial=true;break;}}
   if(n1==16&&n2==16&&one!=two){d.chainStatus="chain-bytes-changed-between-reads";r.partial=true;break;}
   Transfer t{};if(!decodeTransfer(two.data(),have,at,pe64,t)){d.chainStatus=(n1<16||n2<16)?"terminal-unreadable-or-unrecognized":"terminal-unrecognized-byte-pattern";break;}
   ChainStep h;h.address=at;h.kind=t.kind;h.indirect=t.indirect;h.call=t.isCall;h.pointerAddress=t.pointerAddress;h.target=t.target;h.status="byte-pattern-candidate;instruction-boundary-unverified";
   if(t.indirect){std::array<uint8_t,8>a{},z{};size_t width=pe64?8:4;if(!readExact(io,b,t.pointerAddress,a.data(),width)||!readExact(io,b,t.pointerAddress,z.data(),width)||a!=z){h.status="indirect-pointer-unreadable-or-dynamic";d.chain.push_back(h);d.chainStatus=h.status;r.partial=true;break;}h.target=pe64?probeU64(z.data()):probeU32(z.data());}
   size_t owner=ownerOf(r,h.target);bool unlistedImage=false;if(owner<r.modules.size()){const auto&m=r.modules[owner];h.targetModule=m.name+" @ "+hexAddress(m.base);const uint64_t delta=h.target-m.base;const ExportSymbol*nearestSymbol=nullptr;for(const auto&e:symbols[owner].exports)if(e.forwarder.empty()&&!e.name.empty()&&e.rva<=delta&&(!nearestSymbol||e.rva>nearestSymbol->rva))nearestSymbol=&e;if(nearestSymbol){h.nearestExport=nearestSymbol->name;h.exportDisplacement=delta-nearestSymbol->rva;}}
   else{MemoryRegion target{};const bool queried=io.query(h.target,target);unlistedImage=queried&&target.image;h.targetModule=queried?target.image?"image-memory-not-in-module-snapshot":target.executable?"anonymous-executable-memory":"nonexecutable-memory":"unmapped-or-unreadable";}
   d.chain.push_back(h);if(unlistedImage){d.chainStatus="target-architecture-unverified";r.partial=true;break;}if(t.isCall){d.chainStatus="call-destination-observed;caller-continuation-not-traced";break;}at=h.target;if(owner<r.modules.size()){if(!r.modules[owner].structuralMatch){d.chainStatus="target-architecture-unverified";r.partial=true;break;}pe64=r.modules[owner].pe64;}if(depth==7)d.chainStatus="depth-limit-8";
  }
 }
}
inline bool sameSnapshot(std::vector<ModuleSnapshot>a,std::vector<ModuleSnapshot>b){
 auto cmp=[](const ModuleSnapshot&x,const ModuleSnapshot&y){return x.base<y.base;};std::sort(a.begin(),a.end(),cmp);std::sort(b.begin(),b.end(),cmp);if(a.size()!=b.size())return false;
 for(size_t i=0;i<a.size();++i)if(a[i].base!=b[i].base||a[i].size!=b[i].size||a[i].path!=b[i].path)return false;
 return true;
}
inline ProbeReport collect(ProbeIo&io,const Options&o,Progress&p){
 ProbeReport r;r.pid=o.pid;r.startedUtc=io.utc();r.processCreationTime=io.creationTime();ProbeBudget budget{io,p,io.nowMs(),0,{}};
 r.notes={"只读采集：不修改、挂起或恢复目标线程，不注入，不执行目标代码，也不卸载其他组件。", "默认基准为各模块当前磁盘文件；若磁盘也已被修改，必须提供同一构建的外部原始 EXE。文件名和结构匹配不能认证原版。SHA-256 是本次读取的基准文件字节摘要。", "默认比较所有已枚举模块的可执行节；主 EXE、非系统模块优先。没有比较的模块或字节明确列为未知。未枚举的手工映射模块及匿名可执行内存不做全覆盖扫描。", "按 PE 节映射并应用重定位；导入表等已建模的正常加载器写入被排除。差异也可能来自合法自修改或代码节内数据，不能全部归因为 Hook。改写可能来自工具箱自身、其他合法模组或平台；本工具不证明写入者，目录交集不代表作者归属。", "每页两次只读采样，模块头和文件身份复验；这是非原子观察，不能保证两次读取之间或扫描后没有改写。", "预算：512 模块、每文件 64 MiB、基准文件合计 512 MiB、归一化镜像 256 MiB、256 MiB 可执行节、所有远程读取请求合计 768 MiB（含双读与元数据）、4096 差异、256 跳转链，每链 8 跳、每项最多 32 B 样本、8 个参考交集标签；声明最多 8192/模块、16384/总计，导出标签最多 4096/模块、32768/总计。文本字段最多 2048 B，单份报告文件最多 32 MiB。扫描阶段最多 37 秒，给跳转链和结束复验预留 8 秒；总计 45 秒截止是协作式检查，不能中断正在进行的操作系统调用；UI 不等待该工作线程。", "IAT/EAT/vtable/数据指针完整性、VEH 注册、硬件断点、内核 Hook、加密或延迟安装钩子不在本次全覆盖范围。没有差异不等于没有 Hook。", "工具箱入口、签名与依赖交集来自参考地址映射。当前 EXE 的 ABI/指令边界未由该映射认证；跳转目标和最近导出仅为线索，不是写入者或功能用途证明。未列出的 MEM_IMAGE 架构未知时停止；匿名可执行跳板按上游执行模式解读仍只是候选。", ".syhks00 声明来自已归一化磁盘基准；记录了运行时可读性和是否不同。声明不证明已安装，也不证明当前改写者；声明长度 0 不代表零实际覆盖或未安装。"};
 if(r.processCreationTime.empty()){r.partial=true;r.error="process creation time unavailable; reads remain bound to retained process handle";}
 auto windows=referenceWindows(r);std::vector<ModuleSnapshot>snap;std::string snapshotError;r.enumerationComplete=io.snapshot(snap,snapshotError);
 if(!r.enumerationComplete){r.partial=true;r.error=snapshotError;}
 if(snap.size()>ProbeMaxModules){snap.resize(ProbeMaxModules);r.enumerationComplete=false;r.partial=true;r.error="module-count-budget-exceeded";}
 std::stable_sort(snap.begin(),snap.end(),[](const ModuleSnapshot&a,const ModuleSnapshot&b){if(a.main!=b.main)return a.main;return a.system<b.system;});
 for(const auto&s:snap){ModuleRecord m;m.path=s.path;m.name=s.name;m.base=s.base;m.mappedSize=s.size;m.main=s.main;m.system=s.system;r.modules.push_back(m);}
 p.modulesTotal.store(uint32_t(snap.size()),std::memory_order_relaxed);std::vector<ModuleSymbols>symbols(snap.size());size_t exportBudget=ProbeMaxTotalExports,declarationBudget=ProbeMaxTotalDeclarations;
 for(size_t i=0;i<snap.size();++i){collectModule(io,budget,snap[i],i,o,r,symbols[i],exportBudget,declarationBudget);p.modulesDone.store(uint32_t(i+1),std::memory_order_relaxed);}
 annotateReferences(r,windows);traceCandidates(io,budget,r,symbols);std::vector<ModuleSnapshot>after;std::string afterError;
 r.processAliveAtEnd=io.alive();if(!budget.stop())r.moduleListStable=io.snapshot(after,afterError)&&r.enumerationComplete&&sameSnapshot(snap,after);
 if(!r.processAliveAtEnd||!r.moduleListStable||snap.empty())r.partial=true;
 if(!r.moduleListStable)r.notes.push_back("模块列表结束复验失败或发生变化；后加入、卸载或替换的模块没有被认证为稳定覆盖。"+(afterError.empty()?std::string():" "+afterError));
 if(!budget.reason.empty()){r.partial=true;r.notes.push_back("停止原因："+budget.reason);}if(!budget.scanReason.empty()){r.partial=true;r.notes.push_back("扫描覆盖停止原因："+budget.scanReason);}r.cancelled=p.cancel.load(std::memory_order_relaxed);r.elapsedMs=io.nowMs()-budget.start;r.finishedUtc=io.utc();
 r.outcome=r.cancelled?"cancelled-partial-report":r.partial?"partial-observation":r.differences.empty()?"no-differences-observed-in-compared-executable-bytes":"differences-observed-writer-unknown";return r;
}
} // namespace hookprobe

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <tlhelp32.h>
#include <filesystem>
#include <fstream>
#include <cwctype>

namespace hookprobe {
struct ProbeHandle {
 HANDLE value=nullptr;explicit ProbeHandle(HANDLE h=nullptr):value(h){}~ProbeHandle(){if(value&&value!=INVALID_HANDLE_VALUE)CloseHandle(value);}
 ProbeHandle(const ProbeHandle&)=delete;ProbeHandle&operator=(const ProbeHandle&)=delete;
 explicit operator bool()const{return value&&value!=INVALID_HANDLE_VALUE;}
};
inline uint64_t fileTimeValue(FILETIME t){return (uint64_t(t.dwHighDateTime)<<32)|t.dwLowDateTime;}
inline std::string probeWinError(const char*what){return std::string(what)+" (Win32 "+std::to_string(GetLastError())+")";}
inline std::wstring lowerPath(std::wstring p){for(auto&c:p){if(c==L'/')c=L'\\';c=std::towlower(c);}return p;}
inline std::string fileStampId(const BY_HANDLE_FILE_INFORMATION&i){return hexAddress(i.dwVolumeSerialNumber)+":"+hexAddress((uint64_t(i.nFileIndexHigh)<<32)|i.nFileIndexLow);}
inline bool probeFileStamp(HANDLE h,FileIdentity&f){BY_HANDLE_FILE_INFORMATION i{};if(!GetFileInformationByHandle(h,&i))return false;f.bytes=(uint64_t(i.nFileSizeHigh)<<32)|i.nFileSizeLow;f.writeTime=fileTimeValue(i.ftLastWriteTime);f.identity=fileStampId(i);return !(i.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT));}
inline bool sameFileStamp(const FileIdentity&a,const FileIdentity&b){return a.identity==b.identity&&a.bytes==b.bytes&&a.writeTime==b.writeTime;}
class WindowsProbeIo final:public ProbeIo {
 HANDLE process_;uint32_t pid_;Progress&progress_;uint64_t start_,diskReadBytes_=0;std::wstring mainPath_,windowsPath_;std::string creation_;
 bool shouldStop(){return progress_.cancel.load(std::memory_order_relaxed)||GetTickCount64()-start_>=ProbeDeadlineMs||!alive();}
public:
 WindowsProbeIo(HANDLE h,uint32_t pid,Progress&p):process_(h),pid_(pid),progress_(p),start_(GetTickCount64()){
  std::vector<wchar_t>path(32768);DWORD n=DWORD(path.size());if(QueryFullProcessImageNameW(process_,0,path.data(),&n))mainPath_=lowerPath(std::wstring(path.data(),n));
  std::array<wchar_t,32768>w{};UINT wn=GetWindowsDirectoryW(w.data(),UINT(w.size()));if(wn&&wn<w.size())windowsPath_=lowerPath(std::wstring(w.data(),wn))+L"\\";
  FILETIME a{},b{},c{},d{};if(GetProcessTimes(process_,&a,&b,&c,&d))creation_=hexAddress(fileTimeValue(a));
 }
 uint64_t nowMs()override{return GetTickCount64();}
 std::string utc()override{SYSTEMTIME t{};GetSystemTime(&t);char s[40]{};snprintf(s,sizeof s,"%04u-%02u-%02uT%02u:%02u:%02u.%03uZ",unsigned(t.wYear),unsigned(t.wMonth),unsigned(t.wDay),unsigned(t.wHour),unsigned(t.wMinute),unsigned(t.wSecond),unsigned(t.wMilliseconds));return s;}
 std::string creationTime()override{return creation_;}
 bool alive()override{return WaitForSingleObject(process_,0)==WAIT_TIMEOUT;}
 bool snapshot(std::vector<ModuleSnapshot>&out,std::string&error)override{
  out.clear();HANDLE raw=INVALID_HANDLE_VALUE;for(unsigned retry=0;retry<3&&!shouldStop();++retry){raw=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE|TH32CS_SNAPMODULE32,pid_);if(raw!=INVALID_HANDLE_VALUE||GetLastError()!=ERROR_BAD_LENGTH)break;}
  ProbeHandle h(raw);if(!h){error=probeWinError("module snapshot failed");return false;}MODULEENTRY32W e{};e.dwSize=sizeof e;if(!Module32FirstW(h.value,&e)){error=probeWinError("module enumeration failed");return false;}
  bool first=true;do{ModuleSnapshot s;s.path=e.szExePath;s.name=utf8(e.szModule);s.base=uint64_t(reinterpret_cast<uintptr_t>(e.modBaseAddr));s.size=e.modBaseSize;std::wstring p=lowerPath(s.path);s.main=!mainPath_.empty()?p==mainPath_:first;s.system=!windowsPath_.empty()&&p.compare(0,windowsPath_.size(),windowsPath_)==0;first=false;out.push_back(s);if(out.size()>=ProbeMaxModules){error="module enumeration reached 512-module cap; additional modules unknown";return false;}if(shouldStop()){error="module enumeration interrupted";return false;}e.dwSize=sizeof e;}while(Module32NextW(h.value,&e));
  if(GetLastError()!=ERROR_NO_MORE_FILES){error=probeWinError("module enumeration incomplete");return false;}return true;
 }
 bool query(uint64_t address,MemoryRegion&out)override{
  if(address>uint64_t((std::numeric_limits<uintptr_t>::max)()))return false;MEMORY_BASIC_INFORMATION m{};if(!VirtualQueryEx(process_,reinterpret_cast<const void*>(uintptr_t(address)),&m,sizeof m))return false;
  out.base=uint64_t(reinterpret_cast<uintptr_t>(m.BaseAddress));out.size=m.RegionSize;out.allocationBase=uint64_t(reinterpret_cast<uintptr_t>(m.AllocationBase));out.image=m.Type==MEM_IMAGE;
  const DWORD p=m.Protect&0xff;out.readable=m.State==MEM_COMMIT&&!(m.Protect&(PAGE_GUARD|PAGE_NOACCESS))&&(p==PAGE_READONLY||p==PAGE_READWRITE||p==PAGE_WRITECOPY||p==PAGE_EXECUTE_READ||p==PAGE_EXECUTE_READWRITE||p==PAGE_EXECUTE_WRITECOPY);
  out.executable=m.State==MEM_COMMIT&&!(m.Protect&(PAGE_GUARD|PAGE_NOACCESS))&&(p==PAGE_EXECUTE||p==PAGE_EXECUTE_READ||p==PAGE_EXECUTE_READWRITE||p==PAGE_EXECUTE_WRITECOPY);return true;
 }
 size_t read(uint64_t address,uint8_t*out,size_t n)override{SIZE_T got=0;if(address>uint64_t((std::numeric_limits<uintptr_t>::max)()))return 0;ReadProcessMemory(process_,reinterpret_cast<const void*>(uintptr_t(address)),out,n,&got);return size_t(got);}
 bool readFile(const std::wstring&path,size_t maximum,std::vector<uint8_t>&bytes,FileIdentity&stamp,std::string&error)override{
  bytes.clear();stamp.path=path;stamp.readStartedMs=nowMs();maximum=size_t((std::min)(uint64_t(maximum),ProbeMaxDiskReadBytes-diskReadBytes_));
  // Network/device baselines can block indefinitely inside a filesystem driver.
  // Reject explicit UNC/device and mapped remote paths. Parent reparse points
  // and a filesystem driver can still block an individual OS call.
  if(path.size()<3||path[1]!=L':'||(path[2]!=L'\\'&&path[2]!=L'/')){error="baseline must be a local absolute drive path; network/device paths are not read";return false;}
  wchar_t root[4]={path[0],L':',L'\\',0};UINT drive=GetDriveTypeW(root);if(drive==DRIVE_REMOTE||drive==DRIVE_UNKNOWN||drive==DRIVE_NO_ROOT_DIR){error="remote or unknown baseline drive skipped";return false;}
  ProbeHandle f(CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_FLAG_SEQUENTIAL_SCAN|FILE_FLAG_OPEN_REPARSE_POINT,nullptr));
  if(!f){error=probeWinError("baseline open failed");return false;}if(!probeFileStamp(f.value,stamp)){error="baseline metadata unavailable or final path is a reparse point";return false;}
  if(!stamp.bytes||stamp.bytes>maximum){error="baseline empty or exceeds remaining per-file/total baseline byte budget";return false;}bytes.resize(size_t(stamp.bytes));size_t at=0;
  while(at<bytes.size()){if(shouldStop()){error="baseline read cancelled, expired or process exited";return false;}DWORD got=0,n=DWORD((std::min)(bytes.size()-at,size_t(1024*1024)));stamp.readRequestedBytes+=n;diskReadBytes_+=n;if(!ReadFile(f.value,bytes.data()+at,n,&got,nullptr)||got!=n){error=probeWinError("baseline read incomplete");return false;}at+=got;}
  FileIdentity after;stamp.readFinishedMs=nowMs();stamp.unchangedAfterRead=probeFileStamp(f.value,after)&&sameFileStamp(stamp,after);return true;
 }
 bool fileUnchanged(const FileIdentity&stamp)override{if(stamp.path.empty()||shouldStop())return false;ProbeHandle h(CreateFileW(stamp.path.c_str(),FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,nullptr));FileIdentity after;return h&&probeFileStamp(h.value,after)&&sameFileStamp(stamp,after);}
};
inline bool writeProbeFile(const std::filesystem::path&path,const std::string&data){if(data.size()>ProbeMaxReportBytes)return false;std::ofstream f(path,std::ios::binary|std::ios::trunc);if(!f)return false;f.write(data.data(),std::streamsize(data.size()));f.flush();if(!f)return false;f.close();return bool(f);}
inline Result run(const Options&o,Progress&p){
 Result result;try{
  if(!o.pid||o.outputDirectory.empty()){result.summary=L"探测失败：缺少进程 PID 或输出目录。";return result;}
  // Keep the handle until all scans and report construction finish; PID reuse
  // cannot silently redirect subsequent reads to another process.
  ProbeHandle process(OpenProcess(PROCESS_QUERY_INFORMATION|PROCESS_VM_READ|SYNCHRONIZE,FALSE,o.pid));
  if(!process){result.summary=L"无法以只读权限打开游戏进程（Win32 "+std::to_wstring(GetLastError())+L"）。";return result;}
  WindowsProbeIo io(process.value,o.pid,p);ProbeReport report=collect(io,o,p);result.cancelled=report.cancelled;result.partial=report.partial;result.findings=uint32_t(report.differences.size());
  for(const auto&m:report.modules)if(m.structuralMatch&&m.comparedBytes)++result.modulesScanned;
  std::error_code ec;std::filesystem::create_directories(std::filesystem::path(o.outputDirectory),ec);if(ec){result.partial=true;result.summary=L"已探测，但不能创建报告目录。";return result;}
  const auto json=std::filesystem::path(o.outputDirectory)/L"report.json",html=std::filesystem::path(o.outputDirectory)/L"report.html";
  if(writeProbeFile(json,reportJson(report)))result.jsonPath=json.wstring();if(writeProbeFile(html,reportHtml(report)))result.htmlPath=html.wstring();result.success=!result.jsonPath.empty()&&!result.htmlPath.empty();
  result.partial=result.partial||!result.success;result.summary=(result.cancelled?L"已取消，已保留局部报告。 ":!result.success?L"探测完成，但部分报告写入失败。 ":result.partial?L"报告已导出，部分区域未能确认。 ":L"报告已导出。 ");result.summary+=L"已比较 "+std::to_wstring(result.modulesScanned)+L" 个模块，记录 "+std::to_wstring(result.findings)+L" 处差异/动态样本；写入者未知。";
 }catch(const std::exception&){result.partial=true;result.summary=L"探测因资源或文件错误中止，已有报告路径仍可打开。";}catch(...){result.partial=true;result.summary=L"探测意外中止。";}return result;
}
} // namespace hookprobe
#endif
