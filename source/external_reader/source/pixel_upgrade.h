#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <vector>

// Presentation-only indexed-art conversion and bounded physical scaling.
// Neither path changes the source palette, game memory, SHP indices, lighting
// tables, simulation, hit-testing, shroud visibility or object ownership.
namespace external { namespace pixelupgrade {
enum class PaletteRange : uint8_t { OriginalDAC, FullRGB };
enum class TwoX : uint8_t { Nearest, EdgeBlend };
constexpr uint8_t expand6(unsigned c,PaletteRange mode=PaletteRange::FullRGB){
 return uint8_t(mode==PaletteRange::FullRGB ? (c*255u+31u)/63u : c*4u);
}
inline bool decodePalette(const uint8_t*rgb,size_t length,std::array<uint32_t,256>&out,PaletteRange mode){
 if(!rgb||length!=768)return false;
 std::array<uint32_t,256> result{};
 for(unsigned i=0;i<256;++i){
  unsigned r=rgb[i*3],g=rgb[i*3+1],b=rgb[i*3+2];
  if((r|g|b)>63)return false;
  result[i]=0xff000000u|(uint32_t(expand6(r,mode))<<16)|
            (uint32_t(expand6(g,mode))<<8)|expand6(b,mode);
 }
 result[0]=0;out=result;return true;
}
constexpr unsigned channel(uint32_t c,int shift){return (c>>shift)&255u;}
inline bool similar(uint32_t a,uint32_t b){
 if((a>>24)!=(b>>24))return false;
 if(!(a>>24))return true;
 // Conservative approximate RGB distance; prevents mixing unrelated colors.
 int dr=int(channel(a,16))-int(channel(b,16));
 int dg=int(channel(a,8))-int(channel(b,8));
 int db=int(channel(a,0))-int(channel(b,0));
 return dr*dr+2*dg*dg+db*db<=3*24*24;
}
inline uint32_t blend(uint32_t a,uint32_t b){
 // Inputs are premultiplied ARGB. Never invent coverage across a cutout or
 // a shroud boundary; opaque edge blending can create true intermediate RGB.
 if((a>>24)!=255||(b>>24)!=255)return a;
 uint32_t v=0xff000000u;
 for(int s:{0,8,16})v|=(((channel(a,s)+channel(b,s)+1)/2)&255u)<<s;
 return v;
}
// Edge-guided interpolation derived from the published Scale2x neighborhood
// rule, with an optional half-blend rather than aliasing a palette index.
// This is NOT the xBRZ algorithm. The 2x image preserves source coordinates.
inline std::array<uint32_t,4> block(uint32_t center,uint32_t up,uint32_t left,
                                   uint32_t right,uint32_t down,TwoX mode){
 std::array<uint32_t,4> p{{center,center,center,center}};
 if(mode==TwoX::Nearest||!(center>>24)||similar(up,down)||similar(left,right))return p;
 const auto choose=[&](uint32_t a,uint32_t b){
  return similar(a,b)?blend(center,a):center;
 };
 p[0]=choose(up,left);
 p[1]=choose(up,right);
 p[2]=choose(down,left);
 p[3]=choose(down,right);
 return p;
}
template<class Fetch>
bool sample2x(Fetch fetch,int sourceX,int sourceY,int width,int height,
              std::vector<uint32_t>&out,TwoX mode=TwoX::EdgeBlend){
 if(width<1||height<1||width>8192||height>8192||
    (width&1)||(height&1)||uint64_t(width)*height>16*1024*1024)return false;
 out.assign(size_t(width)*height,0);
 for(int y=0;y<height;y+=2)for(int x=0;x<width;x+=2){
  int sx=sourceX+x/2,sy=sourceY+y/2;
  uint32_t c=fetch(sx,sy);
  if(!c)continue; // transparent outside the legal native backing
  auto p=block(c,fetch(sx,sy-1),fetch(sx-1,sy),
               fetch(sx+1,sy),fetch(sx,sy+1),mode);
  size_t at=size_t(y)*width+x;
  out[at]=p[0];out[at+1]=p[1];
  out[at+width]=p[2];out[at+width+1]=p[3];
 }
 return true;
}
inline bool scale2x(const std::vector<uint32_t>&src,int width,int height,
                    std::vector<uint32_t>&dst,TwoX mode=TwoX::EdgeBlend){
 if(width<1||height<1||width>4096||height>4096||
    uint64_t(width)*height>4*1024*1024||src.size()!=size_t(width)*height)return false;
 auto at=[&](int x,int y)->uint32_t{
  if(x<0||y<0||x>=width||y>=height)return 0;
  return src[size_t(y)*width+x];
 };
 return sample2x(at,0,0,width*2,height*2,dst,mode);
}
// Actual crop-to-output nearest sampling, including fractional 1.5x output.
template<class Fetch>bool sampleNearest(Fetch fetch,double x,double y,double spanX,double spanY,int width,int height,std::vector<uint32_t>&out){
 if(width<1||height<1||width>8192||height>8192||uint64_t(width)*height>16*1024*1024||
    !std::isfinite(x)||!std::isfinite(y)||!std::isfinite(spanX)||!std::isfinite(spanY)||
    std::abs(x)>1e7||std::abs(y)>1e7||spanX<=0||spanY<=0||spanX>32768||spanY>32768)return false;
 out.resize(size_t(width)*height);
 for(int yy=0;yy<height;++yy){int sy=int(std::floor(y+(yy+.5)*spanY/height));
  for(int xx=0;xx<width;++xx)out[size_t(yy)*width+xx]=fetch(int(std::floor(x+(xx+.5)*spanX/width)),sy);
 }return true;
}
// Edge-adaptive fractional resampling for the actual physical 1x-4x output.
// Four-source premultiplied ARGB sampling, no CRT/NTSC/bloom/blur pipeline.
// Prefer interpolation ALONG a prominent edge rather than across it.
inline unsigned colorDistance(uint32_t a,uint32_t b){
 const int dr=int(channel(a,16))-int(channel(b,16));
 const int dg=int(channel(a,8))-int(channel(b,8));
 const int db=int(channel(a,0))-int(channel(b,0));
 return unsigned(dr*dr+2*dg*dg+db*db);
}
using SampleWeights=std::array<double,4>;
inline SampleWeights nearestWeights(double fx,double fy){
 if(!std::isfinite(fx)||!std::isfinite(fy))return {};
 fx=std::clamp(fx,0.,1.);fy=std::clamp(fy,0.,1.);
 SampleWeights weights{};weights[(fy<.5?0:2)+(fx<.5?0:1)]=1.;return weights;
}
inline SampleWeights bilinearWeights(double fx,double fy){
 if(!std::isfinite(fx)||!std::isfinite(fy))return {};
 fx=std::clamp(fx,0.,1.);fy=std::clamp(fy,0.,1.);
 return {{(1-fx)*(1-fy),fx*(1-fy),(1-fx)*fy,fx*fy}};
}
// Keep the reconstruction decision separate from color evaluation. A caller
// with live lighting can use unlit art as the guide, then apply these weights
// to unquantized light contributions instead of making edges depend on light.
inline SampleWeights adaptiveWeights(uint32_t ul,uint32_t ur,uint32_t dl,uint32_t dr,
                                     double fx,double fy){
 if(!std::isfinite(fx)||!std::isfinite(fy))return {};
 fx=std::clamp(fx,0.,1.);fy=std::clamp(fy,0.,1.);
 // Transparency is a geometric boundary. Do not blur opaque actors into
 // missing tiles, shroud or source-crop transparent margins.
 const unsigned a0=ul>>24,a1=ur>>24,a2=dl>>24,a3=dr>>24;
 if(a0!=a1||a0!=a2||a0!=a3)return nearestWeights(fx,fy);
 if(a0==0)return {};
 const unsigned gx=colorDistance(ul,ur)+colorDistance(dl,dr);
 const unsigned gy=colorDistance(ul,dl)+colorDistance(ur,dr);
 // Strong vertical edge: prefer one column and interpolate down that edge.
 if(gx>4800&&uint64_t(gx)*10>uint64_t(gy)*17)fx=fx<.5?0.:1.;
 // Strong horizontal edge: prefer one row and interpolate along that edge.
 else if(gy>4800&&uint64_t(gy)*10>uint64_t(gx)*17)fy=fy<.5?0.:1.;
 // Crossing diagonal edges should not erase small unit silhouettes.
 else if(gx+gy>16000){
  const unsigned same0=colorDistance(ul,dr),same1=colorDistance(ur,dl);
  if(uint64_t(std::min(same0,same1))*2<uint64_t(std::max(same0,same1)))
   return nearestWeights(fx,fy);
 }
 return bilinearWeights(fx,fy);
}
inline uint32_t adaptivePixel(uint32_t ul,uint32_t ur,uint32_t dl,uint32_t dr,
                              double fx,double fy){
 const auto weights=adaptiveWeights(ul,ur,dl,dr,fx,fy);
 uint32_t result=0;
 for(int sh:{0,8,16,24}){
  double v=weights[0]*channel(ul,sh)+weights[1]*channel(ur,sh)+
           weights[2]*channel(dl,sh)+weights[3]*channel(dr,sh);
  unsigned rounded=unsigned(std::clamp(std::floor(v+.5),0.,255.));
  result|=rounded<<sh;
 }
 return result;
}
// Fetch MUST return transparent 0 outside the underlying native image.
// Source coordinates refer to the same fixed native camera crop as the
// original renderer; output width/height are physical device pixels.
template<class Fetch>
bool sampleAdaptive(Fetch fetch,double originX,double originY,
                    double spanX,double spanY,int width,int height,
                    std::vector<uint32_t>&out){
 if(width<1||height<1||width>8192||height>8192||
    uint64_t(width)*height>16*1024*1024||
    !std::isfinite(originX)||!std::isfinite(originY)||
    !std::isfinite(spanX)||!std::isfinite(spanY)||
    std::abs(originX)>1e7||std::abs(originY)>1e7||
    spanX<=0||spanY<=0||spanX>32768||spanY>32768||
    width<spanX||height<spanY||(width==spanX&&height==spanY)||
    double(width)/spanX>4||double(height)/spanY>4)return false;
 out.resize(size_t(width)*height);
 const double dx=spanX/width,dy=spanY/height;
 for(int y=0;y<height;++y){
  const double py=originY+(y+.5)*dy-.5;
  const int iy=int(std::floor(py));
  const double fy=py-iy;
  for(int x=0;x<width;++x){
   const double px=originX+(x+.5)*dx-.5;
   const int ix=int(std::floor(px));
   const double fx=px-ix;
   out[size_t(y)*width+x]=adaptivePixel(fetch(ix,iy),fetch(ix+1,iy),
                           fetch(ix,iy+1),fetch(ix+1,iy+1),fx,fy);
  }
 }
 return true;
}
}} // namespace external::pixelupgrade
