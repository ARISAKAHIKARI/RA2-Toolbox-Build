#include "../../external_reader/source/radar_map.h"
#include <cassert>
#include <cstdio>
using namespace external::radar;
static std::shared_ptr<Terrain> bank(){
 auto t=std::make_shared<Terrain>();t->geo={16,16,16,16,32,32,1,0x10000,0x20000,512*512,{16,16,0,0,16,16},32,32};t->reveal=true;t->colors.resize(512*512);
 for(int y=1;y<512;++y)for(int x=1;x<512;++x)if(t->geo.legal(x,y))t->colors[size_t(y)*512+x]=uint32_t(0x204020+(x*17+y*13)%80*0x010101);
 return t;
}
int main(){
 DiagramRenderer renderer;auto first=bank();assert(first->geo.valid());std::weak_ptr<Terrain> old=first;auto image=renderer.render(first,300,150);assert(image&&image->pixels.size()==45000);
 std::vector<uint32_t> expected;raster(*first,300,150,expected);assert(image->pixels==expected);assert(renderer.render(first,300,150)==image);
 auto same=std::make_shared<Terrain>(*first);first.reset();assert(!old.expired());assert(renderer.render(same,300,150)==image);assert(old.expired());
 // Native source art and observation wrappers do not affect the diagram RGB.
 same=std::make_shared<Terrain>(*same);same->art.resize(512*512);same->art[20*512+18].sub=7;same->shade.resize(512*512);assert(renderer.render(same,300,150)==image);
 auto changed=std::make_shared<Terrain>(*same);changed->colors[20*512+18]^=0xf0e0c0;auto updated=renderer.render(changed,300,150);assert(updated!=image&&updated->pixels!=image->pixels);raster(*changed,300,150,expected);assert(updated->pixels==expected);
 auto moved=std::make_shared<Terrain>(*changed);++moved->geo.offsetX;auto shifted=renderer.render(moved,300,150);assert(shifted!=updated);raster(*moved,300,150,expected);assert(shifted->pixels==expected);
 auto hidden=std::make_shared<Terrain>(*moved);hidden->reveal=false;auto visibility=renderer.render(hidden,300,150);assert(visibility!=shifted&&visibility->pixels==shifted->pixels);
 auto resized=renderer.render(hidden,301,151);assert(resized!=visibility);raster(*hidden,301,151,expected);assert(resized->pixels==expected);assert(!renderer.render({},301,151));assert(renderer.render(hidden,301,151)==resized);
 renderer.reset();assert(renderer.render(hidden,301,151)!=resized);
 std::puts("PASS diagram cache: identical sampled RGB/geometry retains immutable output, releases old bank, ignores non-diagram art, exact fresh-raster oracle for color/geometry/reveal/size changes and reset");
}
