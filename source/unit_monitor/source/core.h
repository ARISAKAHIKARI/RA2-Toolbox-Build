#pragma once
#include <cstdint>
#include <cstring>
#include <algorithm>
namespace um {
constexpr int MaxTypes=512, MaxPlayers=8, IW=60, IH=48;
constexpr uint32_t Magic=0x554D3032;
struct Type { uint32_t ptr; int category,index; char id[24]; char cameo[25]; int iconCached;uint32_t iconRetry; uint8_t icon[IW*IH]; int iconReady; };
struct Cell { int owned,queued,active,progress,paused; };
struct Player { uint32_t ptr,color; int houseIndex,local; uint16_t name[21]; Cell cells[MaxTypes]; };
struct Snapshot { int status,typeCount,playerCount,missingIcons,truncated,readErrors,paletteReady; uint32_t tick; uint32_t palette[256]; Type types[MaxTypes]; Player players[MaxPlayers]; uint32_t match; int frame,chatActive; };
struct Shared { uint32_t magic,pid,owner; volatile int32_t seq,command; volatile uint32_t heartbeat; volatile int32_t includeBuildings; Snapshot data; };
inline bool eligible(bool human,bool defeated,bool gameOver,bool loser,bool observer,bool passive) {return human&&!defeated&&!gameOver&&!loser&&!observer&&!passive;}
inline bool buildable(int tech,uint32_t owners,bool insignificant,bool spawned,bool cameo) {return tech>=0&&tech<=10&&owners&&!insignificant&&!spawned&&cameo;}
// Air and navy are grouped with vehicles; buildings stay last.
inline int categoryOrder(int category){return category==1||category==2?0:category==0?1:2;}
inline bool showCell(int category,const Cell& cell,bool showBuildings){return cell.queued>0||(cell.owned>0&&(category!=3||showBuildings));}
inline int shownOwned(int category,const Cell& cell,bool showBuildings){return category==3&&!showBuildings?0:cell.owned;}
// SHP(TS) first-frame decoder. type 0/1: raw, 2: row-size prefixed raw, 3: zero-run RLE.
inline bool decode(const uint8_t* b,size_t n,uint8_t out[IW*IH]) {
 auto u16=[&](size_t i)->unsigned{return unsigned(b[i])|(unsigned(b[i+1])<<8);};
 auto u32=[&](size_t i)->uint32_t{return uint32_t(b[i])|(uint32_t(b[i+1])<<8)|(uint32_t(b[i+2])<<16)|(uint32_t(b[i+3])<<24);};
 if(n<32)return false;
 int cw=u16(2),ch=u16(4),left=u16(8),top=u16(10),w=u16(12),h=u16(14);unsigned flags=u32(16)&3;size_t p=u32(28);
 if(cw<1||ch<1||cw>256||ch>256||w<1||h<1||left+w>cw||top+h>ch||p<32||p>=n)return false;
 uint8_t tmp[256*256]={};
 for(int y=0;y<h;y++) {
  size_t end=p+w;
  if(flags>=2){if(p+2>n)return false;unsigned len=u16(p);if(len<2||p+len>n)return false;end=p+len;p+=2;}
  if(end>n)return false;
  int x=0;
  while(p<end&&x<w){unsigned v=b[p++];if(flags==3&&v==0){if(p>=end)return false;unsigned count=b[p++];if(!count||x+int(count)>w)return false;x+=count;}else tmp[(top+y)*cw+left+x++]=uint8_t(v);}
  if(x!=w)return false;
  p=end;
 }
 for(int y=0;y<IH;y++)for(int x=0;x<IW;x++)out[y*IW+x]=tmp[(y*ch/IH)*cw+x*cw/IW];
 return true;
}
}
