#pragma once
#include "voxel_preview.h"
#include "scene_lighting.h"
#include "scene_order.h"
namespace external { namespace camera {
struct Light {
 float gains[3]{};explicit Light(const lighting::State&state){
  int brightness=std::clamp(state.intensity<0?state.ambient*10:state.intensity,0,2000);
  int shade=std::min(254,(261*brightness)>>11),level=std::min(62,127*shade*62/32258);
  for(int c=0;c<3;++c)gains[c]=level/31.f*std::clamp(state.tint[c],0,2000)/1000.f;
 }
 uint32_t apply(uint32_t pixel,uint8_t index,bool object)const{
  if(object&&index>=240&&index<=254)return pixel;uint32_t result=pixel&0xff000000u;unsigned alpha=pixel>>24;
  for(int c=0;c<3;++c){int shift=16-c*8;result|=unsigned(std::clamp(((pixel>>shift)&255)*gains[c],0.f,float(alpha)))<<shift;}return result;
 }
};
inline uint32_t lit(uint32_t pixel,uint8_t index,bool object,const lighting::State&state){return Light(state).apply(pixel,index,object);}
// Presentation-only opacity. Shared camera poses stay opaque; translucent
// previews retain premultiplied RGB and the same indexed lighting rules.
inline void previewPixels(const voxel::Image&image,const lighting::State&state,unsigned opacity,std::vector<uint32_t>&out){
 Light light(state);opacity=std::min(255u,opacity);out.resize(image.pixels.size());bool indexed=image.indices.size()==out.size();
 for(size_t i=0;i<out.size();++i){uint32_t p=light.apply(image.pixels[i],indexed?image.indices[i]:0,image.objectPalette),v=0;
  for(int shift:{0,8,16,24})v|=((((p>>shift)&255)*opacity+127)/255)<<shift;out[i]=v;}
}
inline size_t pixels(const voxel::Image&i){size_t n=i.pixels.size();for(const auto&part:i.attachments)n+=pixels(part);return n;}
inline void opaque(voxel::Image&i){
 for(auto&p:i.pixels)if(unsigned a=p>>24)p=0xff000000u|(std::min(255u,((p>>16)&255)*255/a)<<16)|(std::min(255u,((p>>8)&255)*255/a)<<8)|std::min(255u,(p&255)*255/a);
 for(auto&part:i.attachments)if(!part.shadow)opaque(part);
}
class Surface {
 int width_=0,height_=0;
public:
 std::vector<uint32_t> color;std::vector<float> depth;
 bool reset(int w,int h,uint32_t background=0xff080c08u){
  if(w<=0||h<=0||w>2048||h>2048){width_=height_=0;color.clear();depth.clear();return false;}
  width_=w;height_=h;color.assign(size_t(w)*h,background);depth.assign(color.size(),-std::numeric_limits<float>::infinity());return true;
 }
 int width()const{return width_;}int height()const{return height_;}
 void diamond(float ax,float ay,float sx,float sy,float base,uint32_t c,const lighting::State&light){
  c=lit(c,0,false,light);
  for(int y=std::max(0,int(std::floor(ay-15*sy)));y<std::min(height_,int(std::ceil(ay+15*sy)));++y)
   for(int x=std::max(0,int(std::floor(ax-30*sx)));x<std::min(width_,int(std::ceil(ax+30*sx)));++x){
    float dx=(x+.5f-ax)/(30*sx),dy=(y+.5f-ay)/(15*sy);if(std::abs(dx)+std::abs(dy)>1)continue;
    size_t at=size_t(y)*width_+x;float z=base+(y+.5f-ay)/sy-14;if(z>=depth[at]){color[at]=c;depth[at]=z;}
   }
 }
 void blit(const voxel::Image&i,float ax,float ay,float sx,float sy,float base,const lighting::State&light,float lift=1,float shadowDrop=0){
  if(i.width<=0||i.height<=0||i.pixels.size()!=size_t(i.width)*i.height||!std::isfinite(ax)||!std::isfinite(ay)||!std::isfinite(sx)||!std::isfinite(sy)||sx<=0||sy<=0||sx>8||sy>8)return;
  for(const auto&part:i.attachments)if(part.shadow&&!i.shadowsAfter)blit(part,ax,ay+shadowDrop*sy,sx,sy,base,light,lift);
  float left=ax+i.x*sx,top=ay+i.y*sy;
  int x0=std::clamp(int(std::floor(std::clamp(left,-1e7f,1e7f))),0,width_),y0=std::clamp(int(std::floor(std::clamp(top,-1e7f,1e7f))),0,height_);
  int x1=std::clamp(int(std::ceil(std::clamp(left+i.width*sx,-1e7f,1e7f))),0,width_),y1=std::clamp(int(std::ceil(std::clamp(top+i.height*sy,-1e7f,1e7f))),0,height_);
  Light transform(light);bool zData=i.depth.size()==i.pixels.size(),indexed=i.indices.size()==i.pixels.size();
  for(int y=y0;y<y1;++y){int sourceY=int(std::floor((y+.5f-top)/sy));if(sourceY<0||sourceY>=i.height)continue;
   for(int x=x0;x<x1;++x){int sourceX=int(std::floor((x+.5f-left)/sx));if(sourceX<0||sourceX>=i.width)continue;
    size_t from=size_t(sourceY)*i.width+sourceX,to=size_t(y)*width_+x;uint32_t p=i.pixels[from];if(!(p>>24))continue;
    float z=base+i.depthBias;bool useZ=true;
    if(zData){if(std::isnan(i.depth[from]))useZ=false;else z+=i.depth[from];}
    else if(i.flat)z+=i.y+sourceY-14+lift;
    else{int bottom=i.depthBottom==std::numeric_limits<int>::min()?i.y+i.height-1:i.depthBottom;z+=bottom-14+lift+(bottom-i.y-sourceY)/3;}
    if(useZ&&z<=depth[to])continue;
    if(i.shadow){uint32_t old=color[to];color[to]=(old&0xff000000u)|((old&0xfefefeu)>>1);}
    else{p=transform.apply(p,indexed?i.indices[from]:0,i.objectPalette);unsigned a=p>>24;
     if(a==255)color[to]=p;else{uint32_t old=color[to],out=0xff000000u;for(int shift:{0,8,16})out|=std::min(255u,((p>>shift)&255)+((old>>shift)&255)*(255-a)/255)<<shift;color[to]=out;}
    }
    // Building/terrain shadows write their ground plane. Without this, each
    // overlapping shadow halves the same pixel again and quickly turns black.
    if(useZ&&i.writeDepth)depth[to]=z;
   }
  }
  for(const auto&part:i.attachments)if(!part.shadow||i.shadowsAfter)blit(part,ax,ay+(part.shadow?shadowDrop*sy:0),sx,sy,base,light,lift);
 }
};
}}
