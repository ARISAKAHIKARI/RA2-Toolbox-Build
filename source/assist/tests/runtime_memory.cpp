// Calls extracted production wrappers in separate translation units. Only
// their public symbols are renamed so the fixture keeps the system CRT.
#include <algorithm>
#include <chrono>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <sys/mman.h>
#include <unistd.h>

extern "C" void* subject_copy(void*,const void*,size_t);
extern "C" void* subject_fill(void*,int,size_t);
#ifdef RUNTIME_MEMORY_BENCHMARK
extern "C" void* baseline_copy(void*,const void*,size_t);
extern "C" void* baseline_fill(void*,int,size_t);
#endif
static uint64_t checks=0,copyCases=0,fillCases=0,guardCases=0;
static void check(bool passed,const char*label){++checks;if(!passed){std::fprintf(stderr,"FAIL: %s\n",label);std::exit(1);}}
static unsigned char pattern(size_t i){return static_cast<unsigned char>((i*37+(i>>8)*13)^0x5a);}
static constexpr size_t Alignments=16,Padding=32;
static constexpr int Values[]={0,1,255,256,-1,-257,0x12345678,INT_MIN,INT_MAX};

static void matrix(size_t size){
 const size_t total=size+Padding*2+Alignments;
 std::vector<unsigned char>source(total),original(total),output(total),expected(total);
 for(size_t i=0;i<total;++i)original[i]=pattern(i);
 source=original;
 for(size_t sourceOffset=0;sourceOffset<Alignments;++sourceOffset){
  for(size_t destinationOffset=0;destinationOffset<Alignments;++destinationOffset){
   std::fill(output.begin(),output.end(),0xa7);std::fill(expected.begin(),expected.end(),0xa7);
   auto*d=output.data()+Padding+destinationOffset;
   const auto*s=source.data()+Padding+sourceOffset;
   std::copy_n(original.data()+Padding+sourceOffset,size,expected.data()+Padding+destinationOffset);
   check(subject_copy(d,s,size)==d,"copy return pointer");
   check(std::memcmp(output.data(),expected.data(),total)==0,"copy bytes and both canaries");++copyCases;
  }
 }
 check(source==original,"source remains unchanged");
 for(size_t offset=0;offset<Alignments;++offset)for(int value:Values){
  std::fill(output.begin(),output.end(),0xa7);std::fill(expected.begin(),expected.end(),0xa7);
  auto*d=output.data()+Padding+offset;
  std::fill_n(expected.data()+Padding+offset,size,static_cast<unsigned char>(value));
  check(subject_fill(d,value,size)==d,"fill return pointer");
  check(std::memcmp(output.data(),expected.data(),total)==0,"fill truncation and both canaries");++fillCases;
 }
}

struct Guarded {
 size_t page=0,capacity=0,mappingSize=0;unsigned char*mapping=nullptr,*data=nullptr;
 explicit Guarded(size_t wanted){
  page=size_t(sysconf(_SC_PAGESIZE));check(page&&!(page&(page-1)),"native page size");
  capacity=((std::max(wanted,size_t(1))+page-1)/page)*page;mappingSize=capacity+2*page;
  mapping=static_cast<unsigned char*>(mmap(nullptr,mappingSize,PROT_NONE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0));
  check(mapping!=MAP_FAILED,"guard mapping");data=mapping+page;
  check(mprotect(data,capacity,PROT_READ|PROT_WRITE)==0,"writable middle pages");
 }
 ~Guarded(){check(munmap(mapping,mappingSize)==0,"unmap guarded pages");}
};
static void guards(size_t size){
 Guarded source(size+Alignments),destination(size+Alignments);
 for(size_t i=0;i<source.capacity;++i)source.data[i]=pattern(i);
 check(mprotect(source.data,source.capacity,PROT_READ)==0,"read-only source pages");
 std::vector<unsigned char>expected(destination.capacity);
 // Every offset modulo 16 at both mapping edges. Offset zero puts the end
 // exactly against an inaccessible page, catching widened tail accesses.
 for(unsigned edges=0;edges<4;++edges)for(size_t offset=0;offset<Alignments;++offset){
  const size_t sourceOffset=(edges&1)?source.capacity-size-offset:offset;
  const size_t destinationOffset=(edges&2)?destination.capacity-size-offset:offset;
  auto*d=destination.data+destinationOffset;const auto*s=source.data+sourceOffset;
  std::memset(destination.data,0xa7,destination.capacity);std::fill(expected.begin(),expected.end(),0xa7);
  std::copy_n(s,size,expected.data()+destinationOffset);
  check(subject_copy(d,s,size)==d,"guarded copy return");
  check(std::memcmp(destination.data,expected.data(),destination.capacity)==0,"guarded copy exact bytes");++guardCases;
  std::memset(destination.data,0xa7,destination.capacity);std::fill(expected.begin(),expected.end(),0xa7);
  const int value=Values[offset%(sizeof Values/sizeof Values[0])];
  std::fill_n(expected.data()+destinationOffset,size,static_cast<unsigned char>(value));
  check(subject_fill(d,value,size)==d,"guarded fill return");
  check(std::memcmp(destination.data,expected.data(),destination.capacity)==0,"guarded fill exact bytes");++guardCases;
 }
 // A zero count must not touch even an inaccessible page or a null pointer.
 check(subject_copy(destination.mapping,source.mapping,0)==destination.mapping,"zero inaccessible copy");
 check(subject_fill(destination.mapping,0x1234,0)==destination.mapping,"zero inaccessible fill");
 check(subject_copy(nullptr,nullptr,0)==nullptr,"zero null copy");
 check(subject_fill(nullptr,0x1234,0)==nullptr,"zero null fill");
}

#ifdef RUNTIME_MEMORY_BENCHMARK
static volatile uint64_t witness=0;
static double median(std::vector<double> values){std::sort(values.begin(),values.end());return values[values.size()/2];}
static void benchmark(){
 constexpr size_t Sizes[]={1,4,8,16,17,32,64,128,256,4096,6340000,5128*4096};
 for(size_t size:Sizes){
  std::vector<unsigned char>source(size,0x57),output(size,0xa7);
  const size_t iterations=std::max(size_t(8),std::min(size_t(1000000),size_t(64*1024*1024)/size));
  for(bool fill:{false,true}){
   std::vector<double>elapsed[2];
   for(unsigned round=0;round<7;++round)for(unsigned order=0;order<2;++order){
    const unsigned phase=order^(round&1);auto copy=phase?subject_copy:baseline_copy;auto set=phase?subject_fill:baseline_fill;
    for(unsigned warmup=0;warmup<8;++warmup){if(fill)set(output.data(),0x93,size);else copy(output.data(),source.data(),size);}
    const auto start=std::chrono::steady_clock::now();
    if(fill)for(size_t i=0;i<iterations;++i)set(output.data(),0x93,size);
    else for(size_t i=0;i<iterations;++i)copy(output.data(),source.data(),size);
    const auto finish=std::chrono::steady_clock::now();witness+=output[round%size];
    check(std::all_of(output.begin(),output.end(),[&](unsigned char b){return b==(fill?0x93:0x57);}),"benchmark output");
    elapsed[phase].push_back(std::chrono::duration<double,std::nano>(finish-start).count()/double(iterations));
   }
   std::printf("{\"size_bytes\":%zu,\"operation\":\"%s\",\"iterations_per_round\":%zu,\"before_median_ns\":%.6f,\"after_median_ns\":%.6f,\"before_ns\":[",size,fill?"fill":"copy",iterations,median(elapsed[0]),median(elapsed[1]));
   for(size_t i=0;i<elapsed[0].size();++i)std::printf("%s%.6f",i?",":"",elapsed[0][i]);
   std::printf("],\"after_ns\":[");for(size_t i=0;i<elapsed[1].size();++i)std::printf("%s%.6f",i?",":"",elapsed[1][i]);std::puts("]}");
  }
 }
}
#endif

int main(){
#ifdef RUNTIME_MEMORY_BENCHMARK
 benchmark();return 0;
#else
 for(size_t size=0;size<=33;++size)matrix(size);
 for(size_t size:{size_t(63),size_t(64),size_t(65),size_t(127),size_t(128),size_t(129),size_t(4095),size_t(4096),size_t(4097),size_t(65537),size_t(6340000)})matrix(size);
 for(size_t size:{size_t(0),size_t(1),size_t(2),size_t(3),size_t(4),size_t(7),size_t(8),size_t(15),size_t(16),size_t(17),size_t(31),size_t(63),size_t(64),size_t(65),size_t(4095),size_t(4096),size_t(4097),size_t(6340000),size_t(5128*4096)})guards(size);
 std::printf("{\"passed\":true,\"checks\":%llu,\"copy_cases\":%llu,\"fill_cases\":%llu,\"guard_page_cases\":%llu,\"source_alignment_residues\":16,\"destination_alignment_residues\":16,\"windows_executed\":false,\"game_executed\":false}\n",(unsigned long long)checks,(unsigned long long)copyCases,(unsigned long long)fillCases,(unsigned long long)guardCases);
#endif
}
