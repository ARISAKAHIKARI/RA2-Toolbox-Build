#include "../../external_reader/source/viewport_canvas.h"
#include <cassert>
#include <cstdio>
using namespace external;
int main(){
 lighting::State light;camera::NativeCanvas native;assert(native.reset(256,128));
 voxel::Image ground;ground.width=8;ground.height=4;ground.pixels.assign(32,0xff204060);ground.depth.assign(32,0);native.blit(ground,10,10,1,1,50,light);
 std::vector<uint32_t> base;assert(native.presentRegion(24,12,base,6,6,16,8,3));camera::ViewportCanvas view;
 assert(view.prepare(native,base,24,12,6,6,16,8,1));
 voxel::Image hidden;hidden.width=hidden.height=2;hidden.pixels.assign(4,0xffff0000);hidden.depth.assign(4,0);hidden.writeDepth=false;
 view.blit(hidden,12,11,1,1,49,light);std::vector<uint32_t> image;assert(view.finish(image));assert(image==base); // terrain occludes actor at 1.5x
 view.blit(hidden,12,11,1,1,51,light);assert(view.finish(image));assert(image!=base); // depth units do not scale with physical pixels
 voxel::Model model;voxel::Face face;face.p[0]={0,0,0};face.p[1]={12,0,0};face.p[2]={12,8,0};face.p[3]={0,8,0};face.color=1;model.faces={face};
 std::array<uint32_t,256> colors{};colors[1]=0xff80c0e0;voxel::Image one,two;
 assert(voxel::render(model,.17,colors,one,0,0,nullptr,255));assert(voxel::render(model,.17,colors,two,0,0,nullptr,255,0,false,2));
 assert(two.rasterScale==2&&two.width==one.width*2&&two.height==one.height*2&&two.x==one.x*2&&two.y==one.y*2);
 bool newGeometry=false;for(int y=0;y<two.height-1;y+=2)for(int x=0;x<two.width-1;x+=2){auto at=size_t(y)*two.width+x;newGeometry|=two.pixels[at]!=two.pixels[at+1]||two.pixels[at]!=two.pixels[at+two.width];}assert(newGeometry); // not nearest-neighbor duplicated pixels
 camera::Surface aa;assert(aa.reset(40,40));aa.blit(two,15,15,1,1,100,light,1,0,true);bool fractional=false;
 for(auto p:aa.color)fractional|=(p&0xffffff)!=0x080c08&&(p&0xffffff)!=0x80c0e0;assert(fractional);
 // Native-canvas bounds and physical composition agree on logical VXL size.
 camera::NativeCanvas scaled;assert(scaled.reset(40,40));scaled.blit(two,15,15,1,1,100,light);
 std::vector<uint32_t> scaledPixels;assert(scaled.present(40,40,scaledPixels));assert(std::any_of(scaledPixels.begin(),scaledPixels.end(),[](uint32_t p){return p!=0xff080c08;}));
 assert(native.presentRegion(20,20,base,-4,-4,10,10,2));assert(view.prepare(native,base,20,20,-4,-4,10,10,2));
 hidden.width=hidden.height=8;hidden.pixels.assign(64,0xff00ff00);hidden.depth.clear();view.blit(hidden,-3,-3,1,1,100,light);assert(view.finish(image));
 for(int y=0;y<20;++y)for(int x=0;x<20;++x)if(x<8||y<8)assert(image[y*20+x]==0); // no art in transparent margins
 std::vector<uint32_t> taken;assert(view.finish(taken,true)&&taken==image);assert(!view.finish(image));
 assert(!view.prepare(native,base,100000,20,0,0,10,10,3));assert(!view.finish(image));
 std::puts("PASS: target-pixel 1.5x depth/occlusion, actual 2x VXL geometry and logical anchors, AA coverage, transparent backing margins and bounded allocation");
}
