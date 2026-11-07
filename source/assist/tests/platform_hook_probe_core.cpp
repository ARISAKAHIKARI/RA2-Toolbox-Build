// Synthetic PE fixtures execute the production parser/comparator directly.
// No Windows process, game, or platform module is executed by this test.
#include "../../shared/source/platform_hook_probe_core.h"
#include <functional>
#include <iostream>
#include <stdexcept>

using namespace hookprobe;
namespace {
size_t checks=0,scenarios=0;
#define CHECK(condition) do { ++checks; if(!(condition)) throw std::runtime_error(std::string(__func__)+":"+std::to_string(__LINE__)+": " #condition); } while(false)

struct Fixture {
 bool pe64; uint64_t base; std::vector<uint8_t> file;
 static constexpr uint32_t nt=0x80,optional=nt+24;
 explicit Fixture(bool wide=false):pe64(wide),base(wide?0x140000000ull:0x400000ull),file(0x1000,0){
  w16(0,0x5A4D);w32(60,nt);w32(nt,0x4550);w16(nt+4,wide?0x8664:0x14C);
  w16(nt+6,3);w32(nt+8,0x65AB1234);w16(nt+20,wide?240:224);w16(nt+22,wide?0x22:0x102);
  w16(optional,wide?0x20B:0x10B);w32(optional+16,0x1000);
  if(wide)w64(optional+24,base);else w32(optional+28,uint32_t(base));
  w32(optional+32,0x1000);w32(optional+36,0x200);w32(optional+56,0x4000);w32(optional+60,0x400);
  w32(optional+(wide?108:92),16);
  section(0,".text",0x1000,0x400,0x400,0x400,0x60000020);
  section(1,".data",0x2000,0x400,0x800,0x400,0xC0000040);
  section(2,".reloc",0x3000,0x400,0xC00,0x400,0x42000040);
  std::fill(file.begin()+0x400,file.begin()+0x800,uint8_t(0x90));
 }
 uint32_t table()const{return optional+(pe64?240:224);}
 void w16(size_t at,uint16_t value){CHECK(at+2<=file.size());file[at]=uint8_t(value);file[at+1]=uint8_t(value>>8);}
 void w32(size_t at,uint32_t value){CHECK(at+4<=file.size());for(unsigned i=0;i<4;++i)file[at+i]=uint8_t(value>>(8*i));}
 void w64(size_t at,uint64_t value){CHECK(at+8<=file.size());for(unsigned i=0;i<8;++i)file[at+i]=uint8_t(value>>(8*i));}
 void section(unsigned index,const char*name,uint32_t rva,uint32_t virtualSize,uint32_t raw,uint32_t rawSize,uint32_t flags){
  const uint32_t at=table()+index*40;CHECK(at+40<=0x400);std::fill(file.begin()+at,file.begin()+at+40,uint8_t(0));
  std::memcpy(file.data()+at,name,std::min<size_t>(std::strlen(name),8));
  w32(at+8,virtualSize);w32(at+12,rva);w32(at+16,rawSize);w32(at+20,raw);w32(at+36,flags);
 }
 void directory(unsigned index,uint32_t rva,uint32_t bytes){const uint32_t at=optional+(pe64?112:96)+index*8;w32(at,rva);w32(at+4,bytes);}
 uint32_t raw(uint32_t rva)const{
  const unsigned count=file[nt+6]|unsigned(file[nt+7])<<8;
  for(unsigned i=0;i<count;++i){const auto*p=file.data()+table()+i*40;
   const uint32_t start=pecore_detail::u32(p+12),size=pecore_detail::u32(p+16);
   if(rva>=start&&uint64_t(rva)-start<size)return pecore_detail::u32(p+20)+rva-start;
  }throw std::runtime_error("fixture RVA has no raw byte");
 }
 void r16(uint32_t at,uint16_t value){w16(raw(at),value);}
 void r32(uint32_t at,uint32_t value){w32(raw(at),value);}
 void r64(uint32_t at,uint64_t value){w64(raw(at),value);}
 void string(uint32_t at,const char*value){const uint32_t offset=raw(at);const size_t bytes=std::strlen(value)+1;CHECK(offset+bytes<=file.size());std::memcpy(file.data()+offset,value,bytes);}
 void reloc(uint32_t page,std::initializer_list<uint16_t> entries){
  r32(0x3000,page);r32(0x3004,uint32_t(8+entries.size()*2));unsigned i=0;
  for(uint16_t entry:entries)r16(0x3008+2*i++,entry);
  directory(5,0x3000,uint32_t(8+entries.size()*2));
 }
 PeImage parse()const{PeImage image;std::string error;CHECK(parsePe(file.data(),file.size(),image,error));CHECK(error.empty());return image;}
 NormalizedImage normalized(uint64_t actual=0)const{NormalizedImage image;std::string error;CHECK(normalizeImage(file,parse(),actual?actual:base,image,error));CHECK(error.empty());return image;}
};

void badParse(const Fixture&f,const std::string&code){PeImage out;out.sections.push_back({});std::string error;CHECK(!parsePe(f.file.data(),f.file.size(),out,error));CHECK(error==code);CHECK(out.sections.empty());}
void badNormalize(const Fixture&f,uint64_t base,const std::string&code){NormalizedImage out;out.bytes.push_back(1);std::string error;CHECK(!normalizeImage(f.file,f.parse(),base,out,error));CHECK(error==code);CHECK(out.bytes.empty());}
uint32_t load32(const std::vector<uint8_t>&bytes,size_t at){CHECK(at+4<=bytes.size());return pecore_detail::u32(bytes.data()+at);}
uint64_t load64(const std::vector<uint8_t>&bytes,size_t at){CHECK(at+8<=bytes.size());return pecore_detail::u64(bytes.data()+at);}
void scenario(const char*name,const std::function<void()>&run){try{run();++scenarios;}catch(const std::exception&e){throw std::runtime_error(std::string(name)+": "+e.what());}}

void mapping(){
 Fixture f;f.w32(f.table()+8,0x180);f.w32(f.table()+40+8,0x600);
 const auto image=f.normalized();CHECK(image.bytes[0x1000]==0x90);CHECK(image.bytes[0x117F]==0x90);
 CHECK(image.bytes[0x1180]==0);CHECK(image.bytes[0x2400]==0);CHECK(image.bytes[0x25FF]==0);
 CHECK(image.pe.sections[0].mappedSize()==0x180);CHECK(image.pe.sections[0].rawOffset!=image.pe.sections[0].rva);
 std::string error;PeImage live;CHECK(parsePe(f.file.data(),0x200,live,error,false));CHECK(compatibleLayout(image.pe,live,error));
 CHECK(!parsePe(f.file.data(),0x200,live,error,true));CHECK(error=="headers-beyond-file");
 DiffResult diff;CHECK(diffExecutableChunk(image,0x1000,image.bytes.data()+0x1000,0x180,nullptr,diff,error));CHECK(diff.changedBytes==0&&diff.comparedBytes==0x180);
 CHECK(!diffExecutableChunk(image,0x1000,image.bytes.data()+0x1000,0x181,nullptr,diff,error));CHECK(error=="comparison-chunk-not-executable");
 Fixture zeroVirtual;zeroVirtual.w32(zeroVirtual.table()+8,0);CHECK(zeroVirtual.normalized().pe.sections[0].mappedSize()==0x400);
}

void malformed(){
 Fixture f;f.file.resize(63);badParse(f,"missing-dos-header");
 f=Fixture();f.w32(60,0xFFFFFFF0);badParse(f,"invalid-nt-header");
 f=Fixture();f.w32(Fixture::nt,0x12345678);badParse(f,"invalid-nt-header");
 f=Fixture();f.w16(Fixture::nt+6,97);badParse(f,"invalid-pe-header-size");
 f=Fixture();f.w16(Fixture::optional,0x999);badParse(f,"unsupported-optional-header");
 f=Fixture();f.w16(Fixture::nt+4,0xAA64);badParse(f,"unsupported-machine");
 f=Fixture();f.w32(Fixture::optional+56,MaxImageBytes+1);badParse(f,"invalid-image-layout");
 f=Fixture();f.w32(Fixture::optional+32,0x1234);badParse(f,"invalid-image-layout");
 f=Fixture();f.w32(Fixture::optional+92,17);badParse(f,"invalid-data-directory-count");
 f=Fixture();f.directory(1,0xFFFFFFF0,0x40);badParse(f,"directory-out-of-image");
 f=Fixture();f.w32(Fixture::optional+60,0x180);badParse(f,"section-table-out-of-headers");
 f=Fixture();f.w32(f.table()+12,0xFFFFFFF0);badParse(f,"section-out-of-image");
 f=Fixture();f.file.pop_back();badParse(f,"section-raw-data-out-of-file");
 f=Fixture();f.w32(f.table()+40+12,0x1100);badParse(f,"overlapping-virtual-sections");
 f=Fixture();f.w32(f.table()+40+20,0x600);badParse(f,"overlapping-raw-sections");
 f=Fixture();f.directory(4,0x5000,0x20);badParse(f,"certificate-beyond-file");
 f.file.resize(0x5020);CHECK(f.parse().directories[4].rva==0x5000); // Certificates use file offsets, not RVAs.
}

void versionMismatch(){
 const auto disk=Fixture().parse();std::string error;
 auto live=disk;live.timestamp++;CHECK(!compatibleLayout(disk,live,error)&&error=="baseline-timestamp-mismatch");
 live=disk;live.machine=0x8664;CHECK(!compatibleLayout(disk,live,error)&&error=="baseline-machine-mismatch");
 live=disk;live.imageSize+=0x1000;CHECK(!compatibleLayout(disk,live,error));
 live=disk;live.entryRva++;CHECK(!compatibleLayout(disk,live,error));
 live=disk;live.sections[0].virtualSize--;CHECK(!compatibleLayout(disk,live,error)&&error=="baseline-section-layout-mismatch");
 live=disk;live.sections[0].rawOffset+=0x200;CHECK(!compatibleLayout(disk,live,error));
 live=disk;live.directories[1]={0x2000,40};CHECK(!compatibleLayout(disk,live,error)&&error=="baseline-directory-layout-mismatch");
 Fixture f;NormalizedImage out;auto stale=disk;stale.timestamp++;CHECK(!normalizeImage(f.file,stale,f.base,out,error));CHECK(out.bytes.empty());
 // Identical layout does not prove original content: a disk code edit still passes layout only.
 f.file[f.raw(0x1000)]=0xCC;CHECK(compatibleLayout(disk,f.parse(),error));
}

void x86Relocations(){
 Fixture f;f.r32(0x1010,0x402345);f.r16(0x1020,0x40);f.r16(0x1022,0x1234);f.r16(0x1024,0x40);
 f.reloc(0x1000,{0x3010,0x1020,0x2022,0x4024,0xF123,0});
 const uint64_t actual=0x523456;const auto image=f.normalized(actual);CHECK(image.relocations==4);
 CHECK(load32(image.bytes,0x1010)==0x52579B);CHECK(pecore_detail::u16(image.bytes.data()+0x1020)==0x52);
 CHECK(pecore_detail::u16(image.bytes.data()+0x1022)==0x468A);
 const int64_t highadj=(int64_t(0x40)<<16)+int16_t(0xF123)+int64_t(actual-f.base)+0x8000;
 CHECK(pecore_detail::u16(image.bytes.data()+0x1024)==uint16_t(uint64_t(highadj)>>16));
 auto live=image.bytes;DiffResult diff;std::string error;
 CHECK(diffExecutableChunk(image,0x1000,live.data()+0x1000,0x400,nullptr,diff,error));CHECK(diff.changedBytes==0);
 auto preferred=f.normalized();DiffResult unnormalized;
 CHECK(diffExecutableChunk(preferred,0x1000,live.data()+0x1000,0x400,nullptr,unnormalized,error));CHECK(unnormalized.changedBytes>0);
 Fixture down;down.r32(0x1010,0x401234);down.reloc(0x1000,{0x3010,0});CHECK(load32(down.normalized(0x300000).bytes,0x1010)==0x301234);
}

void x64Relocations(){
 Fixture f(true);f.r64(0x1018,f.base+0x2100);f.reloc(0x1000,{0xA018,0});
 const auto image=f.normalized(f.base+0x123450000ull);CHECK(image.relocations==1);
 CHECK(load64(image.bytes,0x1018)==f.base+0x123452100ull);
 DiffResult diff;std::string error;CHECK(diffExecutableChunk(image,0x1000,image.bytes.data()+0x1000,0x400,nullptr,diff,error));CHECK(!diff.changedBytes);
 CHECK(load64(f.normalized(f.base-0x100000).bytes,0x1018)==f.base-0x100000+0x2100);
}

void invalidRelocations(){
 Fixture f;badNormalize(f,f.base+0x10000,"missing-relocations-on-rebased-image");
 f.w16(Fixture::nt+22,0x103);badNormalize(f,f.base+0x10000,"rebased-image-with-stripped-relocations");
 f=Fixture();f.reloc(0x1000,{0x5010,0});badNormalize(f,f.base,"unsupported-relocation-type");
 f=Fixture(true);f.reloc(0x1000,{0x3010,0});badNormalize(f,f.base,"unsupported-relocation-type");
 f=Fixture();f.reloc(0x1000,{0x4010});badNormalize(f,f.base,"truncated-highadj-relocation");
 f=Fixture();f.reloc(0x1001,{0x3010,0});badNormalize(f,f.base,"invalid-relocation-block");
 f=Fixture();f.reloc(0x1000,{0x3010,0});f.r32(0x3004,99);badNormalize(f,f.base,"invalid-relocation-block");
 f=Fixture();f.reloc(0xFFFFF000,{0x3FFE,0});badNormalize(f,f.base,"relocation-target-out-of-image");
 f=Fixture();f.reloc(0x1000,{0x3010,0x3012});badNormalize(f,f.base,"overlapping-relocation-targets");
 f=Fixture();badNormalize(f,0xFFFFFF00,"loaded-image-address-overflow");
 f=Fixture(true);badNormalize(f,std::numeric_limits<uint64_t>::max()-1,"loaded-image-address-overflow");
}

void relocationLimit(){
 Fixture f;const uint32_t bytes=8+uint32_t(MaxRelocations+1)*2;
 const uint32_t rawBytes=(bytes+0x1FF)&~0x1FFu;f.file.resize(0xC00+rawBytes,0);
 f.section(2,".reloc",0x3000,bytes,0xC00,rawBytes,0x42000040);
 f.w32(Fixture::optional+56,(0x3000+bytes+0xFFF)&~0xFFFu);f.directory(5,0x3000,bytes);
 f.r32(0x3000,0x1000);f.r32(0x3004,bytes);
 // A compact hostile directory reaches the allocation cap before overlap validation.
 for(size_t i=0;i<MaxRelocations+1;++i){f.file[0xC08+2*i]=0x10;f.file[0xC09+2*i]=0x30;}
 badNormalize(f,f.base,"relocation-count-limit");
}

void addImports(Fixture&f,bool noLookup=false){
 f.directory(1,0x2000,40);f.directory(12,0x1000,0x400); // Deliberately broad; must not hide code.
 f.r32(0x2000,noLookup?0:0x2080);f.r32(0x200C,0x20C0);f.r32(0x2010,0x1100);f.string(0x20C0,"kernel32.dll");
 if(f.pe64){f.r64(0x2080,0x8000000000000001ull);f.r64(0x2088,0);f.r64(0x1100,0x8000000000000001ull);f.r64(0x1108,0);}
 else{f.r32(0x2080,0x80000001u);f.r32(0x2084,0);f.r32(0x1100,0x80000001u);f.r32(0x1104,0);}
}

void loaderImports(){
 for(bool wide:{false,true}){Fixture f(wide);addImports(f);const auto image=f.normalized();const uint32_t width=wide?8:4;
  CHECK(image.loaderWrites.size()==1&&image.loaderWrites[0].rva==0x1100&&image.loaderWrites[0].size==width);
  auto live=image.bytes;for(uint32_t i=0;i<width;++i)live[0x1100+i]=uint8_t(0xA0+i);live[0x1010]=0xCC;live[0x1100+width]=0xCC;
  DiffResult diff;std::string error;CHECK(diffExecutableChunk(image,0x1000,live.data()+0x1000,0x400,nullptr,diff,error));
  CHECK(diff.ignoredBytes==width);CHECK(diff.changedBytes==2);CHECK(diff.ranges.size()==2);CHECK(diff.ranges[0].rva==0x1010&&diff.ranges[1].rva==0x1100+width);
 }
 Fixture fallback;addImports(fallback,true);CHECK(fallback.normalized().loaderWrites[0].size==4);
 Fixture bad;addImports(bad);bad.r32(0x2010,0x3FFF);badNormalize(bad,bad.base,"import-address-table-out-of-image");
 bad=Fixture();addImports(bad);bad.directory(1,0x2000,20);badNormalize(bad,bad.base,"unterminated-import-descriptors");
}

void delayAndTls(){
 for(bool wide:{false,true}){Fixture f(wide);const uint32_t width=wide?8:4;
  f.directory(13,0x2000,64);f.r32(0x2000,1);f.r32(0x2004,0x20C0);f.r32(0x2008,0x1120);f.r32(0x200C,0x1140);
  f.r32(0x2010,0x2080);f.r32(0x2018,0x1160);f.string(0x20C0,"optional.dll");
  if(wide){f.r64(0x2080,0x8000000000000001ull);f.r64(0x2088,0);}else{f.r32(0x2080,0x80000001);f.r32(0x2084,0);}
  f.directory(9,0x2200,wide?40:24);
  if(wide)f.r64(0x2210,f.base+0x1180);else f.r32(0x2208,uint32_t(f.base+0x1180));
  const auto image=f.normalized();CHECK(image.loaderWrites.size()==4);uint64_t masked=0;for(const auto&range:image.loaderWrites)masked+=range.size;CHECK(masked==3*width+4);
  auto live=image.bytes;for(const auto&range:image.loaderWrites)std::fill(live.begin()+range.rva,live.begin()+range.rva+range.size,uint8_t(0xA5));
  DiffResult diff;std::string error;CHECK(diffExecutableChunk(image,0x1000,live.data()+0x1000,0x400,nullptr,diff,error));CHECK(diff.changedBytes==0&&diff.ignoredBytes==masked);
  if(wide)f.r64(0x2210,f.base+f.parse().imageSize);else f.r32(0x2208,uint32_t(f.base+f.parse().imageSize));
  badNormalize(f,f.base,"tls-index-out-of-image");
 }
 Fixture legacy;legacy.directory(13,0x2000,64);legacy.r32(0x2004,uint32_t(legacy.base+0x20C0));legacy.r32(0x200C,uint32_t(legacy.base+0x1140));legacy.r32(0x2010,uint32_t(legacy.base+0x2080));
 legacy.string(0x20C0,"legacy.dll");legacy.r32(0x2080,0x80000001);legacy.r32(0x2084,0);CHECK(legacy.normalized().loaderWrites.size()==1);
 legacy.r32(0x2000,2);badNormalize(legacy,legacy.base,"unknown-delay-import-attributes");
}

void diffCoverage(){
 Fixture f;addImports(f);const auto image=f.normalized();auto live=image.bytes;
 live[0x1010]=0xCC;live[0x1011]=0xCC;live[0x1012]=0xCC;live[0x1020]=0xCC;live[0x1021]=0xCC;
 std::vector<uint8_t> valid(0x400,1);valid[0x20]=0;valid[0x21]=2;for(unsigned i=0x100;i<0x104;++i)valid[i]=2;
 DiffResult diff;std::string error;
 CHECK(diffExecutableChunk(image,0x1000,live.data()+0x1000,0x12,valid.data(),diff,error,{10,2}));
 CHECK(diffExecutableChunk(image,0x1012,live.data()+0x1012,0x3EE,valid.data()+0x12,diff,error,{10,2}));
 CHECK(diff.ranges.size()==1&&diff.ranges[0].rva==0x1010&&diff.ranges[0].length==3);
 CHECK(diff.ranges[0].actual.size()==2&&diff.ranges[0].expected.size()==2);CHECK(diff.changedBytes==3);
 CHECK(diff.ignoredBytes==4&&diff.unreadableBytes==1&&diff.unstableBytes==1&&diff.comparedBytes==0x3FA);
 CHECK(diff.comparedBytes+diff.ignoredBytes+diff.unreadableBytes+diff.unstableBytes==0x400);
 valid[0]=3;const auto count=diff.comparedBytes;CHECK(!diffExecutableChunk(image,0x1000,live.data()+0x1000,0x400,valid.data(),diff,error));CHECK(diff.comparedBytes==count);
 CHECK(!diffExecutableChunk(image,0x2000,live.data()+0x2000,8,nullptr,diff,error));CHECK(error=="comparison-chunk-not-executable");
 CHECK(!diffExecutableChunk(image,0xFFFFFFFE,live.data(),8,nullptr,diff,error));CHECK(error=="comparison-chunk-out-of-image");
 CHECK(!diffExecutableChunk(image,0x13FF,live.data()+0x13FF,2,nullptr,diff,error));
}

void diffLimits(){
 const auto image=Fixture().normalized();auto live=image.bytes;for(uint32_t at=0x1000;at<0x1010;at+=2)live[at]=0xCC;
 DiffResult diff;std::string error;CHECK(diffExecutableChunk(image,0x1000,live.data()+0x1000,0x10,nullptr,diff,error,{2,1}));
 CHECK(diff.truncated&&diff.ranges.size()==2&&diff.changedBytes==8);CHECK(diff.ranges[0].actual.size()==1);
 diff={};CHECK(diffExecutableChunk(image,0x1000,live.data()+0x1000,0x10,nullptr,diff,error,{0,0}));CHECK(diff.truncated&&diff.ranges.empty()&&diff.changedBytes==8);
 std::fill(live.begin()+0x1000,live.begin()+0x1200,uint8_t(0xCC));diff={};
 CHECK(diffExecutableChunk(image,0x1000,live.data()+0x1000,0x200,nullptr,diff,error,{999999,999999}));CHECK(diff.ranges[0].length==0x200&&diff.ranges[0].actual.size()==128);
}

void transfers(){
 Transfer out;auto decode=[&](std::initializer_list<uint8_t> data,uint64_t at,bool wide){const std::vector<uint8_t> code(data);return decodeTransfer(code.data(),code.size(),at,wide,out);};
 CHECK(decode({0xE9,0xFB,0xFF,0xFF,0xFF},0x401000,false)&&out.target==0x401000&&out.length==5&&!out.isCall);
 CHECK(decode({0xE8,0x10,0,0,0},0x401000,false)&&out.target==0x401015&&out.isCall);
 CHECK(decode({0xEB,0xFC},0x401000,false)&&out.target==0x400FFE);
 CHECK(decode({0xE9,2,0,0,0},0xFFFFFFFC,false)&&out.target==3);
 CHECK(decode({0xFF,0x25,0x78,0x56,0x34,0x12},0x401000,false)&&out.pointerAddress==0x12345678&&out.indirect&&out.target==0);
 CHECK(decode({0xFF,0x15,0xFA,0xFF,0xFF,0xFF},0x140001000,true)&&out.pointerAddress==0x140001000&&out.isCall);
 CHECK(decode({0x68,0x78,0x56,0x34,0x12,0xC3},0x401000,false)&&out.target==0x12345678);
 CHECK(decode({0x6A,0xFE,0xC3},0x401000,false)&&out.target==0xFFFFFFFE);
 CHECK(decode({0xBA,0x78,0x56,0x34,0x12,0xFF,0xE2},0x401000,false)&&out.target==0x12345678&&!out.isCall);
 CHECK(decode({0xBB,0x78,0x56,0x34,0x12,0xFF,0xD3},0x401000,false)&&out.isCall);
 CHECK(decode({0x48,0xB8,0x78,0x56,0x34,0x12,1,0,0,0,0xFF,0xE0},0x140001000,true)&&out.target==0x112345678ull&&out.length==12);
 CHECK(decode({0x49,0xBB,0x78,0x56,0x34,0x12,1,0,0,0,0x41,0xFF,0xD3},0x140001000,true)&&out.isCall&&out.length==13);
 CHECK(!decode({0x48,0xB8,0,0,0,0,0,0,0,0,0xFF,0xE1},0x140001000,true));
 CHECK(!decode({0xE9,0,0,0},0x401000,false));CHECK(!decode({0x90,0x90},0x401000,false));
 CHECK(!decode({0xEB,0x80},0,true));CHECK(!decode({0xFF,0x25,0,0,0,0},UINT64_MAX,true));
 // This pattern sits inside MOV's immediate; byte-pattern recognition is not an instruction-boundary proof.
 const uint8_t embedded[]={0xB8,0xE9,0,0,0,0x90,0x90};
 CHECK(decodeTransfer(embedded+1,sizeof(embedded)-1,0x401001,false,out));CHECK(!out.instructionBoundaryVerified);
}

void exports(){
 Fixture f;f.directory(0,0x2000,0x100);f.r32(0x2010,7);f.r32(0x2014,2);f.r32(0x2018,3); // Three aliases for two functions.
 f.r32(0x201C,0x2040);f.r32(0x2020,0x2050);f.r32(0x2024,0x2060);
 f.r32(0x2040,0x1010);f.r32(0x2044,0x2090);f.string(0x2090,"KERNEL32.Sleep");
 f.r32(0x2050,0x20B0);f.r32(0x2054,0x20C0);f.r32(0x2058,0x20D0);f.string(0x20B0,"Entry");f.string(0x20C0,"Alias");f.string(0x20D0,"Forward");
 f.r16(0x2060,0);f.r16(0x2062,0);f.r16(0x2064,1);
 std::vector<ExportSymbol> out;std::string error;CHECK(readExports(f.normalized(),out,error));CHECK(out.size()==2);
 CHECK(out[0].ordinal==7&&out[0].name=="Entry"&&out[0].forwarder.empty());CHECK(out[1].forwarder=="KERNEL32.Sleep");
 CHECK(!readExports(f.normalized(),out,error,1)&&out.empty());CHECK(error=="export-table-limit-or-bounds");
 f.r16(0x2064,2);CHECK(!readExports(f.normalized(),out,error)&&error=="export-ordinal-out-of-table");
 f.r16(0x2064,1);f.r32(0x2044,0x20FE);f.file[f.raw(0x20FE)]='X';f.file[f.raw(0x20FF)]='Y';
 CHECK(!readExports(f.normalized(),out,error)&&error=="invalid-export-forwarder");
}

void hookDeclarations(){
 Fixture f;f.w16(Fixture::nt+6,4);f.file.resize(0x1200,0);f.w32(Fixture::optional+56,0x5000);
 f.section(3,".syhks00",0x4000,32,0x1000,0x200,0x40000040);f.r32(0x4000,0x6F1234);f.r32(0x4004,5);f.r32(0x4008,uint32_t(f.base+0x2100));f.string(0x2100,"CandidateEntry");
 f.reloc(0x4000,{0x3008,0});const auto image=f.normalized(0x500000);
 std::vector<HookDeclaration> out;std::string error;CHECK(readHookDeclarations(image,out,error));CHECK(out.size()==1&&out[0].address==0x6F1234&&out[0].bytes==5&&out[0].name=="CandidateEntry");
 auto zeroLength=image;pecore_detail::put32(zeroLength.bytes.data()+0x4004,0);CHECK(readHookDeclarations(zeroLength,out,error)&&out.size()==1&&out[0].bytes==0);
 auto bad=image;bad.pe.sections.back().virtualSize=31;CHECK(!readHookDeclarations(bad,out,error)&&error=="invalid-syringe-section-size");
 bad=image;bad.pe.pe64=true;CHECK(!readHookDeclarations(bad,out,error)&&error=="unsupported-syringe-record-width");
 bad=image;pecore_detail::put32(bad.bytes.data()+0x4008,0x1234);CHECK(!readHookDeclarations(bad,out,error)&&error=="invalid-syringe-declaration");
 CHECK(readHookDeclarations(Fixture().normalized(),out,error)&&out.empty());
}
}

int main(){try{
 scenario("RVA mapping, virtual extent, and memory headers",mapping);
 scenario("Malformed PE bounds and architecture",malformed);
 scenario("Structural baseline matching, not authenticity",versionMismatch);
 scenario("x86 HIGH, LOW, HIGHLOW, HIGHADJ normalization",x86Relocations);
 scenario("x64 DIR64 normalization",x64Relocations);
 scenario("Unsupported, missing, malformed, and overlapping relocations",invalidRelocations);
 scenario("Bounded hostile relocation count",relocationLimit);
 scenario("Precise IAT exclusions and zero terminator evidence",loaderImports);
 scenario("Delay imports and TLS loader writes",delayAndTls);
 scenario("Stable, unreadable, unstable, ignored and cross-chunk diff coverage",diffCoverage);
 scenario("Diff output and sample bounds",diffLimits);
 scenario("Transfer candidates, truncation, target bounds, and unknown instruction boundaries",transfers);
 scenario("Export aliases and forwarders",exports);
 scenario("Relocated Syringe declarations are metadata only",hookDeclarations);
 std::cout<<"{\"passed\":true,\"scenarios\":"<<scenarios<<",\"checks\":"<<checks<<"}\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 1;}}
