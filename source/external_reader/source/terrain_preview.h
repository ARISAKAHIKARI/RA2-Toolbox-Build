#pragma once
#include "voxel_preview.h"
namespace external { namespace terrain {
// The game's 547020 loader relocates the TMP frame directory to absolute
// pointers. Frame-internal data offsets remain relative to the 52-byte header.
// Format references and native evidence are recorded in research/INDEX.md.
template<class Read>bool palette(Read read,uint32_t address,std::array<uint32_t,256>&out){
 uint8_t rgb[768];if(!voxel::range(address,sizeof rgb)||!read(address,rgb,sizeof rgb))return false;
 if(std::all_of(rgb,rgb+sizeof rgb,[](uint8_t c){return c==0;}))return false;
 for(int i=0;i<256;++i)out[i]=0xff000000u|(uint32_t(rgb[i*3])<<16)|(uint32_t(rgb[i*3+1])<<8)|rgb[i*3+2];return true;
}
template<class Read>bool tile(Read read,uint32_t pointer,unsigned index,const std::array<uint32_t,256>&pal,voxel::Image&out){
 uint32_t h[4]{};if(!voxel::range(pointer,16)||!read(pointer,h,sizeof h)||!h[0]||!h[1]||h[0]>64||h[1]>64||h[2]!=60||h[3]!=30)return false;
 unsigned count=h[0]*h[1];index%=count;if(!voxel::range(pointer,16+count*4))return false;uint32_t p=0;if(!read(pointer+16+index*4,&p,4)||!voxel::range(p,52))return false;
 uint8_t header[52];if(!read(p,header,sizeof header))return false;
 auto word=[&](int at){return voxel::u32(header+at);};
 // Both 8-bit planes contain 900 diamond pixels. The zero-length last row
 // is intentional: the native YR renderer also draws 29 scanlines.
 constexpr int area=900;uint32_t zOffset=word(12),extraOffset=word(8),flags=word(36);
 if(zOffset!=52+area||flags&~7u)return false;
 int ex=int32_t(word(20)),ey=int32_t(word(24)),ew=int32_t(word(28)),eh=int32_t(word(32));
 int baseX=(int(index%h[0])-int(index/h[0]))*30,baseY=(int(index%h[0])+int(index/h[0]))*15;
 if(ex<-16384||ex>16384||ey<-16384||ey>16384)return false;ex-=baseX;ey-=baseY;
 bool extra=(flags&1)!=0;int left=0,top=0,right=60,bottom=30;
 if(extra){if(ew<=0||eh<=0||ew>512||eh>512||extraOffset<52+area*2||extraOffset>voxel::MaxBytes)return false;left=std::min(0,ex);top=std::min(0,ey);right=std::max(60,ex+ew);bottom=std::max(30,ey+eh);}
 if(right-left>512||bottom-top>512)return false;
 uint8_t diamond[area];if(!voxel::range(p,52+area*2)||!read(p+52,diamond,sizeof diamond))return false;
 voxel::Image result;result.width=right-left;result.height=bottom-top;result.x=left-30;result.y=top-15;result.pixels.resize(size_t(result.width)*result.height);
 size_t at=0;for(int y=0;y<30;++y){int width=4*std::min(y+1,29-y),x=(60-width)/2;for(int i=0;i<width;++i)result.pixels[size_t(y-top)*result.width+x+i-left]=pal[diamond[at++]];}if(at!=area)return false;
 if(extra){size_t n=size_t(ew)*eh;if(uint64_t(p)+extraOffset+n>0x100000000ull||!voxel::range(p+extraOffset,n))return false;std::vector<uint8_t> pixels(n);if(!read(p+extraOffset,pixels.data(),n))return false;
  for(int y=0;y<eh;++y)for(int x=0;x<ew;++x)if(uint8_t c=pixels[size_t(y)*ew+x])result.pixels[size_t(y+ey-top)*result.width+x+ex-left]=pal[c];
 }
 uint32_t after=0;if(!read(pointer+16+index*4,&after,4)||after!=p)return false;out=std::move(result);return true;
}
}}
