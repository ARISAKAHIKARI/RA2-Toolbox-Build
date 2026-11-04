#include "../../external_reader/source/resolution_sampling.h"
#include "../../external_reader/source/camera_raster.h"
#include <cassert>
#include <cstdio>
using namespace external;
int main(){
 using resolutionsampling::choose;
 for(int scale=1;scale<=4;++scale){auto choice=choose(960,540,960*scale,540*scale);assert(choice.valid&&choice.rasterScale==scale&&choice.scaleX==scale&&choice.scaleY==scale);}
 auto fractional=choose(1280,720,1920,1080);assert(fractional.valid&&fractional.rasterScale==2&&fractional.scaleX==1.5&&fractional.scaleY==1.5);
 auto sameBucket=choose(1280,720,1600,900);assert(sameBucket.valid&&sameBucket.rasterScale==fractional.rasterScale);
 assert(choose(1920,1080,1280,720).rasterScale==1);
 assert(choose(1024,768,2560,1440).rasterScale==3); // greatest axis governs geometry density
 assert(choose(8192,4096,8192,4096).valid); // inclusive 32M-pixel allocation bound
 assert(!choose(8192,4097,8192,4097).valid);
 assert(!choose(1024,768,8193,768).valid);
 assert(!choose(1024,768,4097,768).valid); // physical ratio above 4x must fall back
 assert(!choose(0,720,1920,1080).valid&&!choose(1280,-1,1920,1080).valid);
 assert(!choose(1280,720,0,1080).valid&&!choose(1280,720,1920,-1).valid);
 assert(!choose(std::numeric_limits<double>::infinity(),720,1920,1080).valid);
 assert(!choose(1280,720,std::numeric_limits<double>::quiet_NaN(),1080).valid);

 voxel::Model model;voxel::Face face;face.p[0]={0,0,0};face.p[1]={12,0,0};face.p[2]={12,8,0};face.p[3]={0,8,0};face.color=1;model.faces={face};
 std::array<uint32_t,256> colors{};colors[1]=0xff4080a0;voxel::Image native;
 assert(voxel::render(model,.17,colors,native,0,0,nullptr,255));
 lighting::State light;camera::Light transform(light);
 for(int scale:{3,4}){
  voxel::Image image;assert(voxel::render(model,.17,colors,image,0,0,nullptr,255,0,false,scale));
  assert(image.rasterScale==scale&&image.width==native.width*scale&&image.height==native.height*scale);
  assert(image.x==native.x*scale&&image.y==native.y*scale&&image.depthBottom==native.depthBottom*scale);
  assert(image.pixels.size()==native.pixels.size()*scale*scale&&image.depth.size()==image.pixels.size());
  bool newGeometry=false;for(int y=0;y<image.height;y+=scale)for(int x=0;x<image.width;x+=scale){auto origin=image.pixels[size_t(y)*image.width+x];for(int iy=0;iy<scale;++iy)for(int ix=0;ix<scale;++ix)newGeometry|=image.pixels[size_t(y+iy)*image.width+x+ix]!=origin;}assert(newGeometry);
  // At equal physical density each newly rasterized sample lands at its
  // expected pixel, preserving the native logical anchor without another zoom.
  camera::Surface physical;assert(physical.reset(160,120));physical.blit(image,64,48,float(scale),float(scale),100,light,1,0,true);
  size_t drawn=0;for(int y=0;y<image.height;++y)for(int x=0;x<image.width;++x){auto at=size_t(y)*image.width+x;if(!(image.pixels[at]>>24))continue;++drawn;auto target=size_t(48+image.y+y)*physical.width()+64+image.x+x;assert(physical.color[target]==transform.apply(image.pixels[at],image.indices[at],true));}assert(drawn);
  camera::Surface resolve;assert(resolve.reset(80,60));resolve.blit(image,30.25f,25.25f,1.5f,1.5f,100,light,1,0,true);
  assert(std::any_of(resolve.color.begin(),resolve.color.end(),[](uint32_t p){return p!=0xff080c08;}));
  image.shadow=true;image.flat=true;image.writeDepth=false;image.depth.clear();camera::Surface shadow;assert(shadow.reset(80,60));shadow.blit(image,30.25f,25.25f,1.5f,1.5f,100,light,1,0,true);
  assert(std::any_of(shadow.color.begin(),shadow.color.end(),[](uint32_t p){return p!=0xff080c08;}));
 }
 voxel::Image rejected;assert(!voxel::render(model,.17,colors,rejected,0,0,nullptr,255,0,false,0));assert(!voxel::render(model,.17,colors,rejected,0,0,nullptr,255,0,false,5));
 // External images receive the same allocation/scale validation as generated ones.
 camera::Surface guarded;assert(guarded.reset(8,8));voxel::Image invalid;invalid.width=invalid.height=1;invalid.pixels={0xffffffff};invalid.rasterScale=5;guarded.blit(invalid,0,0,1,1,0,light);assert(guarded.color[0]==0xff080c08);
 invalid.rasterScale=1;invalid.width=invalid.height=65536;guarded.blit(invalid,0,0,1,1,0,light);assert(guarded.color[0]==0xff080c08);
 std::puts("PASS: observed resolution ratios choose bounded 1..4x geometry; true 3x/4x samples, logical anchors, physical placement, resolve/shadows and invalid-geometry guards");
}
