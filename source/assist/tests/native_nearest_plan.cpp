#include "../../external_reader/source/native_canvas.h"
#include <cassert>
#include <iostream>
int main(){using namespace external;camera::NativeCanvas canvas;assert(canvas.reset(391,277));lighting::State light;
 voxel::Image image;image.width=391;image.height=277;image.pixels.resize(391*277);image.flat=true;
 for(size_t i=0;i<image.pixels.size();++i)image.pixels[i]=0xff000000u|uint32_t((i*137)%0x1000000);
 canvas.blit(image,0,0,1,1,0,light);size_t checked=0;
 for(double ratio:{1.,1.5,2.,.75})for(double x:{-9.25,0.,37.125,350.5})for(double y:{-3.5,0.,89.75,250.25}){
  int w=int(190*ratio),h=int(150*ratio);std::vector<uint32_t>expected;assert(canvas.presentRegion(w,h,expected,x,y,190,150,1));
  std::vector<vulkancompute::TileJob>jobs;bool available=canvas.gpuRegion(w,h,x,y,190,150,1,jobs);
  const bool nearest=ratio>1||(ratio==1&&std::floor(x)==x&&std::floor(y)==y);assert(available==nearest);if(!available)continue;
  std::vector<uint32_t>actual(size_t(w)*h,0xff080c08u);std::vector<bool>seen(actual.size(),false);
  for(const auto&job:jobs){assert(job.nearest&&!job.reconstructionFactor);
   for(int yy=0;yy<job.height;++yy)for(int xx=0;xx<job.width;++xx){int sx=std::clamp(int((*job.nearestXs)[job.left+xx]+job.sourceX),0,job.sourceWidth-1),sy=std::clamp(int((*job.nearestYs)[job.top+yy]+job.sourceY),0,job.sourceHeight-1);
    size_t at=size_t(yy+job.top)*w+xx+job.left;assert(!seen[at]);seen[at]=true;actual[at]=(*job.pixels)[size_t(sy)*job.sourceWidth+sx];}
  }
  // Final GPU scene clipping uses native pixel centers, just as CPU output.
  for(int yy=0;yy<h;++yy)for(int xx=0;xx<w;++xx){double px=x+(xx+.5)*190/w,py=y+(yy+.5)*150/h;if(px<0||py<0||px>=391||py>=277)actual[size_t(yy)*w+xx]=0;}
  if(actual!=expected){for(size_t i=0;i<actual.size();++i)if(actual[i]!=expected[i]){std::cerr<<"nearest mismatch ratio="<<ratio<<" crop="<<x<<","<<y<<" pixel="<<i%w<<","<<i/w<<" actual="<<std::hex<<actual[i]<<" expected="<<expected[i]<<std::dec<<"\n";break;}}assert(actual==expected);checked+=actual.size();
 }
 std::cout<<"PASS basic GPU nearest terrain plan: 64 admitted/fallback crops, "<<checked<<" exact pixels, disjoint native pages, no xBRZ, fractional scale/borders\n";
}
