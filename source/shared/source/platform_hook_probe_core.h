#pragma once
// Read-only PE/image comparison helpers. No process APIs, executable writes,
// calls to discovered addresses, or complete x86 instruction decoding.
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace hookprobe {
inline constexpr uint32_t MaxImageBytes=256u*1024u*1024u;
inline constexpr uint32_t MaxHeaderBytes=1024u*1024u;
inline constexpr size_t MaxSections=96,MaxLoaderRanges=4096,MaxImportThunks=65536;
inline constexpr size_t MaxRelocations=1048576;
struct PeDirectory {uint32_t rva=0,size=0;};
struct PeSection {
 std::string name;uint32_t rva=0,virtualSize=0,rawOffset=0,rawSize=0,characteristics=0;
 uint32_t mappedSize()const{return virtualSize?virtualSize:rawSize;}
 bool executable()const{return (characteristics&0x20000000u)!=0;}
};
struct PeImage {
 bool pe64=false;uint16_t machine=0,characteristics=0;
 uint32_t timestamp=0,imageSize=0,headersSize=0,entryRva=0,checksum=0;
 uint32_t sectionAlignment=0,fileAlignment=0;
 uint64_t preferredBase=0;std::vector<PeSection> sections;PeDirectory directories[16]{};
};
struct IgnoreRange {uint32_t rva=0,size=0;std::string reason;};
struct NormalizedImage {
 PeImage pe;uint64_t actualBase=0;std::vector<uint8_t> bytes;
 std::vector<IgnoreRange> loaderWrites;uint32_t relocations=0;
};
// Header-only memory parsing requires DOS, NT and the complete section table;
// fileLayout=false never interprets a disk raw offset as a memory offset.
bool parsePe(const uint8_t*data,size_t size,PeImage&out,std::string&error,bool fileLayout=true);
bool compatibleLayout(const PeImage&disk,const PeImage&live,std::string&error);
bool normalizeImage(const std::vector<uint8_t>&file,const PeImage&pe,uint64_t actualBase,
                    NormalizedImage&out,std::string&error);
struct DiffLimits {size_t maxRanges=8192,sampleBytes=32;};
struct DiffRange {
 uint32_t rva=0,length=0;std::vector<uint8_t> expected,actual;
};
struct DiffResult {
 std::vector<DiffRange> ranges;
 uint64_t comparedBytes=0,ignoredBytes=0,unreadableBytes=0,unstableBytes=0,changedBytes=0;
 bool truncated=false;
};
// The supplied chunk must be wholly in one executable section. valid==nullptr
// means every supplied byte was read and stable. Otherwise valid[i] is
// 0=unreadable, 1=stable, 2=changed between reads (counted, never compared).
// Use non-overlapping chunks; incomplete reads never become zero-filled data.
bool diffExecutableChunk(const NormalizedImage&expected,uint32_t rva,const uint8_t*current,
                         size_t size,const uint8_t*valid,DiffResult&out,std::string&error,
                         DiffLimits limits={});
struct Transfer {
 std::string kind;uint8_t length=0;uint64_t target=0,pointerAddress=0;
 bool indirect=false,isCall=false,instructionBoundaryVerified=false;
};
// Recognized byte patterns are candidates only. For an indirect transfer the
// caller must read pointerAddress using the matching PE pointer width.
bool decodeTransfer(const uint8_t*data,size_t size,uint64_t address,bool pe64,Transfer&out);
struct ExportSymbol {uint32_t rva=0,ordinal=0;std::string name,forwarder;};
bool readExports(const NormalizedImage&image,std::vector<ExportSymbol>&out,std::string&error,
                 size_t maximum=16384);
struct HookDeclaration {uint32_t address=0,bytes=0;std::string name;};
// Optional RA2/Syringe declaration metadata; declaration is not proof of a
// currently installed hook. Only the reviewed 32-bit 16-byte record is read.
bool readHookDeclarations(const NormalizedImage&image,std::vector<HookDeclaration>&out,
                          std::string&error,size_t maximum=8192);

namespace pecore_detail {
inline bool fail(std::string&error,const char*why){error=why;return false;}
inline bool span(uint64_t total,uint64_t at,uint64_t count){return at<=total&&count<=total-at;}
inline uint16_t u16(const uint8_t*p){return uint16_t(p[0])|uint16_t(p[1])<<8;}
inline uint32_t u32(const uint8_t*p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
inline uint64_t u64(const uint8_t*p){return uint64_t(u32(p))|uint64_t(u32(p+4))<<32;}
inline void put16(uint8_t*p,uint16_t v){p[0]=uint8_t(v);p[1]=uint8_t(v>>8);}
inline void put32(uint8_t*p,uint32_t v){for(unsigned i=0;i<4;++i)p[i]=uint8_t(v>>(i*8));}
inline void put64(uint8_t*p,uint64_t v){for(unsigned i=0;i<8;++i)p[i]=uint8_t(v>>(i*8));}
inline bool power2(uint32_t v){return v&&!(v&(v-1));}
inline bool sameSection(const PeSection&a,const PeSection&b){
 return a.name==b.name&&a.rva==b.rva&&a.virtualSize==b.virtualSize&&a.rawOffset==b.rawOffset&&
        a.rawSize==b.rawSize&&a.characteristics==b.characteristics;
}
inline bool stringAt(const std::vector<uint8_t>&bytes,uint32_t at,std::string&out,size_t maximum=256){
 out.clear();if(at>=bytes.size())return false;
 for(size_t i=0;i<maximum&&uint64_t(at)+i<bytes.size();++i){const uint8_t c=bytes[at+i];if(!c)return true;out.push_back(char(c));}
 out.clear();return false;
}
inline bool addMask(NormalizedImage&image,uint32_t at,uint64_t bytes,const char*reason,std::string&error){
 if(!bytes)return true;
 if(!at||!span(image.bytes.size(),at,bytes))return fail(error,"loader-write-out-of-image");
 if(image.loaderWrites.size()>=MaxLoaderRanges)return fail(error,"too-many-loader-write-ranges");
 image.loaderWrites.push_back({at,uint32_t(bytes),reason});return true;
}
inline void mergeMasks(std::vector<IgnoreRange>&ranges){
 std::sort(ranges.begin(),ranges.end(),[](const IgnoreRange&a,const IgnoreRange&b){return a.rva<b.rva||(a.rva==b.rva&&a.size<b.size);});
 size_t used=0;
 for(auto&range:ranges){
  if(used&&uint64_t(range.rva)<=uint64_t(ranges[used-1].rva)+ranges[used-1].size){
   auto&last=ranges[used-1];const uint64_t end=std::max(uint64_t(last.rva)+last.size,uint64_t(range.rva)+range.size);
   if(last.reason!=range.reason&&last.reason.find(range.reason)==std::string::npos)last.reason+=";"+range.reason;
   last.size=uint32_t(end-last.rva);
  }else{if(&ranges[used]!=&range)ranges[used]=std::move(range);++used;}
 }ranges.resize(used);
}
inline bool vaToRva(const NormalizedImage&image,uint64_t va,uint32_t&out){
 if(va<image.actualBase||va-image.actualBase>=image.bytes.size())return false;
 out=uint32_t(va-image.actualBase);return true;
}
inline bool thunkMask(NormalizedImage&image,uint32_t lookup,uint32_t target,const char*reason,
                      uint64_t&totalThunks,uint32_t&count,std::string&error){
 const uint32_t width=image.pe.pe64?8u:4u;count=0;
 if(!lookup||!target)return fail(error,"import-thunk-address-missing");
 for(;;){
  const uint64_t source=uint64_t(lookup)+uint64_t(count)*width;
  if(!span(image.bytes.size(),source,width))return fail(error,"import-thunk-list-out-of-image");
  const uint64_t value=width==8?u64(image.bytes.data()+source):u32(image.bytes.data()+source);
  if(!value)break;
  if(++totalThunks>MaxImportThunks)return fail(error,"import-thunk-limit");
  ++count;
 }
 // Exclude only actual non-null IAT cells, not an untrusted broad IAT
 // directory. A modified terminator is still visible in an executable span.
 if(!span(image.bytes.size(),target,(uint64_t(count)+1)*width))return fail(error,"import-address-table-out-of-image");
 return addMask(image,target,uint64_t(count)*width,reason,error);
}
inline bool loaderMasks(NormalizedImage&image,std::string&error){
 uint64_t totalThunks=0;const uint32_t width=image.pe.pe64?8u:4u;
 const auto&imports=image.pe.directories[1];
 if(imports.rva||imports.size){
  if(!imports.rva||imports.size<20||!span(image.bytes.size(),imports.rva,imports.size))return fail(error,"invalid-import-directory");
  bool ended=false;
  for(uint64_t offset=0;span(imports.size,offset,20);offset+=20){
   const auto*p=image.bytes.data()+imports.rva+offset;bool zero=true;for(unsigned i=0;i<20;++i)zero=zero&&!p[i];
   if(zero){ended=true;break;}
   const uint32_t lookup=u32(p),name=u32(p+12),target=u32(p+16);
   if(!name||name>=image.bytes.size())return fail(error,"invalid-import-name");
   uint32_t count=0;
   if(!thunkMask(image,lookup?lookup:target,target,"import-address-table",totalThunks,count,error))return false;
  }
  if(!ended)return fail(error,"unterminated-import-descriptors");
 }
 const auto&delay=image.pe.directories[13];
 if(delay.rva||delay.size){
  if(!delay.rva||delay.size<32||!span(image.bytes.size(),delay.rva,delay.size))return fail(error,"invalid-delay-import-directory");
  bool ended=false;
  for(uint64_t offset=0;span(delay.size,offset,32);offset+=32){
   const auto*p=image.bytes.data()+delay.rva+offset;bool zero=true;for(unsigned i=0;i<32;++i)zero=zero&&!p[i];
   if(zero){ended=true;break;}
   const uint32_t attributes=u32(p);if(attributes&~1u)return fail(error,"unknown-delay-import-attributes");
   uint32_t fields[6]{};for(unsigned i=0;i<6;++i){
    const uint32_t value=u32(p+4+i*4);if(!value)continue;
    if(attributes&1u){if(value>=image.bytes.size())return fail(error,"delay-import-rva-out-of-image");fields[i]=value;}
    else if(!vaToRva(image,value,fields[i]))return fail(error,"delay-import-va-out-of-image");
   }
   // name, module handle, IAT, INT, bound IAT, unload IAT.
   if(!fields[0]||!fields[2]||!fields[3])return fail(error,"incomplete-delay-import-descriptor");
   uint32_t count=0;
   if(!thunkMask(image,fields[3],fields[2],"delay-import-address-table",totalThunks,count,error))return false;
   if(fields[1]&&!addMask(image,fields[1],width,"delay-import-module-handle",error))return false;
   if(fields[5]&&!addMask(image,fields[5],uint64_t(count)*width,"delay-import-unload-table",error))return false;
  }
  if(!ended)return fail(error,"unterminated-delay-import-descriptors");
 }
 const auto&tls=image.pe.directories[9];
 if(tls.rva||tls.size){
  const uint32_t required=image.pe.pe64?40u:24u;
  if(!tls.rva||tls.size<required||!span(image.bytes.size(),tls.rva,required))return fail(error,"invalid-tls-directory");
  const uint8_t*p=image.bytes.data()+tls.rva;const uint64_t address=image.pe.pe64?u64(p+16):u32(p+8);
  if(address){uint32_t at=0;if(!vaToRva(image,address,at)||!addMask(image,at,4,"tls-index",error))return fail(error,"tls-index-out-of-image");}
 }
 mergeMasks(image.loaderWrites);return true;
}
}

inline bool parsePe(const uint8_t*data,size_t size,PeImage&out,std::string&error,bool fileLayout){
 using namespace pecore_detail;out={};error.clear();
 if(!data||size<64||u16(data)!=0x5A4D)return fail(error,"missing-dos-header");
 const uint32_t nt=u32(data+60);
 if(nt<64||nt>MaxHeaderBytes-24||!span(size,nt,24)||u32(data+nt)!=0x4550)return fail(error,"invalid-nt-header");
 const uint16_t sections=u16(data+nt+6),optionalBytes=u16(data+nt+20);
 if(!sections||sections>MaxSections||optionalBytes<2||optionalBytes>4096||!span(size,uint64_t(nt)+24,optionalBytes))return fail(error,"invalid-pe-header-size");
 const uint8_t*optional=data+nt+24;const uint16_t magic=u16(optional);
 const bool pe64=magic==0x20B;if(!pe64&&magic!=0x10B)return fail(error,"unsupported-optional-header");
 const uint32_t fixed=pe64?112u:96u;
 if(optionalBytes<fixed)return fail(error,"short-optional-header");
 PeImage result;result.pe64=pe64;result.machine=u16(data+nt+4);result.characteristics=u16(data+nt+22);
 if((pe64&&result.machine!=0x8664)||(!pe64&&result.machine!=0x14C))return fail(error,"unsupported-machine");
 result.timestamp=u32(data+nt+8);result.entryRva=u32(optional+16);
 result.preferredBase=pe64?u64(optional+24):u32(optional+28);
 result.sectionAlignment=u32(optional+32);result.fileAlignment=u32(optional+36);
 result.imageSize=u32(optional+56);result.headersSize=u32(optional+60);result.checksum=u32(optional+64);
 if(!result.imageSize||result.imageSize>MaxImageBytes||!result.headersSize||result.headersSize>MaxHeaderBytes||
    result.headersSize>result.imageSize||!power2(result.sectionAlignment)||!power2(result.fileAlignment)||
    result.sectionAlignment<result.fileAlignment||result.entryRva>=result.imageSize)return fail(error,"invalid-image-layout");
 if(fileLayout&&result.headersSize>size)return fail(error,"headers-beyond-file");
 const uint32_t directoryCount=u32(optional+(pe64?108:92));
 if(directoryCount>16||uint64_t(fixed)+uint64_t(directoryCount)*8>optionalBytes)return fail(error,"invalid-data-directory-count");
 for(uint32_t i=0;i<directoryCount;++i){
  auto&directory=result.directories[i];directory.rva=u32(optional+fixed+i*8);directory.size=u32(optional+fixed+i*8+4);
  if(directory.size&&i==4){if(fileLayout&&!span(size,directory.rva,directory.size))return fail(error,"certificate-beyond-file");}
  else if(directory.size&&!span(result.imageSize,directory.rva,directory.size))return fail(error,"directory-out-of-image");
  else if(directory.rva&&i!=4&&directory.rva>=result.imageSize)return fail(error,"directory-address-out-of-image");
 }
 const uint64_t table=uint64_t(nt)+24+optionalBytes;
 if(!span(size,table,uint64_t(sections)*40)||!span(result.headersSize,table,uint64_t(sections)*40))return fail(error,"section-table-out-of-headers");
 for(uint32_t i=0;i<sections;++i){
  const uint8_t*p=data+table+i*40;PeSection section;
  for(unsigned n=0;n<8&&p[n];++n)section.name.push_back(char(p[n]));
  section.virtualSize=u32(p+8);section.rva=u32(p+12);section.rawSize=u32(p+16);section.rawOffset=u32(p+20);section.characteristics=u32(p+36);
  const uint32_t mapped=section.mappedSize();
  if(!span(result.imageSize,section.rva,mapped)||(mapped&&section.rva<result.headersSize))return fail(error,"section-out-of-image");
  if(section.rawSize&&(section.rawOffset<result.headersSize||(fileLayout&&!span(size,section.rawOffset,section.rawSize))))return fail(error,"section-raw-data-out-of-file");
  for(const auto&previous:result.sections){
   if(mapped&&previous.mappedSize()&&uint64_t(section.rva)<uint64_t(previous.rva)+previous.mappedSize()&&
      uint64_t(previous.rva)<uint64_t(section.rva)+mapped)return fail(error,"overlapping-virtual-sections");
   if(fileLayout&&section.rawSize&&previous.rawSize&&uint64_t(section.rawOffset)<uint64_t(previous.rawOffset)+previous.rawSize&&
      uint64_t(previous.rawOffset)<uint64_t(section.rawOffset)+section.rawSize)return fail(error,"overlapping-raw-sections");
  }
  result.sections.push_back(std::move(section));
 }
 out=std::move(result);return true;
}

inline bool compatibleLayout(const PeImage&disk,const PeImage&live,std::string&error){
 using namespace pecore_detail;error.clear();
 if(disk.pe64!=live.pe64||disk.machine!=live.machine)return fail(error,"baseline-machine-mismatch");
 if(disk.timestamp!=live.timestamp)return fail(error,"baseline-timestamp-mismatch");
 if(disk.imageSize!=live.imageSize||disk.headersSize!=live.headersSize||disk.preferredBase!=live.preferredBase||
    disk.entryRva!=live.entryRva||disk.sectionAlignment!=live.sectionAlignment||disk.fileAlignment!=live.fileAlignment||
    disk.characteristics!=live.characteristics||disk.checksum!=live.checksum)return fail(error,"baseline-header-layout-mismatch");
 if(disk.sections.size()!=live.sections.size())return fail(error,"baseline-section-count-mismatch");
 for(size_t i=0;i<disk.sections.size();++i)if(!sameSection(disk.sections[i],live.sections[i]))return fail(error,"baseline-section-layout-mismatch");
 for(unsigned i=0;i<16;++i)if(disk.directories[i].rva!=live.directories[i].rva||disk.directories[i].size!=live.directories[i].size)return fail(error,"baseline-directory-layout-mismatch");
 return true;
}

inline bool normalizeImage(const std::vector<uint8_t>&file,const PeImage&pe,uint64_t actualBase,
                           NormalizedImage&out,std::string&error){
 using namespace pecore_detail;out={};error.clear();PeImage parsed;
 if(!parsePe(file.data(),file.size(),parsed,error,true)||!compatibleLayout(pe,parsed,error))return false;
 if(actualBase>std::numeric_limits<uint64_t>::max()-parsed.imageSize||
    (!parsed.pe64&&(actualBase>0xFFFFFFFFull||uint64_t(parsed.imageSize)>0x100000000ull-actualBase)))return fail(error,"loaded-image-address-overflow");
 const uint64_t delta=actualBase-parsed.preferredBase;
 if(delta&&(parsed.characteristics&1u))return fail(error,"rebased-image-with-stripped-relocations");
 if(delta&&!parsed.directories[5].rva&&!parsed.directories[5].size)return fail(error,"missing-relocations-on-rebased-image");
 NormalizedImage result;result.pe=parsed;result.actualBase=actualBase;result.bytes.resize(parsed.imageSize,0);
 std::memcpy(result.bytes.data(),file.data(),parsed.headersSize);
 for(const auto&section:parsed.sections){const uint32_t bytes=std::min(section.mappedSize(),section.rawSize);
  if(bytes)std::memcpy(result.bytes.data()+section.rva,file.data()+section.rawOffset,bytes);
 }
 const auto&reloc=parsed.directories[5];
 if(reloc.rva||reloc.size){
  if(!reloc.rva||reloc.size<8||!span(result.bytes.size(),reloc.rva,reloc.size))return fail(error,"invalid-relocation-directory");
  struct Operation {uint32_t at=0;uint16_t type=0,extra=0;uint8_t bytes=0;};
  std::vector<Operation> operations;
  for(uint64_t offset=0;offset<reloc.size;){
   const uint64_t remaining=reloc.size-offset;const uint8_t*p=result.bytes.data()+reloc.rva+offset;
   if(remaining<8)return fail(error,"truncated-relocation-block");
   const uint32_t page=u32(p),bytes=u32(p+4);
   if(!page&&!bytes){bool zero=true;for(uint64_t i=0;i<remaining;++i)zero=zero&&!p[i];if(zero)break;return fail(error,"invalid-relocation-padding");}
   if((page&0xFFF)||bytes<8||(bytes&1)||bytes>remaining)return fail(error,"invalid-relocation-block");
   for(uint32_t at=8;at<bytes;at+=2){
    const uint16_t entry=u16(p+at),type=entry>>12;const uint64_t target=uint64_t(page)+(entry&0xFFF);
    if(!type)continue;
    Operation operation;operation.type=type;
    if((!parsed.pe64&&(type==1||type==2||type==4))){operation.bytes=2;
     if(type==4){if(at+4>bytes)return fail(error,"truncated-highadj-relocation");operation.extra=u16(p+at+2);at+=2;}
    }else if(!parsed.pe64&&type==3)operation.bytes=4;
    else if(parsed.pe64&&type==10)operation.bytes=8;
    else return fail(error,"unsupported-relocation-type");
    if(!span(result.bytes.size(),target,operation.bytes))return fail(error,"relocation-target-out-of-image");
    if(operations.size()>=MaxRelocations)return fail(error,"relocation-count-limit");
    operation.at=uint32_t(target);operations.push_back(operation);
   }offset+=bytes;
  }
  // Decode before modifying the image: malformed relocations pointing back
  // into their own directory cannot change the parser's loop or entry data.
  std::sort(operations.begin(),operations.end(),[](const Operation&a,const Operation&b){return a.at<b.at;});
  for(size_t i=1;i<operations.size();++i)if(uint64_t(operations[i-1].at)+operations[i-1].bytes>operations[i].at)return fail(error,"overlapping-relocation-targets");
  for(const auto&operation:operations){uint8_t*p=result.bytes.data()+operation.at;
   switch(operation.type){
    case 1:put16(p,uint16_t(u16(p)+uint16_t(uint32_t(delta)>>16)));break;
    case 2:put16(p,uint16_t(u16(p)+uint16_t(delta)));break;
    case 3:put32(p,u32(p)+uint32_t(delta));break;
    case 4:{const int64_t adjusted=(int64_t(u16(p))<<16)+int16_t(operation.extra)+int64_t(actualBase)-int64_t(parsed.preferredBase)+0x8000;
     put16(p,uint16_t(uint64_t(adjusted)>>16));break;}
    case 10:put64(p,u64(p)+delta);break;
    default:return fail(error,"unsupported-relocation-type");
   }
  }result.relocations=uint32_t(operations.size());
 }
 if(!loaderMasks(result,error))return false;
 out=std::move(result);return true;
}

inline bool diffExecutableChunk(const NormalizedImage&expected,uint32_t rva,const uint8_t*current,
                                size_t size,const uint8_t*valid,DiffResult&out,std::string&error,DiffLimits limits){
 using namespace pecore_detail;error.clear();if(!size)return true;
 if(!current||!span(expected.bytes.size(),rva,size))return fail(error,"comparison-chunk-out-of-image");
 bool executable=false;for(const auto&section:expected.pe.sections)if(section.executable()&&rva>=section.rva&&
  uint64_t(rva)+size<=uint64_t(section.rva)+section.mappedSize()){executable=true;break;}
 if(!executable)return fail(error,"comparison-chunk-not-executable");
 if(valid)for(size_t i=0;i<size;++i)if(valid[i]>2)return fail(error,"invalid-comparison-validity");
 limits.maxRanges=std::min<size_t>(limits.maxRanges,65536);limits.sampleBytes=std::min<size_t>(limits.sampleBytes,128);
 size_t mask=0;while(mask<expected.loaderWrites.size()&&uint64_t(expected.loaderWrites[mask].rva)+expected.loaderWrites[mask].size<=rva)++mask;
 for(size_t i=0;i<size;++i){
  const uint32_t at=uint32_t(uint64_t(rva)+i);
  while(mask<expected.loaderWrites.size()&&uint64_t(expected.loaderWrites[mask].rva)+expected.loaderWrites[mask].size<=at)++mask;
  if(mask<expected.loaderWrites.size()&&at>=expected.loaderWrites[mask].rva){++out.ignoredBytes;continue;}
  if(valid&&!valid[i]){++out.unreadableBytes;continue;}
  if(valid&&valid[i]==2){++out.unstableBytes;continue;}
  ++out.comparedBytes;const uint8_t before=expected.bytes[at],after=current[i];if(before==after)continue;
  ++out.changedBytes;
  if(!out.ranges.empty()&&uint64_t(out.ranges.back().rva)+out.ranges.back().length==at){
   auto&range=out.ranges.back();++range.length;
   if(range.expected.size()<limits.sampleBytes){range.expected.push_back(before);range.actual.push_back(after);}
  }else if(out.ranges.size()<limits.maxRanges){
   DiffRange range;range.rva=at;range.length=1;if(limits.sampleBytes){range.expected.push_back(before);range.actual.push_back(after);}out.ranges.push_back(std::move(range));
  }else out.truncated=true;
 }
 return true;
}

inline bool decodeTransfer(const uint8_t*data,size_t size,uint64_t address,bool pe64,Transfer&out){
 using namespace pecore_detail;out={};if(!data||!size)return false;
 auto direct=[&](const char*kind,uint8_t length,int64_t displacement,bool call){
  if(address>std::numeric_limits<uint64_t>::max()-length)return false;
  const uint64_t next=address+length;
  uint64_t target=0;
  if(pe64){
   if(displacement<0){const uint64_t magnitude=uint64_t(-displacement);if(next<magnitude)return false;target=next-magnitude;}
   else{if(next>std::numeric_limits<uint64_t>::max()-uint64_t(displacement))return false;target=next+uint64_t(displacement);}
  }else{if(address>0xFFFFFFFFull)return false;target=uint32_t(uint32_t(next)+uint32_t(displacement));}
  out.kind=kind;out.length=length;out.target=target;out.isCall=call;return true;
 };
 if(size>=5&&(data[0]==0xE9||data[0]==0xE8))return direct(data[0]==0xE9?"jmp-rel32":"call-rel32",5,int32_t(u32(data+1)),data[0]==0xE8);
 if(size>=2&&data[0]==0xEB)return direct("jmp-rel8",2,int8_t(data[1]),false);
 if(size>=6&&data[0]==0xFF&&(data[1]==0x25||data[1]==0x15)){
  out.kind=data[1]==0x25?"jmp-indirect":"call-indirect";out.length=6;out.indirect=true;out.isCall=data[1]==0x15;
  if(pe64){
   const int64_t displacement=int32_t(u32(data+2));
   if(address>std::numeric_limits<uint64_t>::max()-6){out={};return false;}
   const uint64_t next=address+6;
   if(displacement<0){const uint64_t distance=uint64_t(-displacement);if(next<distance){out={};return false;}out.pointerAddress=next-distance;}
   else{if(next>std::numeric_limits<uint64_t>::max()-uint64_t(displacement)){out={};return false;}out.pointerAddress=next+uint64_t(displacement);}
  }else{if(address>0xFFFFFFFFull){out={};return false;}out.pointerAddress=u32(data+2);}
  return true;
 }
 if(!pe64&&size>=6&&data[0]==0x68&&data[5]==0xC3){out.kind="push-imm32-ret";out.length=6;out.target=u32(data+1);return true;}
 if(!pe64&&size>=3&&data[0]==0x6A&&data[2]==0xC3){out.kind="push-imm8-ret";out.length=3;out.target=uint32_t(int32_t(int8_t(data[1])));return true;}
 if(!pe64&&size>=7&&data[0]>=0xB8&&data[0]<=0xBF&&data[5]==0xFF){
  const uint8_t reg=data[0]-0xB8;if(data[6]==uint8_t(0xE0+reg)||data[6]==uint8_t(0xD0+reg)){
   out.isCall=data[6]==uint8_t(0xD0+reg);out.kind=out.isCall?"mov-imm32-call-reg":"mov-imm32-jmp-reg";
   out.length=7;out.target=u32(data+1);return true;
  }
 }
 if(pe64&&size>=12&&(data[0]==0x48||data[0]==0x49)&&data[1]>=0xB8&&data[1]<=0xBF){
  const uint8_t reg=data[1]-0xB8;const size_t branch=(data[0]==0x49)?11:10;
  if(size>=branch+2&&((data[0]==0x48)||(data[10]==0x41))&&data[branch]==0xFF&&
     (data[branch+1]==uint8_t(0xE0+reg)||data[branch+1]==uint8_t(0xD0+reg))){
   out.isCall=data[branch+1]==uint8_t(0xD0+reg);out.kind=out.isCall?"mov-imm64-call-reg":"mov-imm64-jmp-reg";
   out.length=uint8_t(branch+2);out.target=u64(data+2);return true;
  }
 }
 return false;
}

inline bool readExports(const NormalizedImage&image,std::vector<ExportSymbol>&out,std::string&error,size_t maximum){
 using namespace pecore_detail;out.clear();error.clear();const auto&directory=image.pe.directories[0];if(!directory.rva&&!directory.size)return true;
 if(!directory.rva||directory.size<40||!span(image.bytes.size(),directory.rva,directory.size))return fail(error,"invalid-export-directory");
 const uint8_t*p=image.bytes.data()+directory.rva;const uint32_t base=u32(p+16),functions=u32(p+20),names=u32(p+24);
 const uint32_t addresses=u32(p+28),strings=u32(p+32),ordinals=u32(p+36);
 maximum=std::min<size_t>(maximum,65536);
 if(functions>maximum||names>65536||(functions&&!addresses)||(names&&(!strings||!ordinals))||!span(image.bytes.size(),addresses,uint64_t(functions)*4)||
    !span(image.bytes.size(),strings,uint64_t(names)*4)||!span(image.bytes.size(),ordinals,uint64_t(names)*2))return fail(error,"export-table-limit-or-bounds");
 std::vector<std::string> labels(functions);
 for(uint32_t i=0;i<names;++i){const uint32_t name=u32(image.bytes.data()+strings+i*4);const uint16_t index=u16(image.bytes.data()+ordinals+i*2);
  if(index>=functions)return fail(error,"export-ordinal-out-of-table");
  std::string text;
  if(!name||!stringAt(image.bytes,name,text)||text.empty())return fail(error,"invalid-export-name");
  if(labels[index].empty())labels[index]=std::move(text); // aliases share a target; one bounded hint is enough
 }
 std::vector<ExportSymbol> result;
 for(uint32_t i=0;i<functions;++i){const uint32_t rva=u32(image.bytes.data()+addresses+i*4);if(!rva)continue;
  if(rva>=image.bytes.size()||uint64_t(base)+i>0xFFFFFFFFull)return fail(error,"export-target-out-of-image");
  ExportSymbol symbol;symbol.rva=rva;symbol.ordinal=base+i;symbol.name=labels[i].empty()?"#"+std::to_string(symbol.ordinal):labels[i];
  if(rva>=directory.rva&&uint64_t(rva)<uint64_t(directory.rva)+directory.size){
   if(!stringAt(image.bytes,rva,symbol.forwarder)||symbol.forwarder.empty()||uint64_t(rva)+symbol.forwarder.size()+1>uint64_t(directory.rva)+directory.size)return fail(error,"invalid-export-forwarder");
  }result.push_back(std::move(symbol));
 }
 out=std::move(result);return true;
}

inline bool readHookDeclarations(const NormalizedImage&image,std::vector<HookDeclaration>&out,
                                 std::string&error,size_t maximum){
 using namespace pecore_detail;out.clear();error.clear();const PeSection*found=nullptr;
 for(const auto&section:image.pe.sections)if(section.name==".syhks00"){
  if(found)return fail(error,"duplicate-syringe-section");
  found=&section;
 }
 if(!found)return true;
 if(image.pe.pe64)return fail(error,"unsupported-syringe-record-width");
 maximum=std::min<size_t>(maximum,16384);const uint32_t bytes=found->mappedSize();
 if(bytes%16||bytes/16>maximum||!span(image.bytes.size(),found->rva,bytes))return fail(error,"invalid-syringe-section-size");
 std::vector<HookDeclaration> result;
 for(uint32_t offset=0;offset<bytes;offset+=16){const auto*p=image.bytes.data()+found->rva+offset;
  const uint32_t address=u32(p),count=u32(p+4),pointer=u32(p+8);if(!address&&!count&&!pointer&&!u32(p+12))continue;
  uint32_t nameRva=0;HookDeclaration declaration;declaration.address=address;declaration.bytes=count;
  // A zero declared length is valid metadata. It is not a zero-write proof;
  // actual installation and overwritten bytes must be established separately.
  if(!address||count>4096||!vaToRva(image,pointer,nameRva)||!stringAt(image.bytes,nameRva,declaration.name)||declaration.name.empty())return fail(error,"invalid-syringe-declaration");
  result.push_back(std::move(declaration));
 }out=std::move(result);return true;
}
}
