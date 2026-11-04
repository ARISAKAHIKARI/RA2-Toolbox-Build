#pragma once
// Read-only native-unload observer. Both native and toolbox-initiated unloading
// produce the same verified transition. The only effect is an ordinary Harvest
// event through the existing planner admission/nativeMission gateway.
#include "ore_harvest_search.h"
#include "../../external_reader/source/map_bounds.h"
static int admitPlannerOrder(const planner::Order&,uint32_t);
template<class T>static bool oreHarvestRead(uint32_t at,T&value){return at>=0x10000&&uint64_t(at)+sizeof(T)<0x80000000ull&&read(at,&value,sizeof value);}
struct OreHarvestWorld {uint32_t match=0,scenario=0,house=0,houseID=0;int session=-1,frame=-1;};
static OreHarvestWorld oreHarvestWorld;
struct OreHarvestJob {
 uint32_t id=0,pointer=0,type=0,typeID=0,token=0,refinery=0,refineryPointer=0,refineryType=0,refineryTypeID=0;
 int originX=0,originY=0,speed=-1,attemptedFrame=-1;DWORD observedAt=0,readyAt=0;bool ready=false;unsigned retries=0;
 oreharvest::Search search{};
};
static OreHarvestJob oreHarvestJobs[ore::MaxJobs];static unsigned oreHarvestCursor=0;
struct OreHarvestMiner {uint32_t pointer=0,id=0,type=0,typeID=0,unloadToken=0;bool unloading=false;};
static OreHarvestMiner oreHarvestMiners[ore::MaxJobs*8];static int oreHarvestMinerCount=0;
static uint32_t oreHarvestUnitsHash=0;static long oreHarvestConfigRevision=-1;static int oreHarvestObservedFrame=-1;
static DWORD oreHarvestInventoryAt=0;static int oreHarvestInventoryFrame=-1;
struct OreHarvestMap {
 external::mapbounds::Bounds bounds{};uint32_t cells=0,overlays=0,tiberiums=0;int count=0,overlayCount=0,tiberiumCount=0;
 int value[4096]{};bool ready=false;
};
static OreHarvestMap oreHarvestMap;
static void oreHarvestReset(){oreHarvestWorld={};oreHarvestMap={};for(auto&j:oreHarvestJobs)j={};oreHarvestCursor=0;oreHarvestMinerCount=0;oreHarvestUnitsHash=0;oreHarvestConfigRevision=-1;oreHarvestObservedFrame=-1;oreHarvestInventoryAt=0;oreHarvestInventoryFrame=-1;}
static bool oreHarvestPending(){for(const auto&j:oreHarvestJobs)if(j.id)return true;return false;}
static bool oreHarvestNeedsWork(){for(const auto&j:oreHarvestJobs)if(j.id&&j.ready)return true;return false;}
static bool oreHarvestContext(uint32_t house,int frame,OreHarvestWorld&out,bool observation=false){
 out={};out.match=match;out.house=house;uint32_t actual=0;int actualFrame=-1;
 return house==lastHouse&&match&&snap.status==1&&(observation||snap.commandStatus==1)&&
  oreHarvestRead(0xA83D4C,actual)&&actual==house&&oreHarvestRead(house+0x10,out.houseID)&&out.houseID&&
  oreHarvestRead(0xA8B230,out.scenario)&&out.scenario>=0x10000&&oreHarvestRead(0xA8B238,out.session)&&out.session==snap.session&&
  oreHarvestRead(0xA8ED84,actualFrame)&&actualFrame==frame&&(out.frame=frame)>=0;
}
static bool oreHarvestSameWorld(const OreHarvestWorld&a,const OreHarvestWorld&b){return a.match==b.match&&a.scenario==b.scenario&&a.house==b.house&&a.houseID==b.houseID&&a.session==b.session;}
static bool oreHarvestInventory(int frame,DWORD now){
 if(!refreshOreConfig())return false;
 Vec units{};static uint32_t pointers[16384];
 if(!read(0x8B4108,&units,sizeof units)||units.count<0||units.count>16384||units.capacity<units.count||
    (units.count&&(units.items<0x10000||!read(units.items,pointers,size_t(units.count)*4))))return false;
 uint32_t hash=(2166136261u^units.items)*16777619u;hash=(hash^uint32_t(units.count))*16777619u;
 for(int n=0;n<units.count;++n)hash=(hash^pointers[n])*16777619u;
 // A non-miner can be destroyed and a new miner allocated at the same last
 // array slot between polls. Pointer sequence/count alone cannot reveal that
 // new UniqueID/type. Revalidate metadata at a bounded cadence, not every frame.
 const bool metadataFresh=oreHarvestInventoryFrame>=0&&frame>=oreHarvestInventoryFrame&&
  frame-oreHarvestInventoryFrame<30&&DWORD(now-oreHarvestInventoryAt)<500;
 if(hash==oreHarvestUnitsHash&&oreHarvestConfigRevision==oreConfigRevision&&metadataFresh)return true;
 static OreHarvestMiner nextMiners[ore::MaxJobs*8];int nextCount=0;
 for(int n=0;n<units.count&&nextCount<ore::MaxJobs*8;++n){uint32_t p=pointers[n],vt=0,id=0,type=0,typeID=0;int capacity=0;uint8_t flags[2]{};
  if(p<0x10000||!oreHarvestRead(p,vt)||vt!=0x7F5C70||!oreHarvestRead(p+0x10,id)||!id||!oreHarvestRead(p+0x6C4,type)||type<0x10000||
     !oreHarvestRead(type+0x10,typeID)||!typeID||!oreHarvestRead(type+0x800,capacity)||capacity<=0||!read(type+0xE0E,flags,sizeof flags)||
     !(oreConfig.customMiners||flags[0]||flags[1])||!oreTypeAllowed(type,true))continue;
  OreHarvestMiner candidate{p,id,type,typeID};
  // An unrelated birth/death may rebuild this inventory during unloading.
  // Preserve that episode's original input token; a later explicit Harvest
  // click must not be adopted as a new automatic unload episode.
  for(int old=0;old<oreHarvestMinerCount;++old){const auto&m=oreHarvestMiners[old];if(m.pointer==p&&m.id==id&&m.type==type&&m.typeID==typeID){candidate=m;break;}}
  nextMiners[nextCount++]=candidate;
 }
 oreHarvestMinerCount=nextCount;for(int n=0;n<nextCount;++n)oreHarvestMiners[n]=nextMiners[n];
 oreHarvestUnitsHash=hash;oreHarvestConfigRevision=oreConfigRevision;oreHarvestInventoryAt=now;oreHarvestInventoryFrame=frame;return true;
}
static bool oreHarvestMapCapture(){
 OreHarvestMap next{};next.bounds=external::mapbounds::sample([](uint32_t at,void*out,size_t n){return read(at,out,n);});
 Vec overlays=vec(0xA83D80,4096),tiberiums=vec(0xB0F4E8,64);
 if(!next.bounds.valid()||!oreHarvestRead(0x87F924,next.cells)||next.cells<0x10000||!oreHarvestRead(0x87F928,next.count)||next.count<=0||next.count>512*512||
    overlays.count<=0||tiberiums.count<=0)return false;
 next.overlays=overlays.items;next.overlayCount=overlays.count;next.tiberiums=tiberiums.items;next.tiberiumCount=tiberiums.count;
 // Cell::GetContainedTiberiumValue 485020 is Value*(OverlayData+1).
 // Data==0 is one remaining unit, not empty. Match registered resource ranges
 // without invoking 5FDD20's logging fallback or a native search/path routine.
 for(int n=0;n<tiberiums.count;++n){uint32_t type=0,image=0;int first=-1,flat=0,slopes=0,value=0;
  if(!oreHarvestRead(tiberiums.items+uint32_t(n)*4,type)||type<0x10000||!oreHarvestRead(type+0xE0,image)||image<0x10000||
     !oreHarvestRead(image+0x294,first)||!oreHarvestRead(type+0xE8,flat)||!oreHarvestRead(type+0xEC,slopes)||!oreHarvestRead(type+0xB8,value))return false;
  if(first<0||flat<0||slopes<0||flat>4096||slopes>4096||first>overlays.count-flat-slopes||value<=0)continue;
  for(int index=first;index<first+flat+slopes;++index){uint32_t overlay=0;uint8_t resource=0;
   if(!oreHarvestRead(overlays.items+uint32_t(index)*4,overlay)||overlay<0x10000||!oreHarvestRead(overlay+0x2A9,resource))return false;
   if(resource)next.value[index]=value;
  }
 }
 next.ready=true;oreHarvestMap=next;return true;
}
static bool oreHarvestMapCurrent(){
 if(!oreHarvestMap.ready)return false;uint32_t cells=0;int count=0;Vec overlays=vec(0xA83D80,4096),tiberiums=vec(0xB0F4E8,64);
 return oreHarvestRead(0x87F924,cells)&&cells==oreHarvestMap.cells&&oreHarvestRead(0x87F928,count)&&count==oreHarvestMap.count&&
  overlays.items==oreHarvestMap.overlays&&overlays.count==oreHarvestMap.overlayCount&&tiberiums.items==oreHarvestMap.tiberiums&&tiberiums.count==oreHarvestMap.tiberiumCount&&
  external::mapbounds::sample([](uint32_t at,void*out,size_t n){return read(at,out,n);})==oreHarvestMap.bounds;
}
static int oreHarvestCell(int x,int y,int speed,uint32_t*pointer=nullptr){
 if(pointer)*pointer=0;
 const auto&m=oreHarvestMap;if(!m.ready||speed<0||speed>=8||!m.bounds.legal(x,y))return 0;
 const uint32_t index=uint32_t(y*512+x);if(index>=uint32_t(m.count))return 0;
 uint32_t p=0,again=0;int overlay=-1;
 if(!oreHarvestRead(m.cells+index*4,p))return -1;
 if(!p)return 0; // A successfully read unallocated slot contains no resource.
 if(p<0x10000)return -1;
 if(!oreHarvestRead(p+0x44,overlay))return -1;
 if(overlay<0||overlay>=m.overlayCount||!m.value[overlay])return 0;
 int16_t xy[2]{};int land=-1;uint8_t ground[0x10]{};float multiplier=0;
 if(!read(p+0x24,xy,sizeof xy)||xy[0]!=x||xy[1]!=y||!oreHarvestRead(p+0xEC,land)||!read(p+0x118,ground,sizeof ground))return -1;
 uint32_t occupation=0;memcpy(&occupation,ground+0xC,4);
 if(land<0||land>=12||!m.bounds.usable(x,y,int(int8_t(ground[3])),ground[4]!=0)||(occupation&0x9Fu))return 0;
 if(!oreHarvestRead(0x89EA40u+uint32_t(land*9+speed)*4,multiplier))return -1;
 if(!std::isfinite(multiplier)||multiplier<=0||multiplier>=1000000)return 0;
 // Recheck the cell-array identity and native overlay; an exhausted overlay
 // becomes -1. No cell action/pathfinder/Enter/radio query is ever called.
 int after=-1;uint32_t nativeOverlay=0;uint8_t resource=0;
 if(!oreHarvestRead(m.cells+index*4,again)||again!=p||!oreHarvestRead(p+0x44,after)||after!=overlay||
    !oreHarvestRead(m.overlays+uint32_t(overlay)*4,nativeOverlay)||nativeOverlay<0x10000||!oreHarvestRead(nativeOverlay+0x2A9,resource))return -1;
 if(!resource)return 0;if(pointer)*pointer=p;return 1;
}
struct OreHarvestState {double cargo=-1;int mission=-1,queued=-1;bool unloading=false,onMap=false,limbo=true;uint32_t destination=0;};
static int oreHarvestState(const OreHarvestJob&j,uint32_t house,OreHarvestState&out){
 uint32_t vt=0,id=0,type=0,typeID=0,owner=0,carrier=0;int hp=0;uint8_t alive=0,disabled=0,unloading=0,onMap=0,limbo=1;float storage[4]{};
 if(!oreHarvestRead(j.pointer,vt)||!oreHarvestRead(j.pointer+0x10,id)||!oreHarvestRead(j.pointer+0x6C,hp)||
    !oreHarvestRead(j.pointer+0x21C,owner)||!oreHarvestRead(j.pointer+0x6C4,type)||!oreHarvestRead(j.pointer+0x90,alive)||
    !oreHarvestRead(j.pointer+0x1C8,disabled)||!oreHarvestRead(j.pointer+0x11C,carrier)||!oreHarvestRead(j.pointer+0x6D1,unloading)||
    !oreHarvestRead(j.pointer+0x74,onMap)||!oreHarvestRead(j.pointer+0x81,limbo)||!read(j.pointer+0x33C,storage,sizeof storage)||
    !oreHarvestRead(j.pointer+0xAC,out.mission)||!oreHarvestRead(j.pointer+0xB4,out.queued)||!oreHarvestRead(j.pointer+0x5A4,out.destination))return 0;
 if(vt!=0x7F5C70||id!=j.id||type!=j.type||owner!=house||hp<=0||!alive||disabled||carrier||manualVersions.get(j.id)!=j.token||manualVersions.orePaused(j.id))return -1;
 if(!oreHarvestRead(type+0x10,typeID))return 0;if(typeID!=j.typeID)return -1;
 out.cargo=ore::storageAmount(storage);if(out.cargo<0)return 0;out.unloading=unloading!=0;out.onMap=onMap!=0;out.limbo=limbo!=0;return 1;
}
static int oreHarvestRefinery(const OreHarvestJob&j,uint32_t house){
 uint32_t id=0,owner=0,type=0,typeID=0,links=0;int hp=0,count=0;uint8_t alive=0,onMap=0,limbo=1;
 if(!oreHarvestRead(j.refineryPointer+0x10,id)||!oreHarvestRead(j.refineryPointer+0x21C,owner)||!oreHarvestRead(j.refineryPointer+0x520,type)||
    !oreHarvestRead(j.refineryPointer+0x6C,hp)||!oreHarvestRead(j.refineryPointer+0x90,alive)||!oreHarvestRead(j.refineryPointer+0x74,onMap)||
    !oreHarvestRead(j.refineryPointer+0x81,limbo)||!oreHarvestRead(j.refineryPointer+0xE8,count)||!oreHarvestRead(j.refineryPointer+0xE4,links))return -2;
 if(id!=j.refinery||owner!=house||type!=j.refineryType||hp<=0||!alive||!onMap||limbo)return -1;
 if(!oreHarvestRead(type+0x10,typeID))return -2;if(typeID!=j.refineryTypeID)return -1;
 if(count<0||count>256||(count&&links<0x10000))return -2;
 for(int n=0;n<count;++n){uint32_t link=0;if(!oreHarvestRead(links+uint32_t(n)*4,link))return -2;if(link==j.pointer)return 1;}
 return 0;
}
static bool oreHarvestAutomatic(const OreHarvestState&s){
 // A missed manual Move/Attack is still protected, in addition to the ordinary
 // per-actor manual token. Only native mining/idle continuations may hand back.
 // Native unload exit (73E0E3/73E201) preserves every queued mission other
 // than None/Harvest, even when an input hook did not observe that order.
 return (ore::automaticMission(s.mission)||(s.mission==2&&s.queued==10))&&(s.queued==-1||s.queued==10);
}
static bool oreHarvestOrderValid(const planner::Order&o,uint32_t house){
 if(!sh||sh->stop||!(sh->flags&AutoOreReturn)||o.purpose!=planner::OreHarvestOrder||o.mission!=10||o.target||o.destinationTag!=11||o.pursuit)return false;
 OreHarvestWorld world{};int frame=-1;if(!oreHarvestRead(0xA8ED84,frame)||!oreHarvestContext(house,frame,world)||!oreHarvestSameWorld(world,oreHarvestWorld)||world.frame<oreHarvestWorld.frame||!oreHarvestMapCurrent())return false;
 for(const auto&j:oreHarvestJobs)if(j.id==o.unit&&j.token==o.token&&j.refinery==o.refinery&&j.ready&&j.search.done&&j.search.best==o.destination){
  OreHarvestState s{};return oreHarvestState(j,house,s)==1&&s.cargo<=0.01&&!s.unloading&&s.onMap&&!s.limbo&&oreHarvestAutomatic(s)&&
   oreHarvestRefinery(j,house)==0&&oreHarvestCell(int(o.destination%1000),int(o.destination/1000),j.speed)==1;
 }return false;
}
static bool oreHarvestBegin(const OreHarvestMiner&miner,uint32_t house,DWORD now){
 OreHarvestJob*slot=nullptr;for(auto&j:oreHarvestJobs){if(j.id==miner.id)return true;if(!j.id&&!slot)slot=&j;}if(!slot)return false;
 OreHarvestJob next{};next.id=miner.id;next.pointer=miner.pointer;next.type=miner.type;next.typeID=miner.typeID;next.token=manualVersions.get(miner.id);next.observedAt=now;
 OreHarvestState state{};if(oreHarvestState(next,house,state)!=1||!state.unloading)return false;
 int x=0,y=0;if(!oreHarvestRead(miner.pointer+0x9C,x)||!oreHarvestRead(miner.pointer+0xA0,y))return false;
 Vec buildings=vec(0xA8EB40,16384);int64_t distance=INT64_MAX;
 for(int n=0;n<buildings.count;++n){uint32_t rp=0,id=0,owner=0,type=0,typeID=0;int rx=0,ry=0;uint8_t styleB=0;
  if(!oreHarvestRead(buildings.items+uint32_t(n)*4,rp)||rp<0x10000||!oreHarvestRead(rp+0x10,id)||!id||!oreHarvestRead(rp+0x21C,owner)||owner!=house||
     !oreHarvestRead(rp+0x520,type)||type<0x10000||!oreHarvestRead(type+0x10,typeID)||!rawOreCompatible(miner.pointer,rp)||
     !oreHarvestRead(rp+0x9C,rx)||!oreHarvestRead(rp+0xA0,ry)||!oreHarvestRead(type+0x16E5,styleB))continue;
  OreHarvestJob candidate=next;candidate.refinery=id;candidate.refineryPointer=rp;candidate.refineryType=type;candidate.refineryTypeID=typeID;
  const int linked=oreHarvestRefinery(candidate,house);if(linked<0)continue;
  const int64_t d=ore::dockDistance(x,y,rx,ry,styleB!=0);if(!linked&&state.destination!=rp&&d>ore::NearDockCells)continue;
  const int64_t rank=linked?-2:state.destination==rp?-1:d;
  if(rank<distance||(rank==distance&&(!next.refinery||id<next.refinery))){
   distance=rank;next=candidate;next.originX=rx/256+(styleB?2:1);next.originY=ry/256+2;
  }
 }
 if(!next.refinery||next.originX<=0||next.originX>=512||next.originY<=0||next.originY>=512||
    !oreHarvestRead(next.type+0x67C,next.speed)||next.speed<0||next.speed>=8)return false;
 *slot=next;return true;
}
// This lightweight observer runs BEFORE the dispatcher's display/world cadence
// return. It does not build planningWorld, sample waiting rings, scan map cells
// or send an event. Inventory pointer-array changes refresh only miner metadata.
static void oreHarvestObserve(uint32_t house,int frame,DWORD now){
 if(!sh||sh->stop||fault||!(sh->flags&AutoOreReturn)){if(oreHarvestWorld.match||oreHarvestPending())oreHarvestReset();return;}
 OreHarvestWorld world{};if(!oreHarvestContext(house,frame,world,true))return;
 if(!oreHarvestSameWorld(world,oreHarvestWorld)||world.frame<oreHarvestWorld.frame){oreHarvestReset();oreHarvestWorld=world;}else oreHarvestWorld.frame=world.frame;
 if(oreHarvestObservedFrame==frame)return;oreHarvestObservedFrame=frame;
 if(oreHarvestInventory(frame,now))for(int n=0;n<oreHarvestMinerCount;++n){auto&m=oreHarvestMiners[n];uint32_t owner=0,id=0,type=0,typeID=0;uint8_t unloading=0;
  if(!oreHarvestRead(m.pointer+0x10,id)||!oreHarvestRead(m.pointer+0x6C4,type)||!oreHarvestRead(m.type+0x10,typeID))continue;
  if(id!=m.id||type!=m.type||typeID!=m.typeID){oreHarvestUnitsHash=0;continue;}
  if(!oreHarvestRead(m.pointer+0x21C,owner)||!oreHarvestRead(m.pointer+0x6D1,unloading))continue;
  if(unloading&&!m.unloading)m.unloadToken=manualVersions.get(m.id);m.unloading=unloading!=0;
  if(unloading&&owner==house&&m.unloadToken==manualVersions.get(m.id))oreHarvestBegin(m,house,now);
 }
 for(auto&j:oreHarvestJobs)if(j.id){
  if(DWORD(now-j.observedAt)>=15000||(j.ready&&DWORD(now-j.readyAt)>=8000)){j={};continue;}
  OreHarvestState state{};const int valid=oreHarvestState(j,house,state),refinery=oreHarvestRefinery(j,house);
  if(valid<0||refinery==-1){j={};continue;}if(!valid||refinery==-2)continue;
  if(state.unloading){j.observedAt=now;j.ready=false;j.search={};continue;}
  if(!oreHarvestAutomatic(state)){j={};continue;}
  if(state.cargo>0.01){if(j.ready)j={};continue;}
  if(!state.onMap||state.limbo||refinery)continue;
  if(!j.ready){j.ready=true;j.readyAt=now;j.search.begin(j.originX,j.originY);}
 }
}
static void oreHarvestTick(uint32_t house,int frame,DWORD now){
 oreHarvestObserve(house,frame,now);
 if(!oreHarvestNeedsWork())return;
 OreHarvestWorld world{};if(!oreHarvestContext(house,frame,world)||!oreHarvestSameWorld(world,oreHarvestWorld))return;
 if(!oreHarvestMapCurrent()){
  if(!oreHarvestMapCapture())return;
  for(auto&j:oreHarvestJobs)if(j.id&&j.ready)j.search.begin(j.originX,j.originY);
 }
 // Global bounded work, not a per-miner full-map scan. Only post-unload jobs
 // search, and a completed exact-nearest ring proof does not re-run every frame.
 int budget=2048;for(unsigned n=0;n<ore::MaxJobs*8&&budget>0;++n){unsigned index=(oreHarvestCursor+n)%ore::MaxJobs;auto&j=oreHarvestJobs[index];if(!j.id||!j.ready)continue;
  int local=std::min(budget,256),before=local;j.search.advance(local,[&](int x,int y){return oreHarvestCell(x,y,j.speed);});budget-=before-local;
  if(!j.search.done||j.attemptedFrame==frame)continue;j.attemptedFrame=frame;
  if(!j.search.best){j={};continue;}
  OreHarvestState state{};uint32_t target=0;
  if(oreHarvestState(j,house,state)==1&&state.mission==10&&oreHarvestCell(int(j.search.best%1000),int(j.search.best/1000),j.speed,&target)==1&&state.destination==target){j={};continue;}
  if(snap.chat)continue;
  const planner::Order order{j.id,j.token,0,j.search.best,10,11,planner::OreHarvestOrder,j.refinery};
  const int accepted=admitPlannerOrder(order,house);
  if(accepted>0)j={};
  else if(accepted<0){if(++j.retries>=3)j={};else j.search.begin(j.originX,j.originY);}
 }oreHarvestCursor=(oreHarvestCursor+1)%ore::MaxJobs;
}
