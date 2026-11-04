#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../source/watch_observation.h"
#include <cassert>
#include <cstdio>
using namespace external;
static double now(){return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();}
int main(){
 auto terrain=std::make_shared<radar::Terrain>();terrain->geo.rawWidth=terrain->geo.rawHeight=512;terrain->geo.rw=terrain->geo.rh=512;
 auto first=std::make_shared<radar::Frame>();first->terrain=terrain;first->markerScanValid=true;first->frame=1;first->observedMs=1000;
 for(int n=0;n<4096;++n){radar::Marker m;m.object=0x10000+n*0x800;m.id=n+1;m.kind=0;m.x=n+1000;m.y=n+2000;m.pose.voxel=true;m.pose.rawFacing=true;m.pose.rawBody=uint16_t(n*13);first->markers.push_back(m);}
 watchscene::Observation actual;materialmotion::Timeline oracle;assert(actual.observe(first,1000));oracle.observe(*first,1000);
 auto second=std::make_shared<radar::Frame>(*first);second->frame=2;second->observedMs=1010;for(auto&m:second->markers){m.x+=15;m.pose.rawBody+=90;}
 assert(actual.observe(second,1010));oracle.observe(*second,1010);
 for(int n=0;n<120;++n){double at=1010+n*.5;assert(!actual.observe(second,at));oracle.observe(*second,at);auto a=actual.sampleLatest(at,true,true),b=oracle.sampleLatest(at,true,true);assert(materialmotion::presentationKey(a)==materialmotion::presentationKey(b));}
 // A NEW publication in the same engine frame still updates membership,
 // positions and pose immediately; this is not a frame-number cache.
 auto newer=std::make_shared<radar::Frame>(*second);newer->markers.erase(newer->markers.begin());newer->markers[0].y+=5;
 assert(actual.observe(newer,1020));oracle.observe(*newer,1020);assert(materialmotion::presentationKey(actual.sampleLatest(1021,true,true))==materialmotion::presentationKey(oracle.sampleLatest(1021,true,true)));
 constexpr int repeats=800;double start=now();for(int n=0;n<repeats;++n)oracle.observe(*newer,1020+n);double old=now()-start;
 start=now();unsigned observed=0;for(int n=0;n<repeats;++n)observed+=actual.observe(newer,1020+n);double current=now()-start;assert(!observed);
 // reset does not retain a publication or an interpolation predecessor.
 actual.reset();assert(actual.observe(newer,1200));materialmotion::Timeline reset;reset.observe(*newer,1200);assert(materialmotion::presentationKey(actual.sampleLatest(1200,true,true))==materialmotion::presentationKey(reset.sampleLatest(1200,true,true)));
 std::printf("PASS actual watch Observation: 120 dynamic motion/facing results match Timeline; same-frame replacement and reset preserved. Refeeding %d identical immutable 4095-actor publications: old %.3f ms, current %.6f ms; actual copied observations %u. Sampling/asset validation remains per presentation.\n",repeats,old,current,observed);
}
