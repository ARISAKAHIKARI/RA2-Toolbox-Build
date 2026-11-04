#ifdef NDEBUG
#error Assertions required
#endif
#include <cassert>
#include <cstdio>
#include <cstring>
#include <functional>
#include "../source/protocol.h"
#include "../source/native_viewport_host.h"
#include "../../shared/source/overlay_composition.h"
using namespace assist;
static Shared*shared;struct RECT{int left,top,right,bottom;};
static struct{std::function<bool(uint32_t,void*,size_t)>read;bool readFresh(uint32_t at,void*out,size_t n){return read(at,out,n);}}externalReader;
namespace overlaycompositor{struct TestHost{overlaycomposition::Rect mask{};void setMapExclusion(overlaycomposition::Rect value){mask=value;}};static TestHost&host(){static TestHost h;return h;}}
#include "radar_sync_extract.h"
int main(){Shared*s=new Shared{};shared=s;unsigned checks=0;auto check=[&](bool v){++checks;assert(v);};
 int width=168,height=158,screenW=800,screenH=600,screenReads=0;uint8_t side=1;bool fail=false,mutate=false,screenRace=false;
 auto read=[&](uint32_t at,void*out,size_t n){if(fail)return false;if(at==0x7F5BF8&&n==4)memcpy(out,&width,4);else if(at==0x886F94&&n==4)memcpy(out,&height,4);else if(at==0xA8EB7C&&n==1){memcpy(out,&side,1);if(mutate)s->sidebarStatus=2;}else if(at==0x8A00A4&&n==4){int value=screenRace&&++screenReads>1?1024:screenW;memcpy(out,&value,4);}else if(at==0x8A00A8&&n==4)memcpy(out,&screenH,4);else return false;return true;};
 externalReader.read=read;s->sidebarMode=1;s->sidebarStatus=3;
 check(syncRadarExclusion({0,0,1600,1200}));check(overlaycompositor::host().mask==overlaycomposition::Rect{1264,0,336,326});
 screenRace=true;check(!syncRadarExclusion({0,0,1600,1200}));screenRace=false;s->sidebarStatus=0;s->sidebarMode=0;check(syncRadarExclusion({0,0,1600,1200})&&!overlaycompositor::host().mask.w);
 s->sidebarStatus=2;check(!syncRadarExclusion({0,0,1600,1200}));s->sidebarStatus=0;
 nativeviewportprojection::Rect r{};check(nativeviewhost::radarOcclusion(s,read,800,600,r)&&!r.w);
 s->sidebarMode=1;s->sidebarStatus=3;check(nativeviewhost::radarOcclusion(s,read,800,600,r)&&r.x==632&&r.w==168&&r.h==163);
 side=0;check(nativeviewhost::radarOcclusion(s,read,800,600,r)&&r.x==0);side=1;
 fail=true;check(!nativeviewhost::radarOcclusion(s,read,800,600,r));fail=false;
 s->viewZoomBusy=1;check(!nativeviewhost::radarOcclusion(s,read,800,600,r));s->viewZoomBusy=0;
 mutate=true;check(!nativeviewhost::radarOcclusion(s,read,800,600,r));mutate=false;s->sidebarStatus=-2;check(!nativeviewhost::radarOcclusion(s,read,800,600,r));
 using namespace overlaycomposition;Context context{1,1,1,1,8,6,0,0};
 Surface world{1,1,context,{0,0,8,6},true,255,std::make_shared<const std::vector<uint32_t>>(48,0xFF112233u)};
 excludeMapRect(world,{6,0,2,2});check(world.cutouts&&world.cutouts->size()==2);check(valid(world,context));
 std::vector<uint32_t>pixels;check(compose(context,{world},pixels));
 for(int y=0;y<6;++y)for(int x=0;x<8;++x)check(pixels[y*8+x]==(x>=6&&y<2?0u:0xFF112233u));
 Surface ui=world;ui.cameraBound=false;ui.cutouts.reset();excludeMapRect(ui,{6,0,2,2});check(!ui.cutouts);
 world.cutouts=std::make_shared<const std::vector<Rect>>(std::vector<Rect>{{0,5,1,1}});excludeMapRect(world,{6,0,2,2});check(world.cutouts->size()==3);check(compose(context,{world},pixels)&&pixels[40]==0);
 world.rect={4,1,8,6};world.cutouts.reset();excludeMapRect(world,{6,0,2,2});check(world.cutouts->size()==1&&world.cutouts->at(0)==Rect{2,0,2,1});
 delete s;std::printf("{\"passed\":true,\"checks\":%u,\"assertions_active\":true}\n",checks);
}
