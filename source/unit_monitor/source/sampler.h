#pragma once
#include "core.h"
#include "icon_cache.h"
#include "owned_counter.h"
#include "../../external_reader/source/layout_probe.h"

// Layout: YR 1.001 / YRpp; audited against supplied gamemd-spawn(1).exe.
// Read is a bounded, fault-tolerant memory reader, Resolve optionally resolves an SHP cache reference.
namespace um {
struct PtrList {
 uint32_t items[4096]{};int n=0;
 uint32_t* begin(){return items;}uint32_t* end(){return items+n;}
 size_t size()const{return size_t(n);}uint32_t* data(){return items;}
 uint32_t& operator[](size_t i){return items[i];}
 void push_back(uint32_t p){if(n<4096)items[n++]=p;}
};
inline bool sameAscii(const char* a,const char* b){for(;*a&&*b;a++,b++){char x=*a,y=*b;if(x>='A'&&x<='Z')x+=32;if(y>='A'&&y<='Z')y+=32;if(x!=y)return false;}return *a==*b;}

template<class Read,class Resolve> class Sampler {
 Read read; Resolve resolve; IconCache* cache=nullptr; Snapshot state{};external::layout::Result probe_{}; uint32_t localPrev=0; int iconCursor=0; uint32_t ticks=0,matchSerial=0; int framePrev=-1; bool prevBuildings=true; uint8_t iconBuffer[32+256*1024]{}; int ownedBuffer[4096]{};
 template<class T>T get(uint32_t a,T def={}) {T v{};return read(a,&v,sizeof(v))?v:def;}
 bool bytes(uint32_t a,void* p,size_t n){return read(a,p,n);}
 PtrList vec(uint32_t a,int limit=4096) {
  uint32_t p=get<uint32_t>(a+4);int cap=get<int>(a+8,-1),n=get<int>(a+16,-1);
  if(n<0||n>limit||cap<n||cap>65536||(!p&&n)){state.readErrors++;return {};}
  PtrList r;r.n=n;if(n&&!bytes(p,r.data(),n*4)){state.readErrors++;return {};}return r;
 }
 int find(uint32_t p){for(int i=0;i<state.typeCount;i++)if(state.types[i].ptr==p)return i;return -1;}
 bool loadIcon(Type& t) {
  uint32_t p=get<uint32_t>(t.ptr+1776);if(!p)return false;
  uint16_t tag=get<uint16_t>(p);if(tag==65535){uint32_t d=get<uint32_t>(p+12);if(!d)d=resolve(p);p=d;}
  if(!p)return false;
  uint8_t header[32];if(!bytes(p,header,32))return false;
  uint32_t off=uint32_t(header[28])|(uint32_t(header[29])<<8)|(uint32_t(header[30])<<16)|(uint32_t(header[31])<<24);
  unsigned w=header[12]|(header[13]<<8),h=header[14]|(header[15]<<8),flags=header[16]&3;
  if(!w||!h||w>256||h>256||off<32||off>1024*1024)return false;
  uint8_t* b=iconBuffer;size_t used=32;memcpy(b,header,32);b[28]=32;b[29]=b[30]=b[31]=0;
  uint32_t at=p+off;
  for(unsigned y=0;y<h;y++){
   unsigned len=w;if(flags>=2){uint16_t row=0;if(!bytes(at,&row,2)||row<2||row>1024)return false;len=row;}
   size_t pos=used;used+=len;if(used>sizeof(iconBuffer)||!bytes(at,b+pos,len))return false;at+=len;
  }
  if(!decode(b,used,t.icon))return false;
  t.iconReady=1;t.iconCached=0;return true;
 }
 void palette(){
  state.paletteReady=0;
  uint32_t conv=get<uint32_t>(0x87F6B0),mid=get<uint32_t>(conv+0x174);uint16_t pal[256];
  if(!conv||get<int>(conv+4)!=2||!bytes(mid,pal,sizeof pal))return;
  for(int i=0;i<256;i++){uint32_t c=pal[i];unsigned r=((c>>11)&31)*255/31,g=((c>>5)&63)*255/63,b=(c&31)*255/31;state.palette[i]=0xFF000000|(r<<16)|(g<<8)|b;}state.palette[0]=0;state.paletteReady=1;
 }
 void types(bool buildings) {
  if(cache)cache->rescope();
  uint32_t arrays[]={0xA8E348,0xA83CE0,0xA8B218,0xA83C68};
  state.typeCount=0;state.truncated=0;iconCursor=0;
  for(int c=0;c<(buildings?4:3);c++){
   auto v=vec(arrays[c]);for(size_t ix=0;ix<v.size();ix++){
    uint32_t p=v[ix];char cameo[25]={};if(!bytes(p+1750,cameo,24))continue;
    bool has=cameo[0]&&!sameAscii(cameo,"none")&&!sameAscii(cameo,"<none>");
    if(!buildable(get<int>(p+1588,-1),get<uint32_t>(p+1740),get<uint8_t>(p+562)!=0,get<uint8_t>(p+3412)!=0,has))continue;
    if(state.typeCount==MaxTypes){state.truncated++;continue;}
    Type& t=state.types[state.typeCount++];t={};t.ptr=p;t.category=c;t.index=int(ix);bytes(p+36,t.id,23);t.id[23]=0;memcpy(t.cameo,cameo,sizeof t.cameo);
   }
  }
 }
 void owned(Player& player,uint32_t house){
  // CountOwnedNow: real ownership (including passengers/garrison), not
  // FactoryClass queue counts. CounterClass capacity grows on demand, so
  // indices above Capacity are zero rather than a fatal category read error.
  for(int cat=0;cat<4;++cat){
   int needed=0;
   for(int i=0;i<state.typeCount;++i)
    if(state.types[i].category==cat)needed=std::max(needed,state.types[i].index+1);
   if(!needed)continue;
   if(!readCounterPrefix(read,house+OwnedOffsets[cat],needed,ownedBuffer,state.readErrors))continue;
   for(int i=0;i<state.typeCount;++i)if(state.types[i].category==cat){
    int n=ownedBuffer[state.types[i].index];
    if(n<0||n>1000000){state.readErrors++;continue;}
    player.cells[i].owned=n;
   }
  }
 }
public:
 Sampler(Read r,Resolve s,IconCache* c=nullptr):read(r),resolve(s),cache(c){}
 external::layout::Result probe()const{return probe_;}
 void reset(){probe_={};localPrev=0;framePrev=-1;}
 const Snapshot& sample(uint32_t tick,bool buildings){
  state.tick=tick;state.readErrors=0;state.playerCount=0;state.status=1;
  probe_=external::layout::inspect(read);
  if(probe_.stage!=external::layout::Stage::Ready){localPrev=0;state.status=0;return state;}
  uint32_t local=get<uint32_t>(0xA83D4C),observer=get<uint32_t>(0xAC1198);
  int frame=get<int>(0xA8ED84);
  if(local!=localPrev||frame<framePrev)++matchSerial;
  state.match=matchSerial;state.frame=frame;state.chatActive=get<uint8_t>(0xA8BC60+25)!=0;
  if(local!=localPrev||frame<framePrev||prevBuildings!=buildings||!state.typeCount){types(buildings);localPrev=local;prevBuildings=buildings;}
  framePrev=frame;palette();
  // All lobby nodes, not just the single local Observer pointer: multiplayer supports multiple spectators.
  PtrList spectators;
  for(uint32_t node:vec(0xA8DA74,32))if(get<int>(node+0x6B)==-1)spectators.push_back(get<int>(node+0x6F,-1));
  auto houses=vec(0xA80228,32);
  if(std::find(houses.begin(),houses.end(),local)==houses.end()){
   probe_={external::layout::Stage::MissingLocalHouseInList,0xA80228,local};
   state.status=0;return state;
  }
  for(size_t k=0;k<houses.size();k++)if(houses[k]==local){for(size_t j=k;j>0;j--)houses[j]=houses[j-1];houses[0]=local;break;}
  for(uint32_t h:houses){
   int idx=get<int>(h+48,-1);uint32_t ht=get<uint32_t>(h+52);
   bool spec=h==observer||std::find(spectators.begin(),spectators.end(),idx)!=spectators.end();
   if(!eligible(get<uint8_t>(h+493)||get<uint8_t>(h+492),get<uint8_t>(h+501),get<uint8_t>(h+502),get<uint8_t>(h+504),spec,get<uint8_t>(ht+0x1A6)))continue;
   if(state.playerCount>=MaxPlayers){state.truncated++;break;}
   Player& player=state.players[state.playerCount++];player={};player.ptr=h;player.houseIndex=idx;player.local=h==local;
   uint8_t rgb[3]={};bytes(h+22265,rgb,3);player.color=0xFF000000|(rgb[0]<<16)|(rgb[1]<<8)|rgb[2];
   bytes(h+90154,player.name,40);player.name[20]=0;
   owned(player,h);
  }
  // Factories are distinct from structures, so multiple primary factories cannot double count a shared queue.
  for(uint32_t f:vec(0xA83E30,1024)){
   uint32_t owner=get<uint32_t>(f+108);Player* p=nullptr;for(int i=0;i<state.playerCount;i++)if(state.players[i].ptr==owner)p=&state.players[i];if(!p)continue;
   for(uint32_t type:vec(f+64,4096)){int i=find(type);if(i>=0)p->cells[i].queued++;}
   uint32_t obj=get<uint32_t>(f+88);if(!obj)continue;
   // Type is at 0x6C0 for infantry, 0x6C4 for vehicle/aircraft and 0x520 for buildings.
   uint32_t offsets[]={1728,1732,1312};int ix=-1;
   for(uint32_t off:offsets){int candidate=find(get<uint32_t>(obj+off));if(candidate>=0){int cat=state.types[candidate].category;if((off==1728&&cat==0)||(off==1732&&(cat==1||cat==2))||(off==1312&&cat==3)){ix=candidate;break;}}}
   if(ix<0)continue;
   Cell& cell=p->cells[ix];cell.queued++;cell.active++;cell.progress=std::clamp(get<int>(f+36),0,54);cell.paused=get<uint8_t>(f+92)||get<uint8_t>(f+113);
   // TechnoClass::CountedAsOwned (+0x3CC) is confirmed in YRpp; the
   // in-factory product is not yet delivered, even if HouseClass counted it.
   if(get<uint8_t>(obj+972)&&cell.owned>0)cell.owned--;
  }
  // Load up to four missing cameos per sample, prioritizing rows actually displayed.
  // Only consume budget when loadIcon succeeds so unloaded enemy-faction SHP pointers do not starve ready icons.
  int budget=4;state.missingIcons=0;
  for(int step=0;step<state.typeCount;step++){
   int i=(iconCursor+step)%state.typeCount;auto&t=state.types[i];
   if(cache&&!state.paletteReady)cache->restorePalette(t,state);
   if(cache&&t.iconReady==1&&!t.iconCached)t.iconCached=cache->remember(t,state);
   if(t.iconReady==1)continue;
   if(cache&&(!t.iconReady||(t.iconReady==2&&!state.paletteReady)))cache->restore(t,state);
   bool used=false;for(int j=0;j<state.playerCount;j++)if(state.players[j].cells[i].owned||state.players[j].cells[i].queued)used=true;
   if(used){bool missing=!t.iconReady;if(missing)state.missingIcons++;if(budget>0&&(!t.iconRetry||int32_t(tick-t.iconRetry)>=0)){t.iconRetry=tick+500;if(loadIcon(t)){budget--;if(missing)state.missingIcons--;if(cache)t.iconCached=cache->remember(t,state);}}}
  }
  if(state.typeCount)iconCursor=(iconCursor+4)%state.typeCount;
  ticks++;return state;
 }
};
}
