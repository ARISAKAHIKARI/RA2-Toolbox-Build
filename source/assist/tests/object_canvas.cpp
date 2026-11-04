#include "../../external_reader/source/object_canvas.h"
#include "gpu_voxel_cases.h"
#include <cassert>
#include <cstdio>
int main(){
 using namespace external;camera::NativeCanvas base;assert(base.reset(1024,768));lighting::State light;
 for(int y=32;y<768;y+=48)for(int x=32;x<1024;x+=60)base.diamond(float(x),float(y),1,1,float(y-100),0xff527698,light);base.clean();
 std::vector<camera::ObjectCanvas::Draw>draws;
 for(unsigned n=0;n<360;++n){auto image=std::make_shared<voxel::Image>();image->width=31;image->height=47;image->x=-15;image->y=-40;image->objectPalette=true;
  image->pixels.resize(31*47);image->indices.resize(31*47);image->writeDepth=n%3==0;image->flat=n%4==0;image->depthBias=float(n%7)-3;
  for(unsigned p=0;p<image->pixels.size();++p){unsigned a=p%5?255:124;image->pixels[p]=(a<<24)|((n*13%(a+1))<<16)|((p*17%(a+1))<<8)|(p*23%(a+1));image->indices[p]=p%7?20:245;}
  auto shadow=*image;shadow.shadow=true;shadow.flat=true;shadow.writeDepth=true;shadow.y+=20;shadow.depthBias=-5;image->attachments.push_back(std::move(shadow));image->shadowsAfter=n%2!=0;
  auto state=light;if(n%5==0){state.intensity=731;state.tint={{1400,700,1200}};}
  draws.push_back({image,float(10+n%30*34)+.25f,float(18+n/30*61)+.75f,float(n%31),2,float(n%5),state});
 }
 camera::ObjectCanvas cache;camera::NativeCanvas output;
 camera::ObjectCanvas delta;camera::NativeCanvas deltaOutput;std::vector<uint32_t>resolved;bool previous=false;
 auto check=[&]{auto reference=base.fork();for(const auto&draw:draws)reference.blit(*draw.image,draw.x,draw.y,1,1,draw.base,draw.light,draw.lift,draw.drop);
  assert(cache.compose(base,draws,output));for(int y=0;y<768;++y)for(int x=0;x<1024;++x){assert(output.pixel(x,y)==reference.pixel(x,y));assert(output.depth(x,y)==reference.depth(x,y));}
  assert(delta.compose(base,draws,deltaOutput,previous));if(!previous)assert(base.present(173,109,resolved,1023.25,767.5));
  assert(deltaOutput.presentDirty(173,109,resolved,1023.25,767.5));std::vector<uint32_t>full;assert(reference.present(173,109,full,1023.25,767.5));assert(resolved==full);previous=true;
 };
 check();assert(cache.stats().changed>=32);auto revision=cache.revision();camera::NativeCanvas old=output;
 check();assert(!cache.stats().changed&&cache.stats().reused>=32&&cache.revision()==revision);
 assert(!deltaOutput.dirtyPages());
 gpuarea::Plan a,b;assert(old.areaPlan(160,120,a)&&output.areaPlan(160,120,b)&&a.pages.size()==b.pages.size());
 for(size_t n=0;n<a.pages.size();++n)assert(a.pages[n].pixels==b.pages[n].pixels&&a.pages[n].version==b.pages[n].version);
 draws[0].x+=128;draws[5].light.intensity=200;draws.erase(draws.begin()+6,draws.begin()+12);check();assert(cache.stats().changed>0&&cache.stats().reused>0);
 std::reverse(draws.begin(),draws.end());check();assert(cache.stats().changed>0);
 voxel::Image dot;dot.width=dot.height=1;dot.pixels={0xff112233};dot.depth={2000};base.blit(dot,511,383,1,1,0,light);previous=false;check();assert(cache.stats().changed>0&&cache.stats().reused>0);
 draws.clear();check();assert(cache.stats().removed>0&&!output.dirtyPages());
 assert(deltaOutput.dirtyPages()>0);check();assert(!deltaOutput.dirtyPages());
 // Resolve each immutable VXL and deferred shadow once, before page workers.
 auto source=std::make_shared<voxel::Image>();auto model=gpuvoxeltest::model();auto vpl=gpuvoxeltest::vpl();std::array<uint32_t,256>palette{};palette.fill(0xff789abc);
 assert(gpuvoxel::image(model,.2,palette,*source,.7,2,&vpl,255));source->depth.clear();source->writeDepth=false;
 auto shadow=*source;shadow.shadow=true;shadow.flat=true;source->attachments.push_back(std::move(shadow));
 for(unsigned n=0;n<96;++n)draws.push_back({source,float(126+n%12*72),float(126+n/12*72),100,2,3,light});
 check();assert(cache.stats().rasterBuilds==1&&cache.stats().rasterHits==95);check();assert(!cache.stats().rasterBuilds&&cache.stats().rasterHits==96&&!cache.stats().changed);
 auto budget=std::make_shared<camera::NativeCanvas::Budget>();budget->maximum=3;camera::NativeCanvas bounded,rejected;assert(bounded.reset(1024,768,budget));camera::ObjectCanvas small;
 assert(!small.compose(bounded,draws,rejected));draws.resize(1);draws[0].x=30;draws[0].y=30;assert(small.compose(bounded,draws,rejected));
 std::puts("PASS native page batching matches serial RGBA/depth; parallel pages, exact reuse, move/removal/order/light/base invalidation, stable GPU area owners, one VXL/shadow raster for96 objects, bounded failure recovery");
}
