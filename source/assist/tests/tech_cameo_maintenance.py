"""Run actual per-frame sidebar inventory/admission code in a bounded fake world.

Native eligibility, factories and AddCameo have explicit deterministic seams.
No executable, network connection, production order or code patch is involved.
"""
from pathlib import Path
import argparse, hashlib, json, subprocess
from tech_refresh_full_pass import function

HERE=Path(__file__).resolve().parent
SOURCE=HERE.parent/'source'

def source():
    sidebar=(SOURCE/'sidebar.h').read_text(encoding='utf-8')
    ensure=function(sidebar,'static bool ensureTechCameo(')
    begin=sidebar.index('struct TechCameoRetry')
    end=sidebar.index('struct FlashRef',begin)
    actual=sidebar[begin:end]
    return r'''
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <tuple>
#include <vector>
#include "sidebar_policy.h"
#include "technology_mode_policy.h"
using namespace assist;using DWORD=uint32_t;
#define assert(value) ((value)?(void)0:(std::fprintf(stderr,"assert %s line %d\n",#value,__LINE__),std::_Exit(3)))
static constexpr int MaxTypes=512;
static constexpr uint32_t Sidebar=0x87F7E8,Tabs=Sidebar+0x1544,UnsafeTech=4096;
struct Type{int kind=0,index=0;};
struct Snapshot{int typeCount=0,techOpen=4,techVisible=0,techUnavailable=0;Type types[MaxTypes];}snap;
static struct{bool stop=false;uint32_t flags=UnsafeTech;}options,*sh=&options;
static struct{uint32_t epoch=7;}sessionTracker;
static uint32_t lastHouse=0x100000,typePointers[MaxTypes];static bool techAllowed[MaxTypes];
static bool techPending=false;static unsigned techImplementation=3,techScanImplementation=UINT32_MAX;
static int techScanFrame=-1;static uint64_t catalogueHash=1;
static std::vector<uint8_t> memory(0x1000000);static uint32_t failAt=0;
static int readCalls=0,readBytes=0,canBuildCalls=0,factoryCalls=0,addCalls=0,redraws=0;
static std::array<bool,MaxTypes> legal,factory;
template<class T>static T get(uint32_t p){assert(size_t(p)+sizeof(T)<=memory.size());T v{};std::memcpy(&v,memory.data()+p,sizeof v);return v;}
template<class T>static void put(uint32_t p,T v){assert(size_t(p)+sizeof(T)<=memory.size());std::memcpy(memory.data()+p,&v,sizeof v);}
static bool read(uint32_t p,void*out,size_t n){++readCalls;readBytes+=int(n);if(size_t(p)+n>memory.size()||(failAt>=p&&failAt<uint64_t(p)+n))return false;std::memcpy(out,memory.data()+p,n);return true;}
static int typeTab(int i){return i%4;}
static int typeIndex(uint32_t p){assert(p>=0x200000&&(p-0x200000)%0x1000==0);return int((p-0x200000)/0x1000);}
static bool hasCameo(int kind,int index){for(int tab=0;tab<4;++tab){uint32_t strip=Tabs+tab*3988;int count=get<int>(strip+84);if(count<0||count>75)continue;for(int n=0;n<count;++n)if(get<int>(strip+88+n*52)==index&&sidebarpolicy::canonicalKind(get<int>(strip+92+n*52))==kind)return true;}return false;}
static void sidebarRedraw(int){++redraws;}
static bool techSessionAllowed(){return true;}
namespace eventdelivery {static bool mayQueue(int s){return s==0||s==3;}}
static uint32_t techFactory(uint32_t p,uint32_t house){assert(house==lastHouse);++factoryCalls;return factory[typeIndex(p)]?0x500000:0;}
template<class R,class...Args>static R call(uint32_t at,uint32_t self,Args...args){
 auto values=std::tuple<Args...>(args...);
 if constexpr(sizeof...(Args)==3){assert(at==0x4F7870&&self==lastHouse);++canBuildCalls;return R(legal[typeIndex(uint32_t(uintptr_t(std::get<0>(values))))]?1:0);}
 if constexpr(sizeof...(Args)==4){assert(at==0x600000);++factoryCalls;return R(factory[typeIndex(self)]?0x500000:0);}
 if constexpr(sizeof...(Args)==2){assert(at==0x6A6300&&self==Sidebar);int kind=std::get<0>(values),index=std::get<1>(values),tab=-1;for(int i=0;i<snap.typeCount;++i)if(snap.types[i].kind==kind&&snap.types[i].index==index){tab=typeTab(i);break;}assert(tab>=0);uint32_t strip=Tabs+tab*3988;int count=get<int>(strip+84);assert(count<75&&!hasCameo(kind,index));++addCalls;
  // Stand-in owns sort order, just as native AddCameo does in production.
  int pos=0;while(pos<count&&get<int>(strip+88+pos*52)<index)++pos;
  std::memmove(memory.data()+strip+88+(pos+1)*52,memory.data()+strip+88+pos*52,size_t(count-pos)*52);
  std::memset(memory.data()+strip+88+pos*52,0,52);put(strip+88+pos*52,index);put(strip+92+pos*52,kind);put(strip+84,count+1);return R(true);}
 assert(false);return R{};
}
''' + ensure + '\n' + actual + r'''
static void reset(int count=300){
 std::fill(memory.begin(),memory.end(),0);snap={};snap.typeCount=count;snap.techOpen=4;options={};techImplementation=3;techPending=false;techScanFrame=-1;techScanImplementation=UINT32_MAX;techCameoHouse=0;techCameoFrame=INT32_MIN;failAt=0;legal.fill(true);factory.fill(true);
 put(0xA83D4C,lastHouse);put(0xA8E9A0,true);put(0xA8B238,3);
 static const int kinds[]={7,40,16,3};for(int i=0;i<count;++i){typePointers[i]=0x200000+i*0x1000;snap.types[i]={kinds[i%4],i/4};techAllowed[i]=true;put(typePointers[i],0x700000u);put(0x700000+0x94,0x600000u);}
 for(int i=count;i<MaxTypes;++i){techAllowed[i]=false;typePointers[i]=0;}
 readCalls=readBytes=canBuildCalls=factoryCalls=addCalls=redraws=0;
}
static void presentFrom(int start){for(int i=start;i<snap.typeCount;++i){auto&t=snap.types[i];assert(call<bool>(0x6A6300,Sidebar,t.kind,t.index));}readCalls=readBytes=canBuildCalls=factoryCalls=addCalls=redraws=0;}
int main(){
 std::printf("{\"missing_cases\":[");bool comma=false;
 for(int missing:{0,1,8,75,150,300}){reset();presentFrom(missing);maintainTechCameos(lastHouse,100,1000);
  assert(addCalls==missing&&canBuildCalls==missing&&factoryCalls==missing&&snap.techVisible==300&&!snap.techUnavailable);
  if(comma)std::printf(",");comma=true;std::printf("{\"missing\":%d,\"native_admission\":%d,\"added_same_frame\":%d}",missing,canBuildCalls,addCalls);
  int calls=canBuildCalls;maintainTechCameos(lastHouse,100,1001);assert(canBuildCalls==calls);maintainTechCameos(lastHouse,101,1016);assert(canBuildCalls==calls&&snap.techVisible==300);
 }
 std::printf("],");
 reset();presentFrom(1);legal[0]=false;maintainTechCameos(lastHouse,100,1000);assert(!addCalls&&canBuildCalls==1&&!factoryCalls&&snap.techUnavailable==1);
 maintainTechCameos(lastHouse,101,1249);assert(canBuildCalls==1);legal[0]=true;maintainTechCameos(lastHouse,102,1250);assert(canBuildCalls==2&&addCalls==1);
 reset();presentFrom(1);factory[0]=false;maintainTechCameos(lastHouse,100,1000);assert(!addCalls&&canBuildCalls==1&&factoryCalls==1);
 reset();presentFrom(1);failAt=Tabs+88;maintainTechCameos(lastHouse,100,1000);assert(!addCalls&&!canBuildCalls);failAt=0;maintainTechCameos(lastHouse,100,1000);assert(addCalls==1);
 reset();put(Tabs+84,76);maintainTechCameos(lastHouse,100,1000);assert(!addCalls&&!canBuildCalls);
 reset(512);maintainTechCameos(lastHouse,100,1000);assert(addCalls==300&&snap.techVisible==300&&snap.techUnavailable==212);for(int tab=0;tab<4;++tab)assert(get<int>(Tabs+tab*3988+84)==75);
 reset();sh->stop=true;maintainTechCameos(lastHouse,100,1000);assert(!readCalls&&!canBuildCalls&&!addCalls);
 reset();sh->flags=0;maintainTechCameos(lastHouse,100,1000);assert(!readCalls&&!canBuildCalls&&!addCalls);
 reset();put(lastHouse+0x1F5,true);maintainTechCameos(lastHouse,100,1000);assert(!readCalls&&!canBuildCalls&&!addCalls);
 reset();techImplementation=2;maintainTechCameos(lastHouse,100,1000);assert(!readCalls&&!canBuildCalls&&!addCalls);
 reset();presentFrom(0);auto start=std::chrono::steady_clock::now();for(int frame=0;frame<1000;++frame)maintainTechCameos(lastHouse,frame,1000+16*frame);auto end=std::chrono::steady_clock::now();
 assert(!canBuildCalls&&!factoryCalls&&!addCalls);double us=std::chrono::duration<double,std::micro>(end-start).count()/1000;
 std::printf("\"stable_1000_frames\":{\"microseconds_per_frame\":%.6f,\"read_calls_per_frame\":%.1f,\"read_bytes_per_frame\":%.1f,\"native_admission\":0,\"adds\":0},\"pass\":true}\n",us,readCalls/1000.,readBytes/1000.);
}
'''

def main():
    p=argparse.ArgumentParser();p.add_argument('--zig',required=True,type=Path);p.add_argument('--build-dir',required=True,type=Path);a=p.parse_args();a.build_dir.mkdir(parents=True,exist_ok=True)
    cpp=a.build_dir/'tech-cameo-maintenance.cpp';exe=a.build_dir/'tech-cameo-maintenance.exe';cpp.write_text(source(),encoding='utf-8')
    subprocess.run([str(a.zig),'c++','-std=c++17','-O2','-UNDEBUG','-target','x86_64-windows-gnu','-I'+str(SOURCE),str(cpp),'-o',str(exe)],check=True)
    run=subprocess.run([str(exe)],capture_output=True,text=True,timeout=30);print(run.stdout,end='');print(run.stderr,end='');run.check_returncode();result=json.loads(run.stdout);result['assertions_active']=True;result['source_sha256']=hashlib.sha256((SOURCE/'sidebar.h').read_bytes()).hexdigest();result['scope']='Actual production inventory/admission code; synthetic eligibility/factory/AddCameo. Not game or multiplayer acceptance.'
    (a.build_dir/'result.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')

if __name__=='__main__':main()
