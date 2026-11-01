#!/usr/bin/env python3
"""Actual rule-data bridge world/restore/TechEnhance handoff under synthetic memory."""
import argparse,json,subprocess
from pathlib import Path

def function(s,start):
 a=s.index(start);o=s.index('{',a);i=o+1;n=1
 while n:n+=(s[i]=='{')-(s[i]=='}');i+=1
 return s[a:i]
def main():
 p=argparse.ArgumentParser();p.add_argument('--zig',type=Path,required=True);p.add_argument('--build-dir',type=Path,required=True);a=p.parse_args()
 src=Path(__file__).resolve().parents[1]/'source';source=(src/'tech_data_bridge.h').read_text(encoding='utf-8')
 globals=source[:source.index('static bool techDataWrite(')]
 funcs='\n'.join(function(source,n) for n in ['static techdata::State techDataWorld(','static bool techDataPending(','static bool techDataOwns(','static bool restoreTechData(','static bool techDataBlocksEnhancement(','static bool applyTechData('])
 code=r'''
#include <cstdint>
#include <climits>
#include <cstring>
#include <cstdio>
#include <map>
using namespace assist;
constexpr int MaxTypes=4;constexpr uint32_t UnsafeTech=4096;
struct Type {int kind=40,index=0;};struct Snapshot {int typeCount=2;Type types[MaxTypes];} snap;
struct Settings {uint32_t flags=UnsafeTech;bool techEnhance=true;} options,*sh=&options;
struct Tracker {uint32_t epoch=1;} sessionTracker;
static uint32_t lastHouse=0x10000,typePointers[MaxTypes]{0x20000,0x22000};static unsigned techImplementation=3;
static bool techAllowed[MaxTypes]{true,true},enhancementChangedCatalogue=false,catalogueDirty=false,catalogueReady=true;
static uint32_t catalogueAt=0;static bool enhancementOwned=false,enhancementFail=false;static unsigned enhancementRestores=0;
struct Memory {std::map<uint32_t,uint8_t> bytes;uint32_t unreadable=0,failWrite=0;bool failAfter=false;unsigned writes=0;
 template<class T>void seed(uint32_t at,T value){auto*p=(const uint8_t*)&value;for(unsigned i=0;i<sizeof value;++i)bytes[at+i]=p[i];}
 bool read(uint32_t at,void*out,size_t n){if(unreadable>=at&&unreadable<uint64_t(at)+n)return false;auto*p=(uint8_t*)out;for(unsigned i=0;i<n;++i){auto it=bytes.find(at+i);if(it==bytes.end())return false;p[i]=it->second;}return true;}
} memory;
static bool read(uint32_t a,void*p,size_t n){return memory.read(a,p,n);}
static bool techDataWrite(uint32_t a,const void*p,size_t n){++memory.writes;if(a==memory.failWrite&&!memory.failAfter)return false;auto*b=(const uint8_t*)p;for(unsigned i=0;i<n;++i)memory.bytes[a+i]=b[i];return a!=memory.failWrite;}
static bool restoreTechEnhancement(){++enhancementRestores;if(enhancementFail)return false;if(enhancementOwned){int original=-1;if(!techDataWrite(0x20634,&original,4))return false;enhancementOwned=false;catalogueDirty=true;}return true;}
GLOBALS
FUNCTIONS
static int level(int n=0){int value=0;read(typePointers[n]+0x634,&value,4);return value;}
static void reset(){
 memory={};options={};snap={};snap.types[1].index=1;sessionTracker={};lastHouse=0x10000;techImplementation=3;
 techDataScenario=techDataHouse=techDataEpoch=0;techDataSession=techDataFrame=-1;techDataRestoring=false;for(auto&r:techDataRecords)r={};
 enhancementOwned=enhancementFail=false;enhancementRestores=0;enhancementChangedCatalogue=catalogueDirty=false;catalogueReady=true;catalogueAt=0;techAllowed[0]=techAllowed[1]=true;
 memory.seed<uint32_t>(0xA8B230,0x90000);memory.seed<int>(0xA8B238,3);memory.seed<int>(0xA8ED84,100);
 memory.seed<uint32_t>(0xA83CE4,0x30000);memory.seed<int>(0xA83CF0,2);
 for(int i=0;i<2;++i){uint32_t t=typePointers[i];memory.seed<uint32_t>(0x30000+i*4,t);memory.seed<uint32_t>(t,techdata::vtable(40));memory.seed<uint32_t>(t+0x10,10+i);char id[25]="TYPE";id[4]=char('0'+i);for(unsigned k=0;k<25;++k)memory.seed<uint8_t>(t+0x24+k,id[k]);
  for(unsigned j=0;j<techdata::FieldCount;++j){auto&f=techdata::Fields[j];if(f.size==1)memory.seed<uint8_t>(t+f.offset,1);else memory.seed<uint32_t>(t+f.offset,j==1?2:8);}
  memory.seed<uint32_t>(t+0x63C,0x40000);memory.seed<int>(t+0x640,4);memory.seed<int>(t+0x3B8,5);
 }
}
int main(){unsigned checks=0,failed=0;auto check=[&](bool ok,const char*s){++checks;if(!ok){++failed;std::fprintf(stderr,"FAIL %s\n",s);}};
 reset();enhancementOwned=true;memory.seed<int>(0x20634,1);check(applyTechData(lastHouse)&&level()==0&&techDataPending(),"data mode applies after enhancement restoration");
 check(techDataRecords[0].original[0]==uint32_t(-1)&&!enhancementOwned&&options.techEnhance,"journal captures true pre-enhancement original and preserves preference");
 catalogueDirty=false;unsigned writes=memory.writes;check(applyTechData(lastHouse)&&memory.writes==writes&&!catalogueDirty,"no repeat writes or repeated enhancement catalogue reset");
 techImplementation=0;check(!techDataBlocksEnhancement()&&!techDataPending()&&level()==-1,"exiting data restores before enhancement can reapply");
 reset();enhancementOwned=true;enhancementFail=true;memory.seed<int>(0x20634,1);check(!applyTechData(lastHouse)&&!techDataPending()&&level()==1,"failed enhancement rollback blocks new owner");
 enhancementFail=false;check(applyTechData(lastHouse)&&techDataRecords[0].original[0]==uint32_t(-1),"enhancement restore retry preserves true original");
 reset();memory.seed<uint32_t>(typePointers[1],0x12345678);check(!applyTechData(lastHouse)&&!techDataPending()&&level()==8,"later type preflight failure rolls back already written type");
 reset();check(applyTechData(lastHouse),"initial valid group");memory.seed<uint32_t>(typePointers[0]+0xDA4,77);techImplementation=0;
 check(techDataBlocksEnhancement()&&techDataRestoring&&techDataPending(),"foreign value blocks handoff to enhancement");uint32_t value=0;read(typePointers[0]+0xDA4,&value,4);check(value==77,"foreign field never overwritten");
 memory.seed<uint32_t>(typePointers[0]+0xDA4,8);check(!techDataBlocksEnhancement()&&!techDataPending()&&level()==8,"restored original permits handoff retry");
 reset();applyTechData(lastHouse);memory.unreadable=0xA8B230;techImplementation=0;writes=memory.writes;
 check(techDataBlocksEnhancement()&&techDataPending()&&memory.writes==writes,"unknown world keeps responsibility without writes");memory.unreadable=0;
 check(!techDataBlocksEnhancement()&&level()==8,"same world recovery restores");
 reset();applyTechData(lastHouse);lastHouse=0x11000;sessionTracker.epoch=2;
 check(techDataBlocksEnhancement()&&!techDataPending()&&level()==8,"same-world House/epoch transition restores original");
 check(applyTechData(lastHouse)&&techDataRecords[0].original[0]==8&&techDataHouse==lastHouse,"new authority binds after old values restored");
 reset();applyTechData(lastHouse);techAllowed[0]=false;check(applyTechData(lastHouse)&&level()==8&&level(1)==0&&!techDataOwns(typePointers[0])&&techDataOwns(typePointers[1]),"allowlist removal restores only deselected type");
 reset();applyTechData(lastHouse);memory.seed<int>(0xA8ED84,150);techDataBlocksEnhancement();check(techDataFrame==150,"world frame high water tracks each tick");
 memory.seed<int>(0xA8ED84,120);writes=memory.writes;techDataBlocksEnhancement();check(!techDataPending()&&memory.writes==writes,"rewound world retires old pointer responsibility without writes");
 reset();applyTechData(lastHouse);memory.seed<uint32_t>(0xA8B230,0x91000);writes=memory.writes;techDataBlocksEnhancement();check(!techDataPending()&&memory.writes==writes,"changed Scenario never restores through old identity");
 reset();applyTechData(lastHouse);memory.failWrite=0x20634;memory.failAfter=true;techImplementation=0;
 check(techDataBlocksEnhancement()&&techDataPending()&&level()==8,"post-restore verification failure retains ownership");memory.failWrite=0;writes=memory.writes;
 check(!techDataBlocksEnhancement()&&!techDataPending()&&memory.writes==writes,"already restored retry is idempotent");
 std::printf("technology data-world production checks=%u failures=%u; synthetic reads/writes, no game/netplay\n",checks,failed);return failed?1:0;
}
'''.replace('using namespace assist;','#include "tech_data_policy.h"\nusing namespace assist;').replace('GLOBALS',globals).replace('FUNCTIONS',funcs)
 a.build_dir.mkdir(parents=True,exist_ok=True);cpp=a.build_dir/'tech_data_world.cpp';exe=a.build_dir/'tech_data_world.exe';cpp.write_text(code,encoding='utf-8')
 cmd=[str(a.zig),'c++','-target','x86_64-windows-gnu','-std=c++17','-O1','-fno-exceptions','-fno-rtti','-I',str(src),str(cpp),'-o',str(exe)]
 c=subprocess.run(cmd,text=True,capture_output=True);(a.build_dir/'build.log').write_text(c.stdout+c.stderr,encoding='utf-8')
 if c.returncode:print(c.stderr);raise SystemExit(c.returncode)
 r=subprocess.run([str(exe)],text=True,capture_output=True);(a.build_dir/'test.log').write_text(r.stdout+r.stderr,encoding='utf-8');(a.build_dir/'result.json').write_text(json.dumps(dict(exit_code=r.returncode,production_functions_extracted=True,game_tested=False,multiplayer_tested=False,output=r.stdout+r.stderr),indent=2),encoding='utf-8');print(r.stdout+r.stderr);raise SystemExit(r.returncode)
if __name__=='__main__':main()
