"""Compile the shipped ProduceDeferred branch against a minimal memory/event model.

This verifies the game's branch guards, not an actual x86 DLL or match.
"""
from pathlib import Path
import os
import subprocess
import tempfile

root=Path(__file__).resolve().parents[2]
source=(root/'assist/source/bridge.cpp').read_text()
begin=source.index(' else if(c==ProduceDeferred){')
end=source.index(' else if(c==Produce||c==CancelTechProduction)',begin)
branch=source[begin:end].replace(' else if(c==ProduceDeferred)', ' if(c==ProduceDeferred)',1)
body=r'''
#include <cassert>
#include <cstdint>
#include <map>
#include <iostream>
using uint32_t=std::uint32_t;
struct Type{int kind=0,index=0,naval=0;};
struct Snapshot{Type types[4]{};int typeCount=1;};
struct Shared{uint32_t flags=0;};
static constexpr uint32_t ReserveConstruction=1;
static Snapshot snap;static Shared shared,*sh=&shared;
static uint32_t typePointers[4]{0x600000};
static uint32_t house=0x500000;static std::map<uint32_t,uint32_t> mem;
static int c=1,arg=0,result=0,n=0,events=0,canBuild=1;
static bool cameo=true,factoryFound=true,append=true;
static constexpr int ProduceDeferred=1;
template<class T>T get(uint32_t address){auto i=mem.find(address);return i==mem.end()?T{}:T(i->second);}
template<class T,class...A>T call(uint32_t,A...){return T(canBuild);}
static int typeTab(int){return snap.types[arg].kind==7?0:snap.types[arg].kind==16?2:3;}
static bool hasCameo(int,int){return cameo;}
static bool techFactory(uint32_t,uint32_t){return factoryFound;}
static bool nativeProduceEvent(uint32_t,const Type&,bool){++events;return append;}
static void watchReservedSidebar(uint32_t,int,int,uint32_t,int){}
static int now=10;
static int run(int kind,uint32_t offset,int waiting,uint32_t product=0x700000,bool naval=false,bool owner=true){
 snap.types[0]={kind,0,int(naval)};mem[house+offset]=0x710000;
 mem[0x710000+0x6C]=owner?house:house+4;mem[0x710000+0x50]=waiting;mem[0x710000+0x58]=product;
 result=n=0;
'''+branch+r'''
 return result;
}
int main(){
 sh->flags=ReserveConstruction;
 for(auto item:{std::pair<int,uint32_t>{16,0x53B0},{40,0x53B4},{3,0x53AC}}){
  int before=events;assert(run(item.first,item.second,29)==0&&n==1&&events==before+1);
  assert(run(item.first,item.second,0)==0&&n==1&&events==before+2);
 }
 assert(run(40,0x53B8,29,0x700000,true)==0&&n==1&&events==7);
 assert(run(7,0x53BC,29)==-9&&n==0&&events==7);
 assert(run(7,0x53BC,0,0)==0&&n==1&&events==8);
 assert(run(16,0x53B0,1,0x700000,false,false)==-9&&n==0&&events==8);
 cameo=false;assert(run(16,0x53B0,0)==-9);cameo=true;
 canBuild=0;assert(run(16,0x53B0,0)==-9);canBuild=1;
 factoryFound=false;assert(run(16,0x53B0,0)==-9);factoryFound=true;
 sh->flags=0;assert(run(16,0x53B0,0)==-4);
 std::cout<<"PASS: shipped ProduceDeferred branch accepts busy native unit factories; buildings still require empty factory; native eligibility gates remain\n";
}
'''
with tempfile.TemporaryDirectory() as tmp:
 cpp=Path(tmp)/'runtime.cpp';exe=Path(tmp)/'runtime';cpp.write_text(body)
 subprocess.run(['g++','-std=c++17','-O1','-g','-fsanitize=address,undefined',str(cpp),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
