"""Run the production shroud draw and queue cleanup with controlled native seams.

No engine binaries, network or wall-clock FPS measurements. A fixed low-address
buffer permits executing halfShroud's actual uint32 address arithmetic on Linux.
"""
from pathlib import Path
import argparse, subprocess, tempfile
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--out',type=Path);a=p.parse_args()
here=Path(__file__).resolve().parent
out=a.out or Path(tempfile.mkdtemp(prefix='ra2-stall-'));out.mkdir(parents=True,exist_ok=True)
cpp=r'''
#include <sys/mman.h>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <tuple>
#include <vector>
#include <algorithm>
#include "shroud_raster.h"
#include "private_outlist.h"
#include "single_player_fps.h"
#include "pause_view_policy.h"
using namespace assist;
static unsigned validates=0,sampleReadDepth=0;static bool authorized=true;
namespace gameentry {enum class Client{Assist};static bool active(Client){++validates;return authorized;}}
constexpr uint32_t Memory=0x20000000,Context=Memory+0x100,Point=Memory+0x200,Clip=Memory+0x220,Shape=Memory+0x300,Alpha=Memory+0x400,Pixels=Memory+0x10000,Buffer=Memory+0x20000;
static int shapeBounds[4],firstRow=0;static unsigned helpers=0;
template<class T>static void write(uint32_t p,T v){std::memcpy((void*)(uintptr_t)p,&v,sizeof v);}
template<class T>static T get(uint32_t p){T v{};if(p==0x87E8A4)v=T(Alpha);else if(p==0x89E760)v=T(Shape);else if(p==0x886FA4)v=T(0);else std::memcpy(&v,(void*)(uintptr_t)p,sizeof v);return v;}
// Separate field reads that instantiate the native callback's local structs.
template<class R,class...Args>static R call(uint32_t at,uint32_t,Args...args){
 ++helpers;assert(gameentry::active(gameentry::Client::Assist));
 if constexpr(sizeof...(Args)==2){if constexpr(std::is_pointer_v<std::tuple_element_t<0,std::tuple<Args...>>>){assert(at==0x69E7E0);std::memcpy(std::get<0>(std::tuple<Args...>(args...)),shapeBounds,16);return 0;}return R(Buffer+firstRow*2);}
 else {assert(at==0x69E740);return R(Pixels);}
}
#include "shroud_render_bridge.h"
// Local Rect/Point are aggregates and never use the hardcoded global reads.
int main(){
 void*m=mmap((void*)uintptr_t(Memory),0x40000,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);assert(m==(void*)uintptr_t(Memory));
 unsigned cases=0;
 for(bool fog:{false,true})for(int offset:{0,1,31,32,127,1023})for(int crop:{0,3,17}){
  const int width=32,height=32,clipWidth=width-crop,stride=64,ringPixels=1024;
  write(Context-4,Shape);const uint32_t args=fog?Context+0x30:Context+8;
  write(args,Point);write(args+4,Clip);write(args+8,0);write(Point,10);write(Point+4,20);
  write(Clip,10+crop);write(Clip+4,20);write(Clip+8,clipWidth);write(Clip+12,height);
  shapeBounds[0]=shapeBounds[1]=0;shapeBounds[2]=width;shapeBounds[3]=height;firstRow=offset;
  write(Alpha+0x1C,Buffer+ringPixels*2);write(Alpha+0x20,uint32_t(ringPixels*2));write(Alpha+0x28,stride);
  std::vector<uint16_t>expected(ringPixels);for(int i=0;i<ringPixels;++i){write(Pixels+i,uint8_t((i*73+cases*17)&255));expected[i]=uint16_t((i*13+19)%256);write(Buffer+i*2,expected[i]);}
  for(int y=0;y<height;++y)for(int x=0;x<clipWidth;++x){const auto mask=get<uint8_t>(Pixels+y*width+crop+x);auto&v=expected[(offset+y*stride+x)%ringPixels];if(mask<=127){const uint16_t n=uint16_t((unsigned(mask)+128)/2);v=fog?std::min(v,n):n;}}
  validates=helpers=0;assert(halfShroud(Context,fog));assert(helpers==3&&validates==4);
  assert(!std::memcmp(expected.data(),(void*)uintptr_t(Buffer),ringPixels*2));++cases;
 }
 validates=helpers=0;authorized=false;assert(!halfShroud(Context,false)&&validates==1&&!helpers);authorized=true;
 sampleReadDepth=1;assert(!halfShroud(Context,false));sampleReadDepth=0;
 // Worst-case 4096 expired records drain in bounded frames, preserving the
 // first live record and all later records, including across ring wrap.
 static privateout::Queue q;privateout::Pending item;
 for(unsigned i=0;i<privateout::Capacity;++i){item.stamp.epoch=i;assert(q.push(item));}
 for(unsigned i=0;i<3000;++i)assert(q.pop());for(unsigned i=4096;i<7096;++i){item.stamp.epoch=i;assert(q.push(item));}
 unsigned total=0,calls=0;
 for(unsigned frame=0;q.at(0).stamp.epoch<7090&&frame<40;++frame){unsigned budget=128,seen=0;auto valid=[&](const auto&p){++seen;return p.stamp.epoch>=7090;};total+=privateout::prune(q,budget,valid);assert(seen<=128);calls+=seen;const auto left=q.count;assert(!privateout::prune(q,budget,valid)&&q.count==left);}
 assert(total==4090&&q.count==6&&q.at(0).stamp.epoch==7090&&calls<4200);
 for(unsigned i=0;i<6;++i){assert(q.at(0).stamp.epoch==7090+i);assert(q.pop());}
 for(int mode:{1,2,3,4,6,7,8})assert(!singlefps::session(mode)&&!pauseview::singleplayer(mode));
 assert(singlefps::session(0)&&singlefps::session(5));
 assert(!munmap(m,0x40000));
 printf("PASS game-thread stalls: %u actual shroud draws with clipping/wrap/sentinel parity; 4 lease checks per draw; 4090 cancellations bounded to 128 inspections per frame; multiplayer cannot enter SP waits\n",cases);
}
'''
# The aggregate get<T> requires memcpy for the special global scalar reads too.
cpp=cpp.replace('if(p==0x87E8A4)v=T(Alpha);else if(p==0x89E760)v=T(Shape);else if(p==0x886FA4)v=T(0);else std::memcpy(&v,(void*)(uintptr_t)p,sizeof v);','uint32_t scalar=p==0x87E8A4?Alpha:p==0x89E760?Shape:0;if(p==0x87E8A4||p==0x89E760||p==0x886FA4)std::memcpy(&v,&scalar,std::min(sizeof v,sizeof scalar));else std::memcpy(&v,(void*)(uintptr_t)p,sizeof v);')
(out/'stalls.cpp').write_text(cpp)
commands=[['g++','-std=c++17','-O2','-UNDEBUG','-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(here.parent/'source'),str(out/'stalls.cpp'),'-o',str(out/'stalls')],[str(out/'stalls')]]
for command in commands:
 r=subprocess.run(command,capture_output=True,text=True,timeout=60);print(r.stdout+r.stderr,end='');r.check_returncode()
