#pragma once
// Test-only marshaling oracle. xBRZ edge rules come exclusively from the
// unmodified upstream implementation; no second edge heuristic lives here.
#include "../../third_party/xbrz/xbrz.h"
#include <algorithm>
#include <cstdint>
#include <memory>
#include <random>
#include <string>
#include <utility>
#include <vector>
namespace external { namespace gpuxbrztest {
inline uint32_t straight(uint32_t pixel){
 const unsigned alpha=pixel>>24;if(!alpha)return 0;if(alpha==255)return pixel;
 uint32_t result=alpha<<24;
 for(unsigned shift:{0u,8u,16u})result|=std::min(255u,(((pixel>>shift)&255)*255+alpha/2)/alpha)<<shift;
 return result;
}
inline uint32_t premultiplied(uint32_t pixel){
 const unsigned alpha=pixel>>24;uint32_t result=alpha<<24;
 for(unsigned shift:{0u,8u,16u})result|=((((pixel>>shift)&255)*alpha+127)/255)<<shift;
 return result;
}
inline std::vector<uint32_t> upstream(const std::vector<uint32_t>& source,int width,int height,int factor){
 if(width<1||height<1||source.size()!=size_t(width)*height||factor<2||factor>4)return {};
 std::vector<uint32_t> input(source.size()),output(size_t(width)*height*factor*factor);
 std::transform(source.begin(),source.end(),input.begin(),straight);
 xbrz::scale(size_t(factor),input.data(),output.data(),width,height,xbrz::ColorFormat::argbUnbuffered);
 std::transform(output.begin(),output.end(),output.begin(),premultiplied);
 return output;
}
struct Case {std::string label;int width=0,height=0;std::shared_ptr<const std::vector<uint32_t>> source;};
inline uint32_t pixel(unsigned alpha,unsigned red,unsigned green,unsigned blue){
 return (alpha<<24)|((red*alpha/255)<<16)|((green*alpha/255)<<8)|(blue*alpha/255);
}
inline void add(std::vector<Case>& cases,std::string label,int width,int height,std::vector<uint32_t> source){
 cases.push_back({std::move(label),width,height,std::make_shared<const std::vector<uint32_t>>(std::move(source))});
}
inline std::vector<uint32_t> rotate(const std::vector<uint32_t>& source,int width,int height){
 std::vector<uint32_t> output(source.size());
 for(int y=0;y<height;++y)for(int x=0;x<width;++x)output[size_t(x)*height+(height-1-y)]=source[size_t(y)*width+x];
 return output;
}
inline std::vector<Case> cases(){
 std::vector<Case> output;std::mt19937 random(0x19261029);
 const std::vector<uint32_t> palette={0,0xff000000,0xffffffff,0xffff0000,0xff00ff00,0xff0000ff,0xff777777,0xff787878,pixel(1,255,127,0),pixel(127,71,211,131),pixel(254,3,249,111),pixel(63,255,255,255)};
 for(int alpha:{0,1,2,63,127,128,254,255}){
  add(output,"single-alpha-"+std::to_string(alpha),1,1,{pixel(alpha,211,29,157)});
  add(output,"horizontal-alpha-"+std::to_string(alpha),7,1,{0,pixel(alpha,255,0,0),0,pixel(alpha,0,255,0),0,pixel(alpha,0,0,255),0});
  add(output,"vertical-alpha-"+std::to_string(alpha),1,7,{0,pixel(alpha,255,0,0),0,pixel(alpha,0,255,0),0,pixel(alpha,0,0,255),0});
 }
 for(int mode=0;mode<9;++mode){
  int width=17,height=13;std::vector<uint32_t> image(size_t(width)*height);
  for(int y=0;y<height;++y)for(int x=0;x<width;++x){uint32_t v=0;
   switch(mode){
    case 0:v=((x+y)&1)?0xff000000:0xffffffff;break;
    case 1:v=(x==y||x==y+1)?0xffd34271:0;break;
    case 2:v=(x==width/2||y==height/2)?0xffeec031:0;break;
    case 3:v=(x==width/2&&y==height/2)?0xffffffff:0;break;
    case 4:v=(x>2&&x<14&&y>2&&y<10)?0xff20a3e1:0xffdf2031;if((x==7||x==8)&&y==5)v=0xff000000;break;
    case 5:v=(x>4&&x<12&&y>3&&y<9&&!(x>6&&x<10&&y>4&&y<8))?0xff8787ff:0;break;
    case 6:v=palette[(unsigned(x/2+y/3))%palette.size()];break;
    case 7:v=pixel(unsigned((x*13+y*7)&255),unsigned((x*19+y*3)&255),unsigned((x*5-y*11)&255),unsigned((x*3+y*17)&255));break;
    case 8:v=(x<width/2)?0xff777777:0xff787878;break;
   }image[size_t(y)*width+x]=v;
  }
  for(int turn=0;turn<4;++turn){add(output,"pattern-"+std::to_string(mode)+"-rotation-"+std::to_string(turn),width,height,image);image=rotate(image,width,height);std::swap(width,height);}
 }
 // Small random palettes provoke equal/near-equal distances and rotations;
 // unrestricted premultiplied colors stress alpha-weighted integer gradients.
 for(int n=0;n<128;++n){
  int width=2+int(random()%30),height=2+int(random()%26);std::vector<uint32_t> image(size_t(width)*height);
  for(auto& value:image){
   if(n<96)value=palette[random()%palette.size()];
   else{unsigned alpha=random()%256;value=pixel(alpha,random()%256,random()%256,random()%256);}
  }
  add(output,"random-"+std::to_string(n),width,height,std::move(image));
 }
 // Actual RegionCache source tile dimensions include a three-pixel halo.
 for(int mode=0;mode<2;++mode){const int width=262,height=262;std::vector<uint32_t> image(size_t(width)*height);
  for(int y=0;y<height;++y)for(int x=0;x<width;++x)image[size_t(y)*width+x]=mode?palette[unsigned(x/7+y/11)%palette.size()]:((x-y)%17==0?0xffe83311u:0xff224366u);
  add(output,"full-tile-halo-"+std::to_string(mode),width,height,std::move(image));
 }
 return output;
}
}}
