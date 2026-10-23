#pragma once
#include "voxel_preview.h"
#include <string>
namespace external { namespace terrain {
// The game's 547020 loader relocates the TMP frame directory to absolute
// pointers. Frame-internal data offsets remain relative to the 52-byte header.
// Format references and native evidence are recorded in research/INDEX.md.
template<class Read>bool palette(Read read,uint32_t address,std::array<uint32_t,256>&out){
 uint8_t rgb[768];if(!voxel::range(address,sizeof rgb)||!read(address,rgb,sizeof rgb))return false;
 if(std::all_of(rgb,rgb+sizeof rgb,[](uint8_t c){return c==0;}))return false;
 for(int i=0;i<256;++i)out[i]=0xff000000u|(uint32_t(rgb[i*3])<<16)|(uint32_t(rgb[i*3+1])<<8)|rgb[i*3+2];return true;
}
template<class Read>bool tile(Read read,uint32_t pointer,unsigned index,const std::array<uint32_t,256>&pal,voxel::Image&out,std::string*reason=nullptr){
 auto fail=[&](const std::string&s){if(reason)*reason=s;return false;};if(reason)reason->clear();
 uint32_t h[4]{};if(!voxel::range(pointer,16)||!read(pointer,h,sizeof h))return fail("header-read");
 if(!h[0]||!h[1]||h[0]>64||h[1]>64||h[2]!=60||h[3]!=30)return fail("header="+std::to_string(h[0])+","+std::to_string(h[1])+","+std::to_string(h[2])+","+std::to_string(h[3]));
 unsigned count=h[0]*h[1];index%=count;if(!voxel::range(pointer,16+count*4))return fail("directory-bounds");uint32_t p=0;if(!read(pointer+16+index*4,&p,4)||!voxel::range(p,52))return fail("empty-or-invalid-subtile");
 uint8_t header[52];if(!read(p,header,sizeof header))return fail("subtile-read");
 auto word=[&](int at){return voxel::u32(header+at);};
 // Both 8-bit planes contain 900 diamond pixels. The zero-length last row
 // is intentional: the native YR renderer also draws 29 scanlines.
 constexpr int area=900;uint32_t zOffset=word(12),extraOffset=word(8),flags=word(36);
 if(zOffset!=52+area)return fail("z-offset="+std::to_string(zOffset)+" flags="+std::to_string(flags));
 int ex=int32_t(word(20)),ey=int32_t(word(24)),ew=int32_t(word(28)),eh=int32_t(word(32));
 int baseX=(int(index%h[0])-int(index/h[0]))*30,baseY=(int(index%h[0])+int(index/h[0]))*15;
 // The native renderer tests only flag bit 0 for the extra image. Unused
 // flags and extra coordinates must not invalidate a plain diamond.
 bool extra=(flags&1)!=0;int left=0,top=0,right=60,bottom=30;
 if(extra){if(ex<-16384||ex>16384||ey<-16384||ey>16384)return fail("extra-origin");ex-=baseX;ey-=baseY;
  if(ew<=0||eh<=0||ew>512||eh>512||extraOffset<52+area*2||extraOffset>voxel::MaxBytes)return fail("extra-bounds");left=std::min(0,ex);top=std::min(0,ey);right=std::max(60,ex+ew);bottom=std::max(30,ey+eh);}
 if(right-left>512||bottom-top>512)return fail("canvas-bounds");
 uint8_t diamond[area];if(!voxel::range(p,52+area*2)||!read(p+52,diamond,sizeof diamond))return fail("diamond-read");
 voxel::Image result;result.width=right-left;result.height=bottom-top;result.x=left-30;result.y=top-15;result.pixels.resize(size_t(result.width)*result.height);
 size_t at=0;for(int y=0;y<30;++y){int width=4*std::min(y+1,29-y),x=(60-width)/2;for(int i=0;i<width;++i)result.pixels[size_t(y-top)*result.width+x+i-left]=pal[diamond[at++]];}if(at!=area)return false;
 if(extra){size_t n=size_t(ew)*eh;if(uint64_t(p)+extraOffset+n>0x100000000ull||!voxel::range(p+extraOffset,n))return fail("extra-overflow");std::vector<uint8_t> pixels(n);if(!read(p+extraOffset,pixels.data(),n))return fail("extra-read");
  for(int y=0;y<eh;++y)for(int x=0;x<ew;++x)if(uint8_t c=pixels[size_t(y)*ew+x])result.pixels[size_t(y+ey-top)*result.width+x+ex-left]=pal[c];
 }
 uint32_t after=0;if(!read(pointer+16+index*4,&after,4)||after!=p)return fail("directory-changed");out=std::move(result);return true;
}
}}
