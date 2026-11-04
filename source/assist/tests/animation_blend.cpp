#include "../../external_reader/source/animation_blend.h"
#include "../../external_reader/source/camera_raster.h"
#include <cassert>
#include <iostream>
int main(){
 using namespace external;unsigned flags,temporal,thick,fading,level;int detail,threshold,value,end,translucency;
 if(std::cin.peek()!=std::char_traits<char>::eof()){
  while(std::cin>>flags>>temporal>>thick>>detail>>threshold>>fading>>value>>end>>translucency>>level)
   std::cout<<animations::blend(flags,temporal,thick,detail,threshold,fading,value,end,translucency,level)<<'\n';
  return 0;
 }
 assert(animations::blend(0,false,false,2,0,true,3,5,0,0)==3);
 assert(animations::blend(0,false,false,2,0,true,2,5,0,0)==1);
 assert(animations::blend(0,false,false,2,0,true,1,5,0,0)==0);
 assert(animations::blend(0,false,false,2,0,false,0,10,75,0)==3);
 assert(animations::blend(0,false,false,0,2,false,0,10,75,15)==0);
 assert(animations::blend(0,false,false,2,0,false,0,10,25,15)==-1);
 assert(animations::blend(0,true,false,2,0,false,0,10,0,0)==2);
 voxel::Image image;image.width=image.height=1;image.pixels={0x80804020};image.intrinsicAlpha=true;camera::opaque(image);assert(image.pixels[0]==0x80804020);
 camera::Surface canvas;assert(canvas.reset(1,1));canvas.color[0]=0xff204060;image.writeDepth=false;image.flat=true;lighting::State light;canvas.blit(image,0,0,1,1,100,light);
 assert(canvas.color[0]==0xff8f5f4f); // source over keeps the actual background
 image.intrinsicAlpha=false;camera::opaque(image);assert(image.pixels[0]==0xffff7f3f);
 std::cout<<"PASS native animation fade boundaries, temporal/quality/type flags and intrinsic premultiplied alpha survives camera preparation\n";
}
