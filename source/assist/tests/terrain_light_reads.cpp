#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../../external_reader/source/process_reader.h"
#include "../../external_reader/source/scene_lighting.h"
#include RA2_REFERENCE_LIGHTING
#include <cassert>
#include <chrono>
#include <cstdio>
using namespace external;
template<class T>static void put(uint32_t at,const T&value){std::memcpy(reinterpret_cast<void*>(uintptr_t(at)),&value,sizeof value);}
template<int Tag>struct Read {ProcessReader*reader;uint64_t*calls;bool operator()(uint32_t at,void*out,size_t n)const{++*calls;return reader->read(at,out,n);}};
static double now(){return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();}
int main(){
 void*globals=VirtualAlloc(reinterpret_cast<void*>(0x600000),0x600000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
 void*cells=VirtualAlloc(reinterpret_cast<void*>(0x1000000),0x500000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
 if(globals!=reinterpret_cast<void*>(0x600000)||cells!=reinterpret_cast<void*>(0x1000000)){std::printf("FAIL exact fixture allocation globals=%p cells=%p error=%lu\n",globals,cells,GetLastError());return 2;}
 put(0xA8B230,uint32_t(0x600000));put(0x710000,uint32_t(0x7E5358));put(0x710004,2);put(0x71016C,1);put(0x710170,uint32_t(0x720000));put(0x710198,std::array<int,3>{{1000,1000,1000}});put(0x720002,uint16_t(0xf800));
 for(unsigned n=0;n<10000;++n){uint32_t p=0x1000000+n*0x200;put(p+0x104,uint32_t(65536));put(p+0x34,uint32_t(0x710000));put(p+0x10A,std::array<int16_t,3>{{int16_t(900+n%100),int16_t(700+n%500),1000}});}
 ProcessReader oldReader,newReader;assert(oldReader.open(GetCurrentProcessId())&&newReader.open(GetCurrentProcessId()));
 std::vector<lighting::State>oldValues(10000),newValues(10000);double oldMs=0,newMs=0;uint64_t oldCalls=0,newCalls=0,oldRPM=0,newRPM=0;
 for(int frame=0;frame<31;++frame){put(0xA8ED84,frame);const uint32_t changed=0x1000000+(frame*13)*0x200;put(changed+0x10A,std::array<int16_t,3>{{900,int16_t(600+frame),1000}});
  auto before=[&]{uint64_t calls=0;Read<0>read{&oldReader,&calls};oldReader.begin();lighting::TintCache cache;double start=now();for(unsigned n=0;n<10000;++n)oldValues[n]=lighting::cell_before(read,0x1000000+n*0x200,{},lighting::Layer::Terrain,true,&cache);if(frame){oldMs+=now()-start;oldCalls+=calls;oldRPM+=oldReader.reads;}};
  auto after=[&]{uint64_t calls=0;Read<1>read{&newReader,&calls};newReader.begin();lighting::TintCache cache;double start=now();for(unsigned n=0;n<10000;++n)newValues[n]=lighting::cell(read,0x1000000+n*0x200,{},lighting::Layer::Terrain,true,&cache);if(frame){newMs+=now()-start;newCalls+=calls;newRPM+=newReader.reads;}};
  if(frame&1){before();after();}else{after();before();}
  for(unsigned n=0;n<10000;++n){const auto&a=oldValues[n];const auto&b=newValues[n];assert(a.ambient==b.ambient&&a.intensity==b.intensity&&a.tint==b.tint&&a.paletteRows==b.paletteRows&&a.scalar==b.scalar&&a.rgb555==b.rgb555&&a.table&&b.table&&*a.table==*b.table);}
  assert(oldReader.errors==0&&newReader.errors==0);
 }
 assert(oldRPM==newRPM&&newCalls*2<oldCalls);
 std::printf("PASS production ProcessReader/lighting, self-process fixture, 10000 cells x 30 new read epochs: exact State/table bytes, actual_RPM_per_epoch=%llu->%llu read_calls=%llu->%llu mean_ms=%.6f->%.6f; changed per-cell intensity revalidated each epoch.\n",(unsigned long long)(oldRPM/30),(unsigned long long)(newRPM/30),(unsigned long long)(oldCalls/30),(unsigned long long)(newCalls/30),oldMs/30,newMs/30);
 oldReader.close();newReader.close();VirtualFree(cells,0,MEM_RELEASE);VirtualFree(globals,0,MEM_RELEASE);
}
