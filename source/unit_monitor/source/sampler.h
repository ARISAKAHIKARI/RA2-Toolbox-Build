#pragma once
#include "core.h"
#include "icon_cache.h"
#include "cameo_reader.h"
#include "owned_counter.h"
#include "round_state.h"
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
 Read read; Resolve resolve; IconCache* cache=nullptr; Snapshot state{};external::layout::Result probe_{}; roundstate::Stamp previous{}; bool haveRound=false; int iconCursor=0; uint32_t ticks=0,matchSerial=0; bool prevBuildings=true; uint8_t iconBuffer[32+256*1024]{}; int ownedBuffer[4096]{};
 const Snapshot& invalidate(uint32_t tick){
  memset(&state,0,sizeof state);state.tick=tick;state.match=matchSerial;haveRound=false;iconCursor=0;return state;
 }
 template<class T>T get(uint32_t a,T def={}) {T v{};return read(a,&v,sizeof(v))?v:def;}
 bool bytes(uint32_t a,void* p,size_t n){return read(a,p,n);}
 PtrList vec(uint32_t a,int limit=4096) {
  uint32_t p=get<uint32_t>(a+4);int cap=get<int>(a+8,-1),n=get<int>(a+16,-1);
  if(n<0||n>limit||cap<n||cap>65536||(!p&&n)){state.readErrors++;return {};}
  PtrList r;r.n=n;if(n&&!bytes(p,r.data(),n*4)){state.readErrors++;return {};}return r;
 }
 int find(uint32_t p){for(int i=0;i<state.typeCount;i++)if(state.types[i].ptr==p)return i;return -1;}
 bool loadIcon(Type&t){return readCameo(read,resolve,t,iconBuffer,sizeof iconBuffer);}
 void palette(){state.paletteReady=0;readCameoPalette(read,state);}
 void types(bool buildings,bool newRound) {
  if(cache&&newRound)cache->rescope();
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
 void reset(){probe_={};invalidate(0);}
 const Snapshot& sample(uint32_t tick,bool buildings){
  roundstate::Stamp start{},end{};
  if(!roundstate::capture(read,start,probe_))return invalidate(tick);
  const bool newRound=!haveRound||!start.follows(previous);
  if(newRound){memset(&state,0,sizeof state);if(++matchSerial==0)++matchSerial;}
  state.tick=tick;state.readErrors=0;state.playerCount=0;state.status=1;
  uint32_t local=start.local,observer=get<uint32_t>(0xAC1198);
  state.match=matchSerial;state.frame=start.frame;state.chatActive=get<uint8_t>(0xA8BC60+25)!=0;
  if(newRound||prevBuildings!=buildings||!state.typeCount){types(buildings,newRound);prevBuildings=buildings;}
  palette();
  // All lobby nodes, not just the single local Observer pointer: multiplayer supports multiple spectators.
  PtrList spectators;
  for(uint32_t node:vec(0xA8DA74,32))if(get<int>(node+0x6B)==-1)spectators.push_back(get<int>(node+0x6F,-1));
  PtrList houses;for(int i=0;i<start.count;++i)houses.push_back(start.houses[i].ptr);
  for(size_t k=0;k<houses.size();k++)if(houses[k]==local){for(size_t j=k;j>0;j--)houses[j]=houses[j-1];houses[0]=local;break;}
  for(uint32_t h:houses){
   const roundstate::House* identity=nullptr;for(int i=0;i<start.count;++i)if(start.houses[i].ptr==h)identity=&start.houses[i];
   int idx=identity->index;uint32_t ht=identity->type;
   bool spec=h==observer||std::find(spectators.begin(),spectators.end(),idx)!=spectators.end();
   uint8_t flags[13]{},passive=0;
   // Failed status reads must not turn defeated/loser flags into false.
   if(!bytes(h+492,flags,sizeof flags)||!bytes(ht+0x1A6,&passive,1)){state.readErrors++;continue;}
   if(!eligible(flags[1]||flags[0],flags[9],flags[10],flags[12],spec,passive))continue;
   if(state.playerCount>=MaxPlayers){state.truncated++;break;}
   Player& player=state.players[state.playerCount++];player={};player.ptr=h;player.houseIndex=idx;player.local=h==local;
   uint8_t rgb[3]={};bytes(h+22265,rgb,3);player.color=0xFF000000|(rgb[0]<<16)|(rgb[1]<<8)|rgb[2];
   memcpy(player.name,identity->name,40);player.name[20]=0;
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
  if(!roundstate::capture(read,end,probe_))return invalidate(tick);
  if(!end.follows(start)){
   probe_={external::layout::Stage::UnstableSnapshot,0xA80228,0};return invalidate(tick);
  }
  previous=end;haveRound=true;ticks++;return state;
 }
};
}
