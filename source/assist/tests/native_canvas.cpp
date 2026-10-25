#include "../../external_reader/source/native_canvas.h"
#include <cassert>
#include <iostream>
using namespace external;
static uint32_t reference(const camera::Surface&s,double x0,double y0,double x1,double y1){
 double sums[3]{},area=(x1-x0)*(y1-y0);
 for(int y=int(std::floor(y0));y<int(std::ceil(y1));++y)for(int x=int(std::floor(x0));x<int(std::ceil(x1));++x){
  double w=std::max(0.,std::min(x1,double(x+1))-std::max(x0,double(x)))*std::max(0.,std::min(y1,double(y+1))-std::max(y0,double(y)));
  uint32_t c=s.color[size_t(y)*s.width()+x];for(int ch=0;ch<3;++ch)sums[ch]+=w*((c>>(16-ch*8))&255);
 }
 uint32_t out=0xff000000;for(int ch=0;ch<3;++ch)out|=uint32_t(std::floor(sums[ch]/area+.5))<<(16-ch*8);return out;
}
int main(){
 camera::NativeCanvas native;camera::Surface flat;assert(native.reset(315,170)&&flat.reset(315,170));lighting::State light;
 voxel::Image tile;tile.width=275;tile.height=121;tile.x=-32;tile.y=-24;tile.pixels.resize(tile.width*tile.height);tile.depth.resize(tile.pixels.size());
 for(int y=0;y<tile.height;++y)for(int x=0;x<tile.width;++x){auto n=size_t(y)*tile.width+x;tile.pixels[n]=0xff000000u|uint32_t((x*13)&255)<<16|uint32_t((y*17)&255)<<8|uint32_t((x+y)&255);tile.depth[n]=float(y%7);}
 flat.blit(tile,35.25f,40.75f,1,1,100,light);native.blit(tile,35.25f,40.75f,1,1,100,light);
 voxel::Image shadow;shadow.width=180;shadow.height=17;shadow.x=-20;shadow.y=-4;shadow.pixels.assign(3060,0xffffffff);shadow.depth.assign(3060,200);shadow.shadow=true;shadow.writeDepth=true;
 flat.blit(shadow,120,130,1,1,0,light);native.blit(shadow,120,130,1,1,0,light);
 for(int n=0;n<5;++n){flat.blit(shadow,120,130,1,1,0,light);native.blit(shadow,120,130,1,1,0,light);}
 flat.diamond(128,128,1,1,300,0xff804020,light);native.diamond(128,128,1,1,300,0xff804020,light);
 for(int y=0;y<170;++y)for(int x=0;x<315;++x){assert(native.pixel(x,y)==flat.color[size_t(y)*315+x]);assert(native.depth(x,y)==flat.depth[size_t(y)*315+x]);}
 assert(native.pageCount()<=6);native.clean();
 std::vector<uint32_t> filtered;assert(native.present(73,29,filtered,314.25,169.0625));
 for(int y=0;y<29;++y)for(int x=0;x<73;++x)assert(filtered[size_t(y)*73+x]==reference(flat,x*314.25/73,y*169.0625/29,(x+1)*314.25/73,(y+1)*169.0625/29));
 // Only the touched native page is copied; unrelated color/Z pages remain
 // shared. Refiltering its global footprint has no seams at page boundaries.
 auto fork=native.fork();auto before=native.residentPages();voxel::Image dot;dot.width=dot.height=1;dot.pixels={0xffff0000};dot.depth={1000};
 fork.blit(dot,127,127,1,1,0,light);assert(fork.dirtyPages()==1&&fork.residentPages()==before+1);assert(native.pixel(127,127)!=fork.pixel(127,127));
 auto partial=filtered;assert(fork.presentDirty(73,29,partial,314.25,169.0625));std::vector<uint32_t> whole;assert(fork.present(73,29,whole,314.25,169.0625));assert(partial==whole);
 // A large native plane is sparse and still retains unit source pixels.
 camera::NativeCanvas large;assert(large.reset(8192,4096));large.blit(dot,4096,2048,1,1,0,light);assert(large.pageCount()==1&&large.pixel(4096,2048)==0xffff0000);
 assert(large.present(512,256,whole));assert(whole[128*512+256]!=0xff080c08);
 auto budget=std::make_shared<camera::NativeCanvas::Budget>();budget->maximum=3;camera::NativeCanvas bounded;assert(bounded.reset(1024,1024,budget));
 voxel::Image huge;huge.width=huge.height=600;huge.pixels.assign(600*600,0xffffffff);bounded.blit(huge,0,0,1,1,0,light);assert(bounded.failed()&&budget->pages<=3&&!bounded.present(32,32,whole));assert(bounded.pixel(0,0)==0xff080c08); // no partial scaled replacement
 bounded={};assert(budget->pages==0);assert(!large.reset(32769,1));
 std::cout<<"PASS native 1:1 color/depth/shadow equivalence across page seams, exact fractional-viewport area filter, copy-on-write dirty presentation, large sparse planes and bounded failure\n";
}
