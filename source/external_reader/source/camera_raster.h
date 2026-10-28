#pragma once
#include "voxel_preview.h"
#include "scene_lighting.h"
#include "scene_order.h"
#include "pixel_upgrade.h"
#include "color_precision.h"
#include "xbrz_adapter.h"
namespace external { namespace camera {
struct Light {
 float gains[3]{};colorprecision::Mode precision=colorprecision::Mode::NativeSteps;
 static bool emissive(uint8_t index,bool object){return object&&index>=240&&index<=254;}
 explicit Light(const lighting::State&state,colorprecision::Mode mode=colorprecision::Mode::NativeSteps):precision(mode){
  int brightness=std::clamp(state.intensity<0?state.ambient*10:state.intensity,0,2000);
  for(int c=0;c<3;++c)gains[c]=colorprecision::gain(brightness,state.tint[c],mode);
 }
 // Keep a sample's light contribution unclipped until reconstruction has
 // combined it with its neighbors. Alpha is coverage, not a light channel.
 std::array<double,4> evaluate(uint32_t pixel,uint8_t index,bool object)const{
  const bool fullbright=emissive(index,object);
  return {{double((pixel>>16)&255)*(fullbright?1.:gains[0]),
           double((pixel>>8)&255)*(fullbright?1.:gains[1]),
           double(pixel&255)*(fullbright?1.:gains[2]),double(pixel>>24)}};
 }
 uint32_t apply(uint32_t pixel,uint8_t index,bool object)const{
  if(emissive(index,object))return pixel;uint32_t result=pixel&0xff000000u;unsigned alpha=pixel>>24;
  for(int c=0;c<3;++c){int shift=16-c*8;float value=std::clamp(((pixel>>shift)&255)*gains[c],0.f,float(alpha));
   unsigned channel=precision==colorprecision::Mode::SmoothRGB?unsigned(std::floor(value+.5f)):unsigned(value);result|=channel<<shift;}return result;
 }
};
inline uint32_t lit(uint32_t pixel,uint8_t index,bool object,const lighting::State&state,colorprecision::Mode mode=colorprecision::Mode::NativeSteps){return Light(state,mode).apply(pixel,index,object);}
// Presentation-only opacity. Shared camera poses stay opaque; translucent
// previews retain premultiplied RGB and the same indexed lighting rules.
inline void previewPixels(const voxel::Image&image,const lighting::State&state,unsigned opacity,std::vector<uint32_t>&out,colorprecision::Mode mode=colorprecision::Mode::NativeSteps){
 Light light(state,mode);opacity=std::min(255u,opacity);out.resize(image.pixels.size());bool indexed=image.indices.size()==out.size();
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
 colorprecision::Mode precision_=colorprecision::Mode::NativeSteps;
 xbrzadapter::Cache scaleCache_;bool reconstructionFailed_=false;
public:
 std::vector<uint32_t> color;std::vector<float> depth;
 bool reset(int w,int h,uint32_t background=0xff080c08u){
  if(w<=0||h<=0||w>8192||h>8192||uint64_t(w)*h>16*1024*1024){width_=height_=0;color.clear();depth.clear();return false;}
  width_=w;height_=h;reconstructionFailed_=false;color.assign(size_t(w)*h,background);depth.assign(color.size(),-std::numeric_limits<float>::infinity());return true;
 }
 int width()const{return width_;}int height()const{return height_;}
 void precision(colorprecision::Mode mode){precision_=mode==colorprecision::Mode::SmoothRGB?mode:colorprecision::Mode::NativeSteps;}
 colorprecision::Mode precision()const{return precision_;}
 bool failed()const{return reconstructionFailed_;}
 void diamond(float ax,float ay,float sx,float sy,float base,uint32_t c,const lighting::State&light){
  c=lit(c,0,false,light,precision_);
  for(int y=std::max(0,int(std::floor(ay-15*sy)));y<std::min(height_,int(std::ceil(ay+15*sy)));++y)
   for(int x=std::max(0,int(std::floor(ax-30*sx)));x<std::min(width_,int(std::ceil(ax+30*sx)));++x){
    float dx=(x+.5f-ax)/(30*sx),dy=(y+.5f-ay)/(15*sy);if(std::abs(dx)+std::abs(dy)>1)continue;
    size_t at=size_t(y)*width_+x;float z=base+(y+.5f-ay)/sy-14;if(z>=depth[at]){color[at]=c;depth[at]=z;}
   }
 }
 void blit(const voxel::Image&i,float ax,float ay,float sx,float sy,float base,const lighting::State&light,float lift=1,float shadowDrop=0,bool smooth=false){
  if(i.width<=0||i.height<=0||i.width>resolutionsampling::MaxDimension||i.height>resolutionsampling::MaxDimension||uint64_t(i.width)*i.height>resolutionsampling::MaxPixels||i.rasterScale<1||i.rasterScale>resolutionsampling::MaxRasterScale||i.pixels.size()!=size_t(i.width)*i.height||!std::isfinite(ax)||!std::isfinite(ay)||!std::isfinite(sx)||!std::isfinite(sy)||sx<=0||sy<=0||sx>8||sy>8)return;
  for(const auto&part:i.attachments)if(part.shadow&&!i.shadowsAfter&&(!part.airborneShadow||shadowDrop>0))blit(part,ax,ay+shadowDrop*sy,sx,sy,base,light,lift,0,smooth);
  float pxScale=sx/i.rasterScale,pyScale=sy/i.rasterScale;
  float left=ax+i.x*pxScale,top=ay+i.y*pyScale;
  int x0=std::clamp(int(std::floor(std::clamp(left,-1e7f,1e7f))),0,width_),y0=std::clamp(int(std::floor(std::clamp(top,-1e7f,1e7f))),0,height_);
  int x1=std::clamp(int(std::ceil(std::clamp(left+i.width*pxScale,-1e7f,1e7f))),0,width_),y1=std::clamp(int(std::ceil(std::clamp(top+i.height*pyScale,-1e7f,1e7f))),0,height_);
  Light transform(light,precision_);bool zData=i.depth.size()==i.pixels.size(),indexed=i.indices.size()==i.pixels.size();
  auto colorAt=[&](int x,int y){if(x<0||y<0||x>=i.width||y>=i.height)return uint32_t(0);size_t at=size_t(y)*i.width+x;return transform.apply(i.pixels[at],indexed?i.indices[at]:0,i.objectPalette);};
  // SHP colors enter the unchanged upstream xBRZ scaler AFTER current scene
  // lighting, remap and emissive evaluation. Never bake a fixed light into art.
  // Supersampled VXLs retain their real geometry and coverage resolve below.
  const bool reconstruct=smooth&&!i.shadow&&i.rasterScale==1&&(pxScale>1||pyScale>1)&&x0<x1&&y0<y1;
  const xbrzadapter::ScaledImage* enlarged=nullptr;
  if(reconstruct){int f=xbrzadapter::factor(pxScale,pyScale);std::vector<uint32_t> source(i.pixels.size());
   for(size_t n=0;n<source.size();++n)source[n]=transform.apply(i.pixels[n],indexed?i.indices[n]:0,i.objectPalette);
   enlarged=scaleCache_.get(source,i.width,i.height,f);if(!enlarged)reconstructionFailed_=true;
  }
  for(int y=y0;y<y1;++y){int sourceY=int(std::floor((y+.5f-top)/pyScale));if(sourceY<0||sourceY>=i.height)continue;
   for(int x=x0;x<x1;++x){int sourceX=int(std::floor((x+.5f-left)/pxScale));if(sourceX<0||sourceX>=i.width)continue;
    size_t from=size_t(sourceY)*i.width+sourceX,to=size_t(y)*width_+x;uint32_t p=i.pixels[from];
    std::array<double,4> filtered{};bool preciseSample=false;
    bool sampled=enlarged||(!reconstruct&&smooth&&(!i.shadow||i.rasterScale>1)&&(pxScale!=1||pyScale!=1));
    if(enlarged){
     // Source coverage and Z stay native: a filtered transparent neighbor
     // cannot invent new hit-test/occlusion geometry around a cutout.
     if(!(p>>24))continue;
     p=xbrzadapter::sample(*enlarged,(x+.5-left)/pxScale,(y+.5-top)/pyScale);
    }else if(sampled){
     double fx=(x+.5-left)/pxScale-.5,fy=(y+.5-top)/pyScale-.5;int ix=int(std::floor(fx)),iy=int(std::floor(fy));fx-=ix;fy-=iy;
     const bool resolve=i.rasterScale>1&&pxScale<=1&&pyScale<=1;
     if(precision_==colorprecision::Mode::SmoothRGB&&!i.shadow){
      std::array<uint32_t,4> raw{};std::array<uint8_t,4> indices{};
      for(int n=0;n<4;++n){int xx=ix+(n&1),yy=iy+(n>>1);if(xx>=0&&yy>=0&&xx<i.width&&yy<i.height){size_t at=size_t(yy)*i.width+xx;raw[n]=i.pixels[at];if(indexed)indices[n]=i.indices[at];}}
      auto weights=resolve?pixelupgrade::bilinearWeights(fx,fy):pixelupgrade::adaptiveWeights(raw[0],raw[1],raw[2],raw[3],fx,fy);
      // A SHP's emissive index is a material boundary, even when its source
      // RGB matches an ordinary index. True VXL coverage still resolves by
      // area; each contribution below retains its own lighting semantics.
      if(!resolve){bool first=Light::emissive(indices[0],i.objectPalette);for(int n=1;n<4;++n)if(Light::emissive(indices[n],i.objectPalette)!=first){weights=pixelupgrade::nearestWeights(fx,fy);break;}}
      for(int n=0;n<4;++n)if(weights[n]){auto value=transform.evaluate(raw[n],indices[n],i.objectPalette);for(int c=0;c<4;++c)filtered[c]+=weights[n]*value[c];}
      p=uint32_t(std::clamp(std::floor(filtered[3]+.5),0.,255.))<<24;preciseSample=true;
     }else{
      uint32_t a=colorAt(ix,iy),b=colorAt(ix+1,iy),c=colorAt(ix,iy+1),d=colorAt(ix+1,iy+1);
      if(resolve){
      // Actual supersampled geometry: preserve fractional coverage at its
      // silhouette. The SHP path below keeps cutout boundaries conservative.
      p=0;for(int shift:{0,8,16,24}){double v=(1-fy)*((1-fx)*((a>>shift)&255)+fx*((b>>shift)&255))+fy*((1-fx)*((c>>shift)&255)+fx*((d>>shift)&255));p|=unsigned(std::lround(v))<<shift;}
      }else p=pixelupgrade::adaptivePixel(a,b,c,d,fx,fy);
     }
    }
    if(!(p>>24))continue;
    float z=base+i.depthBias;bool useZ=true;
    if(zData){if(std::isnan(i.depth[from]))useZ=false;else z+=i.depth[from];}
    else if(i.flat)z+=float(i.y+sourceY)/i.rasterScale-14+lift;
    else{int bottom=i.depthBottom==std::numeric_limits<int>::min()?i.y+i.height-1:i.depthBottom;z+=float(bottom)/i.rasterScale-14+lift+float((bottom-i.y-sourceY)/3)/i.rasterScale;}
    if(useZ&&z<=depth[to])continue;
    if(i.shadow){uint32_t old=color[to],v=old&0xff000000u;unsigned a=p>>24;for(int shift:{0,8,16})v|=(((old>>shift)&255)*(510-a)/510)<<shift;color[to]=v;}
    else if(preciseSample){
     const double alpha=std::clamp(filtered[3],0.,255.);uint32_t out=0xff000000u,old=color[to];
     for(int c=0;c<3;++c){int shift=16-c*8;double value=std::clamp(filtered[c],0.,alpha)+((old>>shift)&255)*(255-alpha)/255.;out|=uint32_t(std::clamp(std::floor(value+.5),0.,255.))<<shift;}color[to]=out;
    }
    else{if(!sampled)p=transform.apply(p,indexed?i.indices[from]:0,i.objectPalette);unsigned a=p>>24;
     if(a==255)color[to]=p;else{uint32_t old=color[to],out=0xff000000u;for(int shift:{0,8,16})out|=std::min(255u,((p>>shift)&255)+((old>>shift)&255)*(255-a)/255)<<shift;color[to]=out;}
    }
    // Building/terrain shadows write their ground plane. Without this, each
    // overlapping shadow halves the same pixel again and quickly turns black.
    if(useZ&&i.writeDepth)depth[to]=z;
   }
  }
  for(const auto&part:i.attachments)if((!part.shadow||i.shadowsAfter)&&(!part.airborneShadow||shadowDrop>0))blit(part,ax,ay+(part.shadow?shadowDrop*sy:0),sx,sy,base,light,lift,0,smooth);
 }
};
}}
