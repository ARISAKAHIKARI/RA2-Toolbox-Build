#pragma once
#include <cstddef>
#include <cstdint>
namespace cursorresource {
constexpr unsigned MaxFrames=16384,MaxBytes=16*1024*1024;
inline unsigned u16(const unsigned char* p){return unsigned(p[0])|(unsigned(p[1])<<8);}
inline uint32_t u32(const unsigned char* p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
// Raw SHA layout only. References must be resolved by the caller first.
// Inspect all headers, then the last stored row block; decoder validates the
// entire copied resource outside the game thread. No per-frame capture work.
template<class Read>
bool extent(uint32_t image,Read read,unsigned char* headers,unsigned& bytes,unsigned& frames,unsigned& w,unsigned& h){
 if(!read(image,headers,8)||u16(headers)!=0)return false;
 w=u16(headers+2);h=u16(headers+4);frames=u16(headers+6);
 if(!w||!h||w>512||h>512||!frames||frames>MaxFrames)return false;
 bytes=8+frames*24;if(!read(image,headers,bytes))return false;
 unsigned lastOffset=0,lastWidth=0,lastHeight=0,lastFormat=0;
 uint64_t total=0;
 for(unsigned i=0;i<frames;++i){auto p=headers+8+i*24;
  unsigned x=u16(p),y=u16(p+2),fw=u16(p+4),fh=u16(p+6),fmt=u32(p+8)&255,off=u32(p+20);
  if(x+fw>w||y+fh>h||fmt>3)return false;
  total+=uint64_t(fw)*fh;if(total>32*1024*1024)return false;
  if(!off||!fw||!fh)continue;
  if(off<8+frames*24||off>=MaxBytes)return false;
  if(fmt<2){uint64_t end=uint64_t(off)+fw*fh;if(end>MaxBytes)return false;if(end>bytes)bytes=unsigned(end);}
  if(off>lastOffset){lastOffset=off;lastWidth=fw;lastHeight=fh;lastFormat=fmt;}
 }
 if(lastOffset){uint64_t end=lastOffset;
  if(lastFormat<2)end+=lastWidth*lastHeight;
  else for(unsigned y=0;y<lastHeight;++y){unsigned char row[2];
   if(end+2>MaxBytes||!read(image+uint32_t(end),row,2))return false;
   unsigned length=u16(row);if(length<2||length>2048)return false;end+=length;
  }
  if(end>MaxBytes)return false;if(end>bytes)bytes=unsigned(end);
 }
 return bytes<=MaxBytes;
}
}
