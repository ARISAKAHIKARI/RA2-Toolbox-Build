#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../../external_reader/source/gpu_area_residency.h"
#include <cassert>
#include <queue>
#include <random>
#include <set>
#include <cstdio>
using namespace external;
// Independent, deliberately slow pixel-by-pixel read graph. Production
// visits page spans; this oracle follows every shader source coordinate.
static std::vector<size_t> oracle(const gpuarea::Plan&p,const std::vector<size_t>&dirty,const std::vector<uint64_t>&removed){
 const int cols=(p.nativeWidth+127)/128,rows=(p.nativeHeight+127)/128;std::vector<std::set<int>>edges(size_t(cols)*rows);
 for(const auto&d:*p.objects)if(d.displace){auto b=vulkancompute::drawBounds(d,p.nativeWidth,p.nativeHeight);
  for(int y=b[1];y<b[3];++y)for(int x=b[0];x<b[2];++x){int64_t from=int64_t(y)*p.nativeWidth+x+d.cloakWords;if(from<0||from>=int64_t(p.nativeWidth)*p.nativeHeight)continue;
   int source=int(from/p.nativeWidth)/128*cols+int(from%p.nativeWidth)/128,dest=y/128*cols+x/128;edges[source].insert(dest);edges[dest].insert(source);
  }
 }
 std::set<int>seen;std::queue<int>todo;auto seed=[&](int x){if(seen.insert(x).second)todo.push(x);};for(size_t n:dirty)seed(p.pages[n].y*cols+p.pages[n].x);
 for(auto key:removed){auto x=uint32_t(key>>32),y=uint32_t(key);if(x<uint32_t(cols)&&y<uint32_t(rows))seed(y*cols+x);}
 while(!todo.empty()){int a=todo.front();todo.pop();for(int b:edges[a])seed(b);}
 std::vector<size_t>out;for(size_t n=0;n<p.pages.size();++n)if(seen.count(p.pages[n].y*cols+p.pages[n].x))out.push_back(n);return out;
}
int main(){
 std::mt19937 rng(20261105);size_t cases=0;
 for(int round=0;round<300;++round){gpuarea::Plan p;p.nativeWidth=257+rng()%200;p.nativeHeight=129+rng()%170;p.nativeSurface=true;
  auto draws=std::make_shared<std::vector<vulkancompute::DrawJob>>();for(int n=0;n<4;++n){vulkancompute::DrawJob d;d.left=float(rng()%p.nativeWidth)+.25f;d.top=float(rng()%p.nativeHeight)+.75f;d.scaleX=.5f+float(rng()%5)/2;d.scaleY=1;
   d.source.width=1+rng()%55;d.source.height=1+rng()%45;d.displace=true;d.nativePitch=p.nativeWidth;d.cloakWords=int(rng()%(p.nativeWidth*160))-p.nativeWidth*80;draws->push_back(d);
  }p.objects=draws;
  std::vector<uint64_t>removed;for(int y=0;y<(p.nativeHeight+127)/128;++y)for(int x=0;x<(p.nativeWidth+127)/128;++x){if(rng()%5)p.pages.push_back({uint32_t(x),uint32_t(y)});else removed.push_back(gpuarea::pageKey(x,y));}
  if(p.pages.empty())continue;std::vector<size_t>dirty;for(size_t n=0;n<p.pages.size();++n)if(rng()%5==0)dirty.push_back(n);
  auto expected=oracle(p,dirty,removed),actual=dirty;gpuarea::expandNativeDirty(p,actual,removed);assert(actual==expected);++cases;
  auto scaled=*draws;scaled[0].nativeScaleX=2;p.objects=std::make_shared<const std::vector<vulkancompute::DrawJob>>(scaled);actual.clear();auto state=gpuarea::expandNativeDirty(p,actual,{});assert(state.full&&actual.size()==p.pages.size());
  scaled[0].nativeScaleX=1;scaled[0].left=std::numeric_limits<float>::quiet_NaN();p.objects=std::make_shared<const std::vector<vulkancompute::DrawJob>>(scaled);actual.clear();state=gpuarea::expandNativeDirty(p,actual,{});assert(state.full&&actual.size()==p.pages.size());
 }
 std::printf("PASS GPU native page dependency closure: %zu randomized pixel-graph comparisons; signed/wrapped/missing source pages, transitive reads, conservative scale/invalid fallback\n",cases);
}
