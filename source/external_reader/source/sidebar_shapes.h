#pragma once
#include <cstdint>
#include <cstring>
#include <vector>
#include <algorithm>
namespace external { namespace sidebar {
constexpr uint32_t ReminderMask=15; // all four native production pages
struct Frame {int x=0,y=0,width=0,height=0;std::vector<uint32_t> pixels;};
inline unsigned u16(const uint8_t*b){return unsigned(b[0])|(unsigned(b[1])<<8);}
inline uint32_t u32(const uint8_t*b){return uint32_t(u16(b))|(uint32_t(u16(b+2))<<16);}
// TAB00..03.SHP are already loaded by Sidebar at 6A593E. Read the actual
// ShapeButton (stride 0x60), state frame and palette; never load/patch in-game.
template<class Read> bool sample(Read read,int tab,int screenWidth,int screenHeight,Frame&out,int requestedFrame=-1){
 out={};if(tab<0||tab>3)return false;uint32_t button=0xB07C48+tab*0x60;uint8_t b[0x60]{};
 if(!read(button,b,sizeof b)||u32(b+0x24)!=unsigned(203+tab)||b[0x1E])return false;
 int x=int32_t(u32(b+0x0C)),y=int32_t(u32(b+0x10));
 uint32_t shp=u32(b+0x58),converter=u32(b+0x50),decoded=0;
 uint8_t head[8]{};if(shp<0x10000||!read(shp,head,8))return false;
 if(u16(head)==65535){if(!read(shp+12,&decoded,4)||decoded<0x10000||!read(decoded,head,8))return false;shp=decoded;}
 int w=u16(head+2),h=u16(head+4),frames=u16(head+6);
 if(w<1||h<1||w>256||h>256||frames<1||frames>256||x<0||y<0||int64_t(x)+w>screenWidth||int64_t(y)+h>screenHeight)return false;
 int frame=requestedFrame>=0?requestedFrame:b[0x40]?(b[0x2D]!=0)+(b[0x34]?3:0):(b[0x2C]!=0);if(frame>=frames)return false;
 uint8_t f[24]{};if(!read(shp+8+frame*24,f,24))return false;
 int left=u16(f),top=u16(f+2),width=u16(f+4),height=u16(f+6);unsigned compression=u32(f+8)&3;uint32_t offset=u32(f+20);
 if(width<1||height<1||left+width>w||top+height>h||offset<unsigned(8+24*frames)||offset>1024*1024||uint64_t(shp)+offset+256*1024>UINT32_MAX)return false;
 int mode=0;uint32_t paletteAt=0;uint16_t palette[256];
 if(converter<0x10000||!read(converter+4,&mode,4)||mode!=2||!read(converter+0x174,&paletteAt,4)||paletteAt<0x10000||!read(paletteAt,palette,sizeof palette))return false;
 std::vector<uint32_t> pixels(size_t(w)*h,0);uint32_t at=shp+offset;
 for(int row=0;row<height;++row){uint8_t data[1024];unsigned bytes=width;
  if(compression>=2){uint16_t n=0;if(!read(at,&n,2)||n<2||n>1024)return false;bytes=n;}
  if(!read(at,data,bytes))return false;at+=bytes;unsigned pos=compression>=2?2:0;int col=0;
  while(pos<bytes&&col<width){unsigned v=data[pos++];if(compression==3&&!v){if(pos>=bytes)return false;unsigned run=data[pos++];if(!run||col+int(run)>width)return false;col+=run;continue;}
   if(v){unsigned rgb=palette[v],r=((rgb>>11)&31)*255/31,g=((rgb>>5)&63)*255/63,blue=(rgb&31)*255/31,luma=(r*3+g*6+blue)/10;
    pixels[(top+row)*w+left+col]=0xFF000000u|(r<<16)|(g<<8)|blue;
   }++col;
  }if(col!=width)return false;
 }
 // Reject pointer swaps while reading, e.g. sidebar faction/round changes.
 uint32_t after=0;if(!read(button+0x58,&after,4)||after!=u32(b+0x58))return false;
 out={x,y,w,h,std::move(pixels)};return true;
}
// A TAB SHP contains both its glyph and an opaque rectangular plate.
// Compare the four normal-state tabs to remove that shared plate, including
// bevels/gradients. Never turn an unavailable glyph mask into a solid rectangle.
inline bool glyph(const Frame(&base)[4],int tab,Frame&shown,bool paused){
 const auto&own=base[tab];if(shown.width!=own.width||shown.height!=own.height)return false;
 for(const auto&f:base)if(f.width!=own.width||f.height!=own.height||f.pixels.size()!=own.pixels.size())return false;
 std::vector<uint8_t> mask(own.pixels.size());unsigned count=0;
 for(size_t i=0;i<mask.size();++i){if(!own.pixels[i])continue;unsigned equal=0;
  for(const auto&f:base)equal+=f.pixels[i]==own.pixels[i];
  // Glyph-only differences. Shared central strokes may remain uncolored;
  // shared backing pixels must never blink merely because they are opaque.
  if(equal<=2){bool backing=false;size_t row=i/own.width;for(int x=0;x<own.width&&!backing;++x){size_t q=row*own.width+x;bool shared=true;for(const auto&f:base)shared&=f.pixels[q]==base[0].pixels[q];backing=shared&&base[0].pixels[q]==own.pixels[i];}if(!backing){mask[i]=1;++count;}}
 }
 if(!count||count*2>mask.size())return false;
 for(size_t i=0;i<mask.size();++i){if(!mask[i]){shown.pixels[i]=0;continue;}
  auto color=shown.pixels[i];if(!color)continue;unsigned luma=(((color>>16)&255)*3+((color>>8)&255)*6+(color&255))/10;
  unsigned rr=std::min(255u,80+luma),gg=paused?std::min(230u,40+luma*4/5):20+luma/5,bb=15+luma/4;
  shown.pixels[i]=0xFF000000u|(rr<<16)|(gg<<8)|bb;
 }return true;
}
struct Cache {
 Frame frames[4];uint32_t tick=0,pid=0,match=0,revision=0;int width=0,height=0;
 template<class Read>uint32_t update(Read read,uint32_t now,uint32_t process,uint32_t epoch,int w,int h,uint32_t mask,uint32_t pausedMask=15){
  bool context=pid!=process||match!=epoch||width!=w||height!=h;
  if(context){for(auto&f:frames)f={};pid=process;match=epoch;width=w;height=h;tick=0;++revision;}
  mask&=ReminderMask;
  if(!tick||uint32_t(now-tick)>=100){tick=now;Frame base[4];bool ready=true;for(int i=0;i<4;++i)if(!sample(read,i,w,h,base[i],0))ready=false;
   for(int i=0;i<4;++i){Frame next;if((mask&(1u<<i))&&ready&&sample(read,i,w,h,next)&&!glyph(base,i,next,(pausedMask&(1u<<i))!=0))next={};auto&f=frames[i];
   if(f.x!=next.x||f.y!=next.y||f.width!=next.width||f.height!=next.height||f.pixels!=next.pixels){f=std::move(next);++revision;}
  }}
  uint32_t ready=0;for(int i=0;i<4;++i)if((mask&(1u<<i))&&!frames[i].pixels.empty())ready|=1u<<i;return ready;
 }
};
}}
