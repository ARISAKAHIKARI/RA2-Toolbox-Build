#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

// Presentation-only indexed-art conversion and bounded 2x scaling.
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
}} // namespace external::pixelupgrade
