#ifdef NDEBUG
#error assertions required
#endif
#define NOMINMAX
#include <windows.h>
#include <cstdio>
#include <cassert>
#include <chrono>
#include <cstring>
#include <vector>
#include "../../external_reader/source/scene_lighting.h"
#include "fixtures/object_lighting_baseline/object.inc"
using namespace external;
static std::vector<uint8_t>memory(0x2000000);
static uint64_t calls,bytes;static uint32_t failAt;static bool rpm,varyFallback=true;
template<class T>static void put(uint32_t at,const T&v){assert(size_t(at)+sizeof v<memory.size());std::memcpy(memory.data()+at,&v,sizeof v);}
struct Read {bool operator()(uint32_t at,void*out,size_t n)const{
 ++calls;bytes+=n;if(uint64_t(at)+n>memory.size()||(failAt>=at&&failAt<uint64_t(at)+n))return false;
 if(rpm){SIZE_T got=0;return ReadProcessMemory(GetCurrentProcess(),memory.data()+at,out,n,&got)&&got==n;}
 std::memcpy(out,memory.data()+at,n);return true;
}};
static bool equal(const lighting::State&a,const lighting::State&b){return a.ambient==b.ambient&&a.tint==b.tint&&a.intensity==b.intensity&&a.paletteRows==b.paletteRows&&a.scalar==b.scalar&&a.rgb555==b.rgb555&&((!a.table&&!b.table)||(a.table&&b.table&&*a.table==*b.table));}
struct Actor {uint32_t type,owner;int kind,x,y,z;};static std::vector<Actor>actors;
static void initialize(){
 put(0xA8B230,uint32_t(0x600000));put(0xA8ED84,1);put(0x60352C,100);put(0x603534,std::array<int,3>{{100,100,100}});
 put(0x8871E0,uint32_t(0x730000));put(0x7317D4,int32_t(30));put(0x7317D8,int32_t(-40));put(0x7317DC,int32_t(15));put(0x89E7C0,256);
 put(0x87F924,uint32_t(0x1800000));put(0x87F928,512*512);put(0xB054D4,uint32_t(0x270000));put(0xB054E0,3);
 for(int n=0;n<3;++n){uint32_t owner=0x400000+n*0x20000,scheme=0x700000+n*0x1000,convert=0x710000+n*0x1000,data=0x740000+n*0x10000;
  put(owner+0x16054,n);put(0x270000+n*4,scheme);put(scheme+0x30C,convert);put(convert,uint32_t(n==1?0x7ED0A4:0x7E5358));put(convert+4,2);put(convert+0x16C,n==2?1:53);put(convert+0x170,data);put(convert+0x1B0,uint8_t(n&1));put(convert+(n&1?0x1A4:0x198),std::array<int,3>{{1000+n*20,900+n*20,1100-n*10}});
  for(int row=0;row<(n==2?1:53);++row)for(int i=0;i<256;++i)put(data+(row*256+i)*2,uint16_t((row*113+i*17)&65535));
 }
 for(int n=0;n<8;++n){uint32_t type=0x250000+n*0x2000;put(type+0x236,uint8_t(n&1));put(type+0xDF0,uint32_t(n==6?0x260000:0));put(type+0x1548,int16_t(n*13));put(type+0x1702,uint8_t(n==4));}
 put(0x260004,uint32_t(0x280000));put(0x260010,3);for(int n=0;n<3;++n)put(0x280000+n*4,uint32_t(0x700000+(2-n)*0x1000));
 for(int n=0;n<1024;++n){int x=n%32+1,y=n/32+1;uint32_t cell=0x1000000+n*0x200;put(0x1800000+(y*512+x)*4,cell);put(cell+0x24,std::array<int16_t,2>{{int16_t(x),int16_t(y)}});put(cell+0x104,uint32_t(65536));put(cell+0x34,uint32_t(0x710000+(n%3)*0x1000));put(cell+0x10A,std::array<int16_t,3>{{int16_t(700+n%200),int16_t(900+n%100),int16_t(1200+n%200)}});put(cell+0x140,uint32_t(n%2?0x80:0));put(cell+0x11B,int8_t(2));
  actors.push_back({uint32_t(0x250000+(n%8)*0x2000),uint32_t(0x400000+(n%3)*0x20000),n%4,x*256+128,y*256+128,n%5?0:1536});
 }
}
static double now(){return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();}
static std::vector<lighting::State>run(bool cached){lighting::ObjectLightContext context;lighting::State fallback;fallback.ambient=93;fallback.tint={{850,1100,950}};fallback.paletteRows=7;fallback.scalar=true;fallback.rgb555=true;std::vector<lighting::State>out;out.reserve(actors.size());for(size_t n=0;n<actors.size();++n){const auto&a=actors[n];if(varyFallback){fallback.paletteRows=7+unsigned(n%3);fallback.scalar=n&1;fallback.rgb555=n%3==1;fallback.tint={{850+int(n%4)*20,1100,950}};}out.push_back(cached?lighting::object(Read{},a.type,a.owner,a.kind,a.x,a.y,a.z,fallback,&context):lighting::objectBefore(Read{},a.type,a.owner,a.kind,a.x,a.y,a.z,fallback));}return out;}
int main(){initialize();unsigned checks=0;auto compare=[&]{auto old=run(false),next=run(true);assert(old.size()==next.size());for(size_t n=0;n<old.size();++n){assert(equal(old[n],next[n]));++checks;}};
 compare(); // mixed type/owner, converters, scalar/plain, bridge layers, custom palette.
 for(int variant=0;variant<20;++variant){failAt=0;put(0xA8ED84,variant+2);put(0x8A0DD0,variant&1?std::array<int,6>{{10,3,0,3,5,3}}:std::array<int,6>{});
  put(0x400000+0x16054,variant%3);put(0x250000+0xDF0,uint32_t(variant&1?0x260000:0));put(0x710198,std::array<int,3>{{900+variant,1000,1100}});put(0x7317D8,int32_t(variant-40));put(0x710170,uint32_t(0x740000+(variant%2)*0x10000));
  if(variant<16){const uint32_t fails[]={0x8A0DD0,0x71016C,0x710000,0x710004,0x710170,0x710198,0x7101B0,0x8871E0,0x7317D8,0x400000+0x16054,0x250000+0xDF0,0xB054D4,0x270000,0x70030C,0x1000000+0x10A,0x87F924};failAt=fails[variant];}compare();}
 failAt=0;rpm=true;compare(); // Actual local Win32 ReadProcessMemory boundary.
 varyFallback=false;compare();
 double oldMs=0,newMs=0;uint64_t oldCalls=0,newCalls=0;for(int repeat=0;repeat<20;++repeat){auto baseline=[&]{calls=0;double start=now();auto r=run(false);oldMs+=now()-start;oldCalls+=calls;assert(r.size()==1024);};auto candidate=[&]{calls=0;double start=now();auto r=run(true);newMs+=now()-start;newCalls+=calls;assert(r.size()==1024);};if(repeat&1){baseline();candidate();}else{candidate();baseline();}}
 assert(newCalls<oldCalls);
 std::printf("PASS %u exact per-actor light states across custom/nonuniform owner/type/converter/bridge/intensity/fallback/failed field/new-pass changes; actual ReadProcessMemory call boundary. benchmark_same_scene_fallback actors=1024 calls=%llu->%llu mean_ms=%.6f->%.6f\n",checks,(unsigned long long)(oldCalls/20),(unsigned long long)(newCalls/20),oldMs/20,newMs/20);
}
