// Executes the production collector through a deterministic read-only OS boundary.
// Synthetic PE fixtures are not real game/platform or Windows execution evidence.
#include "../../shared/source/platform_hook_probe_windows.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <map>
using namespace hookprobe;
static void u16(std::vector<uint8_t>&v,size_t p,uint16_t n){v[p]=uint8_t(n);v[p+1]=uint8_t(n>>8);}
static void u32(std::vector<uint8_t>&v,size_t p,uint32_t n){for(unsigned i=0;i<4;++i)v[p+i]=uint8_t(n>>(i*8));}
static constexpr uint32_t Text=0xF7000,Site=0xF7870,Size=0x2000,Image=Text+Size;
struct Fixture {
 ModuleSnapshot module;std::vector<uint8_t>file,memory;bool missingFile=false;
 Fixture(uint64_t base=0x400000,const wchar_t*path=L"C:\\Game\\gamemd-spawn.exe",bool main=true){
  module={path,main?"gamemd-spawn.exe":"other.dll",base,Image,main,false};file.assign(0x200+Size,0);file[0]='M';file[1]='Z';u32(file,0x3c,0x80);u32(file,0x80,0x4550);u16(file,0x84,0x14c);u16(file,0x86,1);u32(file,0x88,0x12345678);u16(file,0x94,224);u16(file,0x96,0x102);u16(file,0x98,0x10b);u32(file,0xa8,Text);u32(file,0xb4,uint32_t(base));u32(file,0xb8,0x1000);u32(file,0xbc,0x200);u32(file,0xd0,Image);u32(file,0xd4,0x200);u32(file,0xf4,16);
  std::memcpy(file.data()+0x178,".text",5);u32(file,0x180,Size);u32(file,0x184,Text);u32(file,0x188,Size);u32(file,0x18c,0x200);u32(file,0x19c,0x60000020);std::fill(file.begin()+0x200,file.end(),0x90);rebuild();
 }
 void rebuild(){PeImage pe;NormalizedImage image;std::string e;assert(parsePe(file.data(),file.size(),pe,e));assert(normalizeImage(file,pe,module.base,image,e));memory=image.bytes;}
 void jump(uint32_t from,uint64_t target){memory[from]=0xE9;u32(memory,from+1,uint32_t(target-(module.base+from+5)));}
 void imports(){auto raw=[](uint32_t r){return size_t(0x200+r-Text);};const uint32_t desc=Text+0x100,lookup=Text+0x200,iat=Text+0x220,name=Text+0x240;
  u32(file,0x100,desc);u32(file,0x104,40);std::fill(file.begin()+raw(desc),file.begin()+raw(desc)+40,0);u32(file,raw(desc),lookup);u32(file,raw(desc)+12,name);u32(file,raw(desc)+16,iat);u32(file,raw(lookup),0x80000001);u32(file,raw(lookup)+4,0);u32(file,raw(iat),0x11111111);u32(file,raw(iat)+4,0);std::memcpy(file.data()+raw(name),"fake.dll",9);rebuild();
 }
};
struct FakeIo final:ProbeIo {
 std::vector<Fixture>fixtures;Progress*progress=nullptr;uint64_t clock=100;uint32_t snapshots=0,codeReads=0;
 bool live=true,changedList=false,fileChanged=false,failEnumeration=false,swapCandidate=false,exitDuringRead=false,omitSecondModule=false,missingCreation=false;
 uint32_t unreadRva=0,dynamicRva=0;uint64_t readTime=0;std::wstring lastFile;
 FakeIo(){fixtures.emplace_back();}
 uint64_t nowMs()override{return clock;}
 std::string utc()override{return "2026-11-07T00:00:00Z";}
 std::string creationTime()override{return missingCreation?"":"0xC0FFEE";}
 bool alive()override{return live;}
 bool snapshot(std::vector<ModuleSnapshot>&out,std::string&e)override{++snapshots;out.clear();for(size_t i=0;i<fixtures.size();++i)if(!omitSecondModule||i==0)out.push_back(fixtures[i].module);if(changedList&&snapshots>1)out[0].path=L"C:\\replacement.exe";if(failEnumeration)e="synthetic enumeration incomplete";return !failEnumeration;}
 bool query(uint64_t a,MemoryRegion&m)override{for(const auto&f:fixtures)if(probeRange(a,1,f.module.base,f.module.size)){m={f.module.base,f.module.size,f.module.base,true,true,true};if(unreadRva&&probeRange(a,1,f.module.base+unreadRva,4096)){m.base=f.module.base+unreadRva;m.size=4096;m.readable=false;}return true;}return false;}
 size_t read(uint64_t a,uint8_t*out,size_t n)override{for(auto&f:fixtures)if(probeRange(a,n,f.module.base,f.module.size)){uint32_t rva=uint32_t(a-f.module.base);
   if(n>=4096&&rva>=Text){++codeReads;clock+=readTime;if(dynamicRva&&probeRange(dynamicRva,1,rva,n))f.memory[dynamicRva]=uint8_t(codeReads&1?0x41:0x42);if(exitDuringRead)live=false;}
   if(n==16&&rva==Site&&swapCandidate)f.jump(Site,f.module.base+Text+0x100);
   std::memcpy(out,f.memory.data()+rva,n);return n;
  }return 0;}
 bool readFile(const std::wstring&path,size_t maximum,std::vector<uint8_t>&out,FileIdentity&s,std::string&e)override{lastFile=path;for(const auto&f:fixtures)if(path==f.module.path||path==L"C:\\clean.exe"){if(f.missingFile){e="synthetic missing baseline";return false;}if(f.file.size()>maximum){e="synthetic file budget";return false;}out=f.file;s.path=path;s.identity="fixture-identity";s.bytes=out.size();s.readRequestedBytes=out.size();s.writeTime=1;s.readStartedMs=clock;s.readFinishedMs=clock;s.unchangedAfterRead=!fileChanged;return true;}e="no fixture file";return false;}
 bool fileUnchanged(const FileIdentity&)override{return !fileChanged;}
};
static ProbeReport runFake(FakeIo&io,Progress&p,const std::wstring&baseline={}){Options o;o.pid=4242;o.baselineExe=baseline;return collect(io,o,p);}
static void pass(const char*s){std::cout<<"PASS "<<s<<'\n';}
int main(int argc,char**argv){
 {ProbeSha256 h;assert(h.finish()=="e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");ProbeSha256 a;const std::string abc="abc";a.update(reinterpret_cast<const uint8_t*>(abc.data()),abc.size());assert(a.finish()=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");const std::string x="abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";ProbeSha256 h2;for(size_t i=0;i<x.size();i+=7)h2.update(reinterpret_cast<const uint8_t*>(x.data()+i),(std::min)(size_t(7),x.size()-i));assert(h2.finish()=="248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");ProbeSha256 million;std::string block(1000,'a');for(unsigned i=0;i<1000;++i)million.update(reinterpret_cast<const uint8_t*>(block.data()),block.size());assert(million.finish()=="cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");pass("SHA256 empty/abc/padding-boundary/million-a chunked vectors");}
 {FakeIo io;Progress p;auto r=runFake(io,p);assert(!r.partial&&r.differences.empty()&&r.modules[0].comparedBytes==Size&&r.moduleListStable);assert(r.modules[0].file.sha256==sha256(io.fixtures[0].file));assert(r.references.size()==161);assert(r.referenceRangeCount>161);pass("same loaded image compared without uninjected runtime; hashes and 161 references");}
 ProbeReport example;
 {FakeIo io;io.fixtures[0].jump(Site,0x400000+Site);Progress p;auto r=runFake(io,p);assert(!r.partial&&!r.differences.empty());bool cycle=false,intersection=false;for(const auto&d:r.differences){cycle=cycle||d.chainStatus=="cycle";intersection=intersection||!d.referenceIntersections.empty();}assert(cycle&&intersection);example=r;pass("real collector finds transfer candidate, bounded cycle and actual reference intersections");}
 {FakeIo io;io.unreadRva=Text;Progress p;auto r=runFake(io,p);assert(r.partial&&r.modules[0].unreadableBytes==4096&&r.differences.empty());assert(r.modules[0].status=="partial");pass("unreadable executable page remains unknown, never zero-filled differences or clean");}
 {FakeIo io;io.dynamicRva=Site;Progress p;auto r=runFake(io,p);assert(r.partial&&r.modules[0].dynamicBytes==1);assert(r.differences.size()==1&&r.differences[0].type=="dynamic-sample");pass("two reads differ: dynamic sample excluded from stable comparison");}
 {FakeIo io;io.fixtures[0].imports();io.dynamicRva=Text+0x220;Progress p;auto r=runFake(io,p);assert(!r.partial&&r.differences.empty()&&r.modules[0].ignoredBytes==4&&r.modules[0].dynamicBytes==0);pass("loader IAT dynamic bytes use same mask in core and collector, no false hook sample");}
 {FakeIo io;io.fixtures[0].jump(Site,0x400000+Site);io.swapCandidate=true;Progress p;auto r=runFake(io,p);assert(r.partial);bool changed=false;for(const auto&d:r.differences)changed=changed||d.chainStatus=="changed-since-scan";assert(changed);pass("candidate changed after scan: old difference cannot inherit new stable jump chain");}
 {FakeIo io;io.fixtures.emplace_back(0x10000000,L"C:\\unknown.dll",false);io.fixtures[1].missingFile=true;io.fixtures[0].jump(Site,0x10000000+Site);Progress p;auto r=runFake(io,p);assert(r.partial);bool unknown=false;for(const auto&d:r.differences)unknown=unknown||d.chainStatus=="target-architecture-unverified";assert(unknown);pass("unknown target-module architecture stops chain instead of defaulting to x86");}
 {FakeIo io;u32(io.fixtures[0].file,0x88,0xbad);Progress p;auto r=runFake(io,p);assert(r.partial&&!r.modules[0].structuralMatch&&r.modules[0].comparedBytes==0&&r.modules[0].status=="baseline-memory-identity-mismatch");pass("same filename with different PE identity is not compared");}
 {FakeIo io;Progress p;auto r=runFake(io,p,L"C:\\clean.exe");assert(!r.partial&&io.lastFile==L"C:\\clean.exe"&&r.modules[0].baselineSource=="user-supplied-main-exe");pass("explicit clean main EXE baseline works without another process snapshot");}
 {FakeIo io;io.fileChanged=true;Progress p;auto r=runFake(io,p);assert(r.partial&&r.modules[0].status=="baseline-changed-during-read"&&r.differences.empty());pass("changed baseline file rejected before code comparison");}
 {FakeIo io;io.changedList=true;Progress p;auto r=runFake(io,p);assert(r.partial&&!r.moduleListStable);pass("module list replacement is partial observation");}
 {FakeIo io;io.exitDuringRead=true;Progress p;auto r=runFake(io,p);assert(r.partial&&!r.processAliveAtEnd);pass("process exit during read does not switch PID or claim clean");}
 {FakeIo io;Progress p;p.cancel=true;auto r=runFake(io,p);assert(r.partial&&r.cancelled&&r.modules[0].status=="unscanned-cancelled");pass("pre-cancel produces explicit unscanned module and no target reads");}
 {FakeIo io;io.readTime=19000;io.fixtures[0].jump(Site,0x400000+Site);Progress p;auto r=runFake(io,p);assert(r.partial&&r.elapsedMs<ProbeDeadlineMs&&r.modules[0].unscannedBytes==4096);bool chain=false;for(const auto&d:r.differences)chain=chain||d.chainStatus=="cycle";assert(chain&&r.moduleListStable);pass("scan phase reserves final time for existing candidates and module identity snapshot");}
 {FakeIo io;io.readTime=23000;Progress p;auto r=runFake(io,p);assert(r.partial&&r.elapsedMs>=ProbeDeadlineMs);pass("overall cooperative deadline marks coverage partial and terminates bounded loops");}
 {FakeIo io;io.failEnumeration=true;Progress p;auto r=runFake(io,p);assert(r.partial&&!r.enumerationComplete);pass("incomplete enumeration retains known modules and unknown coverage");}
 {Progress p;FakeIo io;ProbeBudget b{io,p,io.nowMs(),ProbeMaxScanBytes,{},{}};assert(!b.scan(1)&&b.scanReason=="scan-byte-budget-exceeded"&&!b.stop());pass("global byte cap stops scan while preserving chain deadline budget");}
 {ProbeReport r;auto w=referenceWindows(r);assert(r.references.size()==161);r.modules.push_back({});r.modules[0].main=true;r.modules[0].base=0x400000;r.modules[0].preferredBase=0x400000;r.modules[0].structuralMatch=true;Difference d;d.module=0;d.rva=0;d.length=0x800000;r.differences.push_back(d);annotateReferences(r,w);assert(r.differences[0].referenceIntersections.size()==8);pass("reference annotation output capped while every entry counter remains available");}
 {const std::string attack="</script><img src=x onerror=alert(1)>\"&'\n";assert(jsonString(attack).find("\\u000a")!=std::string::npos);assert(htmlText(attack).find("<img")==std::string::npos);assert(validUtf8(std::string("\xff\xc0",2))=="\xef\xbf\xbd\xef\xbf\xbd");assert(jsonString(std::string(3000,'a')).size()<2100);example.modules[0].name=attack;example.notes.push_back(attack);if(argc>1){std::ofstream(std::string(argv[1])+"/collector-fixture-report.json",std::ios::binary)<<reportJson(example);std::ofstream(std::string(argv[1])+"/collector-fixture-report.html",std::ios::binary)<<reportHtml(example);}pass("report JSON/HTML escaping, malformed UTF8 and text bound");}
 {FakeIo io;Progress p;ProbeBudget b{io,p,io.nowMs(),0,{},{}};b.remoteReadBytes=ProbeMaxRemoteReadBytes-4;std::array<uint8_t,8>out{},valid{};assert(readObserved(io,b,0x400000+Site,out.data(),out.size(),valid.data())==0);assert(b.reason=="remote-read-byte-budget-exceeded");pass("all remote reads including metadata share a hard request-byte cap");}
 {FakeIo io;io.fixtures.emplace_back(0x10000000,L"C:\\unlisted.dll",false);io.omitSecondModule=true;io.fixtures[0].jump(Site,0x10000000+Site);Progress p;auto r=runFake(io,p);assert(r.partial);bool unknown=false;for(const auto&d:r.differences)unknown=unknown||(d.chainStatus=="target-architecture-unverified"&&!d.chain.empty()&&d.chain[0].targetModule=="image-memory-not-in-module-snapshot");assert(unknown);pass("unlisted MEM_IMAGE architecture stops tracing instead of inheriting source mode");}
 {FakeIo io;io.missingCreation=true;Progress p;auto r=runFake(io,p);assert(r.partial&&r.error.find("creation time unavailable")!=std::string::npos);pass("missing process creation timestamp is explicitly partial identity evidence");}
 {FakeIo io;io.fixtures[0].imports();auto&m=io.fixtures[0].memory;m[Text+0x220]=0xE9;m[Text+0x221]=0;m[Text+0x222]=0;m[Text+0x223]=0;m[Text+0x224]=1;Progress p;auto r=runFake(io,p);assert(!r.partial&&r.differences.size()==1&&r.differences[0].candidateAddress==0&&r.differences[0].type=="code-bytes");pass("candidate backwards search cannot use loader-excluded bytes as a jump instruction");}
 std::cout<<"PASS 24 production collector/report cases; synthetic data only; no Windows/game execution claimed\n";
}
