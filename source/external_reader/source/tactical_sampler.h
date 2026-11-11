#pragma once
#include "../../assist/source/protocol.h"
#include <algorithm>
#include <cmath>
#include <vector>
#include <array>
#include <cstring>
#include <unordered_set>
#include "game_view.h"
#include "layout_probe.h"
#include "catalogue_stamp.h"
#include "display_state.h"
#include "enemy_markers.h"
#include "unit_status.h"
#include "projection.h"
#include "../../shared/source/native_viewport_projection.h"
#include "idle_aircraft.h"
#include <unordered_map>
namespace external {
template<class Read> class TacticalSampler {
 Read read_;assist::Snapshot state_{};layout::Result probe_{};uint32_t house_=0,serial_=0;int lastFrame_=-1;
 uint32_t sharedMatch_=0;
 catalogue::Dependencies catalogueDependencies_;
 uint64_t catalogueHash_=0;uint32_t catalogueAt_=0;bool catalogueReady_=false;
 int sceneFrame_=-1,sceneX_=0,sceneY_=0,sceneWidth_=0,sceneHeight_=0;uint32_t sceneFlags_=0,selectionHash_=0,sceneAt_=0;
 struct LabelSource {uint32_t object=0,id=0,type=0,owner=0;int kind=-1;bool selected=false;};
 LabelSource labelSources_[assist::MaxLabels]{};display::State display_{};
 nativeviewportprojection::Transform viewport_{},sampledViewport_{};bool viewportReady_=true;
 status::Frame statuses_{},publishedStatuses_{};uint32_t statusOptions_=0,sceneStatusOptions_=0;
 enemies::Frame enemies_{},publishedEnemies_{};bool sceneEnemies_=false,specialOnly_=false,sceneSpecialOnly_=false;uint32_t sceneSourceSerial_=0;int sceneEnemyWidth_=0,sceneEnemyHeight_=0;uint32_t enemyOwnerSignature_=0,sceneEnemyOwners_=0;
 struct EnemyOwner {uint32_t pointer=0,color=0;int index=-1;};
 EnemyOwner enemyOwners_[32]{};int enemyOwnerCount_=0;
 struct EnemyTypeName {uint16_t text[40]{};};std::unordered_map<uint32_t,EnemyTypeName> enemyNames_;
 uint32_t typePointers_[assist::MaxTypes]{};std::unordered_set<uint32_t> stopped_;
 struct FactoryState {uint32_t pointer=0,product=0,changed=0;int progress=0,balance=0;bool seen=false;};FactoryState factories_[6];uint32_t productionAt_=0;
 uint32_t idleLines_=0,pausedLines_=0,activeTabs_=0;
 aircraft::Sample aircraftSample_{},publishedAircraft_{},failedAircraftRefresh_{};std::array<uint32_t,8192>publishedAircraftIDs_{};bool aircraftChanged_=false;
 void clearAircraftCandidate(){idleAircraftTick=0;idleAircraftCount=aircraftTotal=0;aircraftSample_={};aircraftChanged_=true;}
 bool captureAircraft(uint32_t house,int frame,uint32_t now,uint32_t sourceSerial){
  clearAircraftCandidate();bool complete=true;auto checked=[&](uint32_t at,void*out,size_t n){const bool ok=read_(at,out,n);complete&=ok;return ok;};
  GameView<decltype(checked)> view(checked);std::vector<uint32_t>objects;if(!view.vecChecked(0xA8E390,objects,8192))return false;
  aircraft::Inspect<decltype(checked)>planes{checked};
  for(uint32_t p:objects)if(planes.valid(p,house)){++aircraftTotal;if(planes.idle(p,house)){uint32_t id=0;if(checked(p+0x10,&id,4)&&id)idleAircraftIDs[idleAircraftCount++]=id;}}
  if(!complete){clearAircraftCandidate();return false;}
  std::sort(idleAircraftIDs,idleAircraftIDs+idleAircraftCount);idleAircraftTick=now;
  aircraftSample_={state_.match,now,sourceSerial,frame,idleAircraftCount,aircraftTotal};return true;
 }
 template<class R>static auto current(R r,uint32_t p,void*out,size_t n,int)->decltype(r.live(p,out,n)){return r.live(p,out,n);}
 template<class R>static bool current(R r,uint32_t p,void*out,size_t n,long){return r(p,out,n);}
 template<class T>T uiValue(uint32_t p,T fallback={}){T value{};return current(read_,p,&value,sizeof value,0)?value:fallback;}
 template<class T>T get(uint32_t a,T fallback={}){T v{};return read_(a,&v,sizeof v)?v:fallback;}
 std::vector<uint32_t> vec(uint32_t a,int maximum=16384){
  if(a==0xA8ECB8){ // Selection is input state, independent of world capture.
   uint32_t items=uiValue<uint32_t>(a+4);int cap=uiValue<int>(a+8,-1),n=uiValue<int>(a+16,-1);
   if(n<0||n>maximum||cap<n||cap>65536||(n&&items<0x10000))return {};
   std::vector<uint32_t>out(size_t(n),0);if(n&&!current(read_,items,out.data(),size_t(n)*4,0))out.clear();return out;
  }
  uint32_t items=get<uint32_t>(a+4);int cap=get<int>(a+8,-1),n=get<int>(a+16,-1);
  if(n<0||n>maximum||cap<n||cap>65536||(n&&items<0x10000))return {};
  std::vector<uint32_t> out(size_t(n),0);if(n&&!read_(items,out.data(),size_t(n)*4))out.clear();return out;
 }
 int kind(uint32_t p){uint32_t v=get<uint32_t>(p);return v==0x7F5C70?2:v==0x7EB058?1:v==0x7E3EBC?3:v==0x7E22A4?0:-1;}
 uint32_t type(uint32_t p,int k){return get<uint32_t>(p+(k==3?0x520:k==1?0x6C0:0x6C4));}
 void name(uint32_t type,uint16_t*out){uint32_t p=get<uint32_t>(type+0x60);uint16_t s[40]{};if(p>=0x10000&&read_(p,s,sizeof s)){std::memcpy(out,s,sizeof s);out[39]=0;}else{char id[24]{};read_(type+0x24,id,23);for(int j=0;j<23;++j)out[j]=uint8_t(id[j]);}}
 const EnemyOwner* enemyOwner(uint32_t owner)const{
  for(int i=0;i<enemyOwnerCount_;++i)if(enemyOwners_[i].pointer==owner)return &enemyOwners_[i];return nullptr;
 }
 void enemyOwners(const std::vector<uint32_t>&houses,uint32_t local){
  enemyOwnerCount_=0;enemyOwnerSignature_=2166136261u;uint32_t allies=get<uint32_t>(local+0x5788,0xffffffffu);
  for(uint32_t owner:houses){if(owner<0x10000||owner==local||enemyOwnerCount_>=32)continue;
   int index=get<int>(owner+0x30,-1);uint32_t country=get<uint32_t>(owner+0x34);
   if(index<0||index>=32||(allies&(1u<<index))||country<0x10000||get<uint8_t>(country+0x1A6,1))continue;
   uint8_t rgb[3]={255,90,90};read_(owner+22265,rgb,3);
   enemyOwners_[enemyOwnerCount_++]={owner,0xff000000u|(uint32_t(rgb[0])<<16)|(uint32_t(rgb[1])<<8)|rgb[2],index};
   enemyOwnerSignature_=(enemyOwnerSignature_^owner)*16777619u;
  }
 }
 bool refreshEnemies(const Projection&proj){
  for(int i=0;i<enemies_.count;++i){auto&m=enemies_.items[i];const auto*owner=enemyOwner(m.owner);
   if(!owner||kind(m.object)!=m.kind||get<uint32_t>(m.object+0x10)!=m.id||type(m.object,m.kind)!=m.type||
      get<uint32_t>(m.object+0x21C)!=m.owner||get<int>(m.object+0x6C)<=0||!get<uint8_t>(m.object+0x74)||
      get<uint8_t>(m.object+0x81)||!get<uint8_t>(m.object+0x90)||(m.kind!=3&&get<uint32_t>(m.object+0x11C,0xffffffffu)))return false;
   int x=INT32_MIN,y=INT32_MIN;
   m.worldX=get<int>(m.object+0x9C,INT32_MIN);m.worldY=get<int>(m.object+0xA0,INT32_MIN);
   project(proj,m.worldX,m.worldY,get<int>(m.object+0xA4,INT32_MIN),x,y);
   if(x==INT32_MIN||y==INT32_MIN)return false; // Offscreen coordinates remain valid.
   if(specialOnly_&&!specialEnemy(m.object,m.type,m.kind))return false;
   m.x=x;m.y=y;m.color=owner->color;m.player=owner->index;
  }return true;
 }
 bool specialEnemy(uint32_t p,uint32_t t,int k){return enemies::special(read_,p,t,k);}
 void addEnemy(uint32_t p,uint32_t t,int k,uint32_t owner,const Projection&proj){
  const auto*enemy=enemyOwner(owner);if(!enemy||t<0x10000||(k!=3&&get<uint32_t>(p+0x11C,0xffffffffu)))return;
  if(specialOnly_&&!specialEnemy(p,t,k))return;
  uint32_t id=get<uint32_t>(p+0x10);if(!id)return;int x,y;
  int worldX=get<int>(p+0x9C,INT32_MIN),worldY=get<int>(p+0xA0,INT32_MIN);
  if(!project(proj,worldX,worldY,get<int>(p+0xA4,INT32_MIN),x,y))return;
  if(enemies_.count>=enemies::Maximum){++enemies_.omitted;return;}
  auto&mark=enemies_.items[enemies_.count++];mark={};mark.object=p;mark.id=id;mark.type=t;mark.owner=owner;
  mark.color=enemy->color;mark.player=enemy->index;mark.kind=k;mark.x=x;mark.y=y;mark.worldX=worldX;mark.worldY=worldY;
  auto found=enemyNames_.find(t);if(found==enemyNames_.end()){
   if(enemyNames_.size()>=1024)enemyNames_.clear();EnemyTypeName value{};name(t,value.text);found=enemyNames_.emplace(t,value).first;
  }std::memcpy(mark.name,found->second.text,sizeof mark.name);
 }
 void publishEnemies(uint32_t house,int frame,uint32_t now,bool enabled){
  enemies_.match=state_.match;enemies_.house=house;enemies_.frame=frame;enemies_.tick=now;
  if(!enabled){enemies_.count=0;return;}
  auto latest=[&](uint32_t at,void*out,size_t n){return current(read_,at,out,n,0);};
  int after=-1;uint32_t currentHouse=0,tactical=0;int cameraX=INT32_MIN,cameraY=INT32_MIN;uint8_t started=0;
  bool valid=latest(0xA8ED84,&after,4)&&latest(0xA83D4C,&currentHouse,4)&&latest(0xA8E9A0,&started,1)&&
   latest(0x887324,&tactical,4)&&tactical>=0x10000&&latest(tactical+0xB0,&cameraX,4)&&latest(tactical+0xB4,&cameraY,4);
  if(!valid||!started||currentHouse!=house||after<frame||after-frame>2||!display::inspect(latest).visible()){
   enemies_.count=statuses_.count=0;sceneFrame_=-1;
  }else{
   // Scrolling during a read is not an object/epoch change. Keep the same
   // sample and move every overlay into the latest viewport together.
   // Dropping only enemy/status data here caused visible gaps and full rescans.
   rebaseCamera(cameraX,cameraY);
  }
 }
 bool readStatus(uint32_t p,uint32_t t,int k,uint32_t owner,const Projection&proj,status::Marker&out){
  out={};out.object=p;out.id=get<uint32_t>(p+0x10);out.type=t;out.owner=owner;out.kind=k;
  if(!out.id||!status::inspect(read_,p,k,statusOptions_,out))return false;
  int wx=get<int>(p+0x9C,INT32_MIN),wy=get<int>(p+0xA0,INT32_MIN),wz=get<int>(p+0xA4,INT32_MIN);
  if(!project(proj,wx,wy,wz,out.x,out.y))return false;
  if(wx<0||wy<0||wx>=512*256||wy>=512*256)return false;
  return true;
 }
 bool refreshStatuses(const Projection&proj){
  for(int i=0;i<statuses_.count;++i){auto&mark=statuses_.items[i];uint32_t p=mark.object;
   if(kind(p)!=mark.kind||get<uint32_t>(p+0x10)!=mark.id||type(p,mark.kind)!=mark.type||
      get<int>(p+0x6C)<=0||!get<uint8_t>(p+0x74)||get<uint8_t>(p+0x81)||!get<uint8_t>(p+0x90))return false;
   status::Marker fresh{};if(!readStatus(p,mark.type,mark.kind,get<uint32_t>(p+0x21C),proj,fresh))return false;
   mark=fresh;
  }return true;
 }
 void publishStatuses(uint32_t house,int frame,uint32_t now){
  statuses_.match=state_.match;statuses_.house=house;statuses_.tick=now;statuses_.frame=frame;
  if(!statusOptions_){statuses_.count=0;return;}
  auto latest=[&](uint32_t at,void*out,size_t n){return current(read_,at,out,n,0);};
  int after=-1;uint32_t currentHouse=0,tactical=0;int cameraX=INT32_MIN,cameraY=INT32_MIN;uint8_t started=0;
  if(!latest(0xA8ED84,&after,4)||!latest(0xA83D4C,&currentHouse,4)||!latest(0xA8E9A0,&started,1)||
     !latest(0x887324,&tactical,4)||tactical<0x10000||!latest(tactical+0xB0,&cameraX,4)||!latest(tactical+0xB4,&cameraY,4)||
     !started||currentHouse!=house||after<frame||after-frame>2||cameraX!=state_.cameraX||cameraY!=state_.cameraY||!display::inspect(latest).visible()){
   statuses_.count=0;sceneFrame_=-1;
  }
 }
 int frozen(uint32_t object){
  uint32_t temporal=get<uint32_t>(object+0x278);if(temporal<0x10000||get<uint32_t>(temporal+0x28)!=object)return 0;
  int remaining=get<int>(temporal+0x48);if(remaining<=0)return 0;
  uint32_t seen[52]{};int count=0;int64_t rate=0;
  // Mirrors the raw helper chain used by GetWarpPerStep; never calls it (it writes WarpPerStep).
  for(uint32_t p=temporal;p>=0x10000&&count<52;p=get<uint32_t>(p+0x44)){
   bool loop=false;for(int j=0;j<count;++j)if(seen[j]==p)loop=true;if(loop||get<uint32_t>(p+0x28)!=object)break;seen[count++]=p;
   int step=get<int>(p+0x4C);if(step>0&&step<1000000)rate+=step;
  }
  if(!rate)return 0;return int(std::min<int64_t>(1000000,(remaining+rate-1)/rate));
 }
 int passengers(uint32_t p,int cap){
  int used=0;uint32_t q=get<uint32_t>(p+0x118),seen[256]{};int count=0;
  while(q>=0x10000&&count<256){bool loop=false;for(int j=0;j<count;++j)if(seen[j]==q)loop=true;if(loop)break;seen[count++]=q;
   int qk=kind(q);if(qk<0)break;uint32_t qt=type(q,qk);double size=get<double>(qt+0x380);if(!(size>0&&size<=128))break;
   used+=int(size);q=get<uint32_t>(q+0x30);
  }return std::min(used,cap);
 }
 // The object list/type catalogue follow simulation frames. Visible positions
 // and passenger counts follow the host display clock, including same-frame
 // changes. Revalidate identity/liveness before using a cached object pointer.
 bool refreshLabels(const Projection&proj){
  for(int i=0;i<state_.labels;++i){const auto&ref=labelSources_[i];auto&l=state_.label[i];uint32_t p=ref.object;
   if(kind(p)!=ref.kind||get<uint32_t>(p+0x10)!=ref.id||type(p,ref.kind)!=ref.type||get<uint32_t>(p+0x21C)!=ref.owner||
      get<int>(p+0x6C)<=0||!get<uint8_t>(p+0x74)||get<uint8_t>(p+0x81)||!get<uint8_t>(p+0x90))return false;
   int x=INT32_MIN,y=INT32_MIN;project(proj,get<int>(p+0x9C),get<int>(p+0xA0),get<int>(p+0xA4),x,y);
   // Off-screen is not an invalid object. A camera edge must not trigger a
   // complete object-array scan at every 2 ms display sample.
   if(x==INT32_MIN||y==INT32_MIN)return false;
   l.x=x;l.y=y;
   if(l.capacity){if(get<int>(ref.type+0x5E0)!=l.capacity)return false;int used=passengers(p,l.capacity);
    if(ref.selected)state_.used+=used-l.used;l.used=used;
   }
  }return true;
 }
 void types(){
  state_.typeCount=0;uint32_t arrays[]={0xA8B218,0xA8E348,0xA83CE0,0xA83C68};int kinds[]={3,16,40,7};
  for(int c=0;c<4;++c){auto list=vec(arrays[c],4096);for(size_t j=0;j<list.size()&&state_.typeCount<assist::MaxTypes;++j){auto p=list[j];
   if(!p||!assist::validType(get<int>(p+0x634,-1),get<uint8_t>(p+0x232)!=0,get<uint8_t>(p+0xD54)!=0,get<uint32_t>(p+0x6CC)))continue;
   typePointers_[state_.typeCount]=p;auto&t=state_.types[state_.typeCount++];t={};read_(p+0x24,t.id,23);name(p,t.name);t.kind=kinds[c];t.index=int(j);t.naval=get<uint8_t>(p+0xCCE);t.cost=get<int>(p+0x610);
  }}
 }
public:
 uint32_t idleProductionMask=0,pausedProductionMask=0,idleAircraftTick=0;int idleAircraftCount=0,aircraftTotal=0;uint32_t idleAircraftIDs[8192]{};int productionTabsY=168;
 int flashCount=0,flashes[4]{};
 explicit TacticalSampler(Read read):read_(read){}
 layout::Result probe()const{return probe_;}
 display::State displayState()const{return display_;}
 const enemies::Frame& enemyMarkers()const{return enemies_;}
 const status::Frame& unitStatuses()const{return statuses_;}
 const aircraft::Sample& idleAircraftDisplay()const{return publishedAircraft_;}
 bool refreshAircraft(uint32_t sourceTick,uint32_t sourceSerial){
  if(state_.status!=1||!house_)return false;
  // A missing page cannot recover in the same sealed publication. Remember
  // only failed derived attempts; a new source or full sample may try again.
  if(failedAircraftRefresh_.frame==state_.frame&&failedAircraftRefresh_.match==state_.match&&failedAircraftRefresh_.serial==sourceSerial)return false;
  if(captureAircraft(house_,state_.frame,sourceTick,sourceSerial)){failedAircraftRefresh_={};return true;}
  failedAircraftRefresh_={state_.match,sourceTick,sourceSerial,state_.frame};return false;
 }
 const nativeviewportprojection::Transform&sampledViewport()const{return sampledViewport_;}
 void viewport(const nativeviewportprojection::Transform&t,bool ready=true){if(viewport_!=t||viewportReady_!=ready)sceneFrame_=-1;viewport_=t;viewportReady_=ready;}
 void productionReminders(uint32_t enabled,uint32_t&idle,uint32_t&paused)const{idle=paused=0;for(int i=0;i<6;++i){unsigned tab=i<4?i:3;if(!(enabled&(1u<<i))||(activeTabs_&(1u<<tab)))continue;if(idleLines_&(1u<<i))idle|=1u<<tab;if(pausedLines_&(1u<<i))paused|=1u<<tab;}}
 void reset(){catalogueDependencies_.clear();idleLines_=pausedLines_=activeTabs_=0;idleProductionMask=pausedProductionMask=idleAircraftTick=0;idleAircraftCount=aircraftTotal=0;aircraftSample_=publishedAircraft_=failedAircraftRefresh_={};aircraftChanged_=false;probe_={};catalogueHash_=0;catalogueAt_=0;catalogueReady_=false;house_=serial_=0;lastFrame_=-1;state_={};sceneFrame_=-1;sceneFlags_=selectionHash_=sceneAt_=0;flashCount=0;productionAt_=0;for(auto&f:factories_)f={};stopped_.clear();enemies_=publishedEnemies_={};statuses_=publishedStatuses_={};statusOptions_=sceneStatusOptions_=0;sceneEnemies_=false;enemyNames_.clear();enemyOwnerCount_=0;}
 void stopped(const std::vector<uint32_t>&ids){stopped_.clear();stopped_.insert(ids.begin(),ids.end());}
 const assist::Snapshot&state()const{return state_;}
 void worldEpoch(uint32_t epoch){if(epoch!=sharedMatch_){catalogueDependencies_.clear();sceneFrame_=-1;productionAt_=0;clearAircraftCandidate();publishedAircraft_=failedAircraftRefresh_={};aircraftChanged_=false;}sharedMatch_=epoch;}
 void commitSample(){publishedEnemies_=enemies_;publishedStatuses_=statuses_;
  if(aircraftChanged_){publishedAircraft_=aircraftSample_;std::copy_n(idleAircraftIDs,idleAircraftCount,publishedAircraftIDs_.begin());aircraftChanged_=false;}}
 void invalidateSample(const assist::Snapshot*stable=nullptr){sceneFrame_=-1;catalogueReady_=false;catalogueHash_=0;sceneAt_=0;productionAt_=0;for(auto&f:factories_)f={};enemyNames_.clear();enemyOwnerCount_=0;
  if(stable){state_=*stable;enemies_=publishedEnemies_;statuses_=publishedStatuses_;}else enemies_.count=statuses_.count=0;
  if(stable&&stable->match==publishedAircraft_.match){aircraftSample_=publishedAircraft_;idleAircraftTick=publishedAircraft_.tick;idleAircraftCount=publishedAircraft_.count;aircraftTotal=publishedAircraft_.total;std::copy_n(publishedAircraftIDs_.begin(),idleAircraftCount,idleAircraftIDs);}
  else{clearAircraftCandidate();publishedAircraft_={};}aircraftChanged_=false;}
 void refreshCamera(uint32_t now){
  if(state_.status!=1)return;catalogueDependencies_.maintain(read_);uint32_t tactical=uiValue<uint32_t>(0x887324);if(tactical<0x10000)return;
  rebaseCamera(uiValue<int>(tactical+0xB0),uiValue<int>(tactical+0xB4));
 }
 void rebaseCamera(int x,int y){
  const int64_t dx=int64_t(x)-state_.cameraX,dy=int64_t(y)-state_.cameraY;
  if(!dx&&!dy)return;state_.cameraX=x;state_.cameraY=y; // Camera-only updates cannot freshen the object sample.
  auto shift=[](int&v,int64_t delta){v=int(std::clamp(int64_t(v)-delta,int64_t(INT32_MIN),int64_t(INT32_MAX)));};
  for(int i=0;i<state_.labels;++i){shift(state_.label[i].x,dx);shift(state_.label[i].y,dy);}
  for(int i=0;i<state_.crateCount;++i){shift(state_.crates[i].x,dx);shift(state_.crates[i].y,dy);}
  for(int i=0;i<state_.threatCount;++i){auto&t=state_.threats[i];shift(t.x,dx);shift(t.y,dy);shift(t.tx,dx);shift(t.ty,dy);}
  for(int i=0;i<enemies_.count;++i){shift(enemies_.items[i].x,dx);shift(enemies_.items[i].y,dy);}
  for(int i=0;i<statuses_.count;++i){shift(statuses_.items[i].x,dx);shift(statuses_.items[i].y,dy);}
 }
 const assist::Snapshot&sample(uint32_t now,uint32_t flags,bool enemyMarkers=false,uint32_t statusOptions=0,uint32_t sourceSerial=0,bool specialOnly=false){
  specialOnly_=specialOnly;
  failedAircraftRefresh_={};
  auto ui=[&](uint32_t at,void*out,size_t n){return current(read_,at,out,n,0);};
  statusOptions_=statusOptions&3;state_.sampledTick=now;state_.renderStatus=0;display_=display::inspect(ui);
  sampledViewport_=viewport_;if(!viewportReady_){state_.labels=state_.threatCount=state_.crateCount=0;enemies_.count=statuses_.count=0;return state_;}
  probe_=layout::inspect(ui);
  if(probe_.stage!=layout::Stage::Ready){
   enemies_.count=statuses_.count=0;
   idleProductionMask=pausedProductionMask=0;clearAircraftCandidate();state_.status=0;state_.labels=state_.selected=state_.transports=state_.used=state_.capacity=0;
   state_.typeCount=state_.swCount=state_.threatCount=state_.crateCount=0;
   house_=0;catalogueReady_=false;catalogueDependencies_.clear();return state_;
  }
  uint32_t house=get<uint32_t>(0xA83D4C);int frame=get<int>(0xA8ED84,-1);
  auto houses=vec(0xA80228,32);if(std::find(houses.begin(),houses.end(),house)==houses.end()){
   enemies_.count=statuses_.count=0;
   probe_={layout::Stage::MissingLocalHouseInList,0xA80228,house};
   idleProductionMask=pausedProductionMask=0;clearAircraftCandidate();state_.status=0;state_.labels=state_.typeCount=state_.swCount=0;
   state_.threatCount=state_.crateCount=0;house_=0;catalogueReady_=false;catalogueDependencies_.clear();return state_;
  }
  bool fresh=house!=house_||frame<lastFrame_;if(fresh){catalogueDependencies_.clear();++serial_;house_=house;catalogueReady_=false;catalogueHash_=0;for(auto&f:factories_)f={};flashCount=0;productionAt_=now-100;enemies_.count=statuses_.count=0;enemyNames_.clear();}lastFrame_=frame;
  catalogueDependencies_.maintain(read_);
  if(fresh||uint32_t(now-catalogueAt_)>=uint32_t(catalogueReady_?500:100)){
   catalogue::Stamp stamp;catalogueAt_=now;
   if(catalogueDependencies_.capture(read_,stamp)){
    if(!catalogueReady_||catalogueHash_!=stamp.hash){types();sceneFrame_=-1;catalogueHash_=stamp.hash;}
    catalogueReady_=true;
   }else{catalogueReady_=false;int count=state_.typeCount;types();if(count!=state_.typeCount)sceneFrame_=-1;}
  }
  state_.status=1;state_.renderStatus=display_.visible()?1:0;state_.frame=frame;state_.match=sharedMatch_?sharedMatch_:serial_;state_.session=get<int>(0xA8B238);state_.chat=uiValue<uint8_t>(0xA8BC60+25);state_.credits=get<int>(house+0x30C);
  uint32_t tactical=uiValue<uint32_t>(0x887324);Projection proj;proj.width=state_.width=uiValue<int>(0x8A00A4);proj.height=state_.height=uiValue<int>(0x8A00A8);
  if(viewport_.active){if(!nativeviewportprojection::valid(viewport_.logical)||!nativeviewportprojection::valid(viewport_.physical)){state_.renderStatus=0;return state_;}proj.width=viewport_.logical.w;proj.height=viewport_.logical.h;}
  int priorX=state_.cameraX,priorY=state_.cameraY;
  proj.cameraX=state_.cameraX=uiValue<int>(tactical+0xB0);proj.cameraY=state_.cameraY=uiValue<int>(tactical+0xB4);proj.zScale=get<double>(0xB0CD48,0.0866025403784);
  const bool showEnemies=enemyMarkers&&state_.renderStatus==1;Projection enemyProj=proj;
  if(showEnemies){enemyOwners(houses,house);int w=uiValue<int>(0xB0CE30),h=uiValue<int>(0xB0CE34);
   if(w>0&&w<=proj.width)enemyProj.width=w;if(h>0&&h<=proj.height)enemyProj.height=h;
  }enemies_.width=enemyProj.width;enemies_.height=enemyProj.height;
  if(state_.renderStatus!=1)statusOptions_=0;statuses_.width=proj.width;statuses_.height=proj.height;
  int sw=uiValue<int>(0xB0CE30),sh=uiValue<int>(0xB0CE34);if(sw>0&&sw<statuses_.width)statuses_.width=sw;if(sh>0&&sh<statuses_.height)statuses_.height=sh;
  uint32_t selectedHash=2166136261u;for(uint32_t p:vec(0xA8ECB8,4096))selectedHash=(selectedHash^p)*16777619u;
  bool cameraChanged=proj.cameraX!=sceneX_||proj.cameraY!=sceneY_;
  bool scene=fresh||statusOptions_!=sceneStatusOptions_||showEnemies!=sceneEnemies_||specialOnly_!=sceneSpecialOnly_||(specialOnly_&&sourceSerial!=sceneSourceSerial_)||frame!=sceneFrame_||flags!=sceneFlags_||selectedHash!=selectionHash_||proj.width!=sceneWidth_||proj.height!=sceneHeight_||(showEnemies&&(enemyProj.width!=sceneEnemyWidth_||enemyProj.height!=sceneEnemyHeight_||enemyOwnerSignature_!=sceneEnemyOwners_))||(cameraChanged&&uint32_t(now-sceneAt_)>=16);
  if(!scene&&(!specialOnly_||sourceSerial)&&refreshLabels(proj)&&(!statusOptions_||refreshStatuses(proj))&&(!showEnemies||refreshEnemies(enemyProj))){
   int dx=proj.cameraX-priorX,dy=proj.cameraY-priorY;
   for(int i=0;i<state_.crateCount;++i){state_.crates[i].x-=dx;state_.crates[i].y-=dy;}
   for(int i=0;i<state_.threatCount;++i){auto&t=state_.threats[i];t.x-=dx;t.y-=dy;t.tx-=dx;t.ty-=dy;}
   publishEnemies(house,frame,now,showEnemies);publishStatuses(house,frame,now);return state_;
  }
  sceneStatusOptions_=statusOptions_;statuses_.count=0;sceneAt_=now;
  sceneFrame_=frame;sceneFlags_=flags;selectionHash_=selectedHash;sceneX_=proj.cameraX;sceneY_=proj.cameraY;sceneWidth_=proj.width;sceneHeight_=proj.height;
  sceneSourceSerial_=sourceSerial;sceneSpecialOnly_=specialOnly_;sceneEnemies_=showEnemies;sceneEnemyWidth_=enemyProj.width;sceneEnemyHeight_=enemyProj.height;sceneEnemyOwners_=enemyOwnerSignature_;enemies_.count=enemies_.omitted=0;
  state_.labels=state_.selected=state_.transports=state_.used=state_.capacity=state_.threatCount=state_.crateCount=0;
  GameView<Read> view(read_);auto objects=vec(0xA8EC78);uint32_t allies=get<uint32_t>(house+0x5788);
  // A valid process handle and match flag do not prove that the YR 1.001
  // object ABI applies. Report a mismatch rather than silently showing zero
  // units if a populated array has no recognizable TechnoClass vtables.
  if(objects.size()>=4){bool anyKnown=false;
   for(size_t i=0;i<objects.size()&&i<128;++i){if(objects[i]&&kind(objects[i])>=0){anyKnown=true;break;}}
   if(!anyKnown){probe_={layout::Stage::UnrecognizedUnitLayout,0xA8EC78,uint32_t(objects.size())};
    enemies_.count=statuses_.count=0;
    idleProductionMask=pausedProductionMask=0;clearAircraftCandidate();state_.status=0;state_.labels=state_.typeCount=state_.swCount=0;
    state_.threatCount=state_.crateCount=0;catalogueReady_=false;return state_;}
  }
  for(auto p:objects){int k=p?kind(p):-1;if(k<0||get<int>(p+0x6C)<=0||!get<uint8_t>(p+0x74)||get<uint8_t>(p+0x81)||!get<uint8_t>(p+0x90))continue;
   uint32_t owner=get<uint32_t>(p+0x21C),t=type(p,k);bool local=owner==house,selected=local&&uiValue<uint8_t>(p+0x83);if(selected)++state_.selected;
   if(showEnemies)addEnemy(p,t,k,owner,enemyProj);
   if(statusOptions_&&statuses_.count<status::Maximum){status::Marker mark;if(readStatus(p,t,k,owner,proj,mark))statuses_.items[statuses_.count++]=mark;}
   int cap=local&&t&&k!=3&&(flags&assist::Capacity)?get<int>(t+0x5E0):0;
   bool capacity=cap>0&&cap<=1024&&(selected||(flags&assist::AllTransports));int used=0;
   if(capacity){used=passengers(p,cap);if(selected){++state_.transports;state_.used+=used;state_.capacity+=cap;}}
   if((flags&assist::Psychic)&&view.enemy(p,house)&&state_.threatCount<assist::MaxThreats){uint32_t target=get<uint32_t>(p+0x2B4);if(view.live(target)&&get<uint32_t>(target+0x21C)==house){int sx,sy,tx,ty;if(project(proj,get<int>(p+0x9C),get<int>(p+0xA0),get<int>(p+0xA4),sx,sy)&&project(proj,get<int>(target+0x9C),get<int>(target+0xA0),get<int>(target+0xA4),tx,ty))state_.threats[state_.threatCount++]={get<uint32_t>(p+0x10),get<uint32_t>(target+0x10),sx,sy,tx,ty};}}
   int iron=(flags&assist::Iron)?assist::remaining(get<int>(p+0x18C),get<int>(p+0x194),frame):0;
   int freeze=(flags&assist::Frozen)?frozen(p):0;
   uint32_t id=get<uint32_t>(p+0x10);int advanced=local&&get<int>(p+0xAC)==6?2:local&&stopped_.count(id);
   if(!capacity&&!iron&&!freeze&&!advanced)continue;int sx,sy;if(!tactical||!project(proj,get<int>(p+0x9C),get<int>(p+0xA0),get<int>(p+0xA4),sx,sy)||state_.labels>=assist::MaxLabels)continue;
   uint8_t rgb[3]={255,255,255};if(owner>=0x10000)read_(owner+22265,rgb,3);uint32_t color=0xff000000u|(uint32_t(rgb[0])<<16)|(uint32_t(rgb[1])<<8)|rgb[2];
   labelSources_[state_.labels]={p,id,t,owner,k,selected};auto&l=state_.label[state_.labels++];l={id,sx,sy,used,capacity?cap:0,iron,get<int>(p+0x1C4)!=0,advanced,color,freeze};
  }
  state_.swCount=0;auto supers=vec(house+0x254,256);for(size_t i=0;i<supers.size()&&state_.swCount<assist::MaxSW;++i){uint32_t p=supers[i];if(!p||get<uint32_t>(p+0x2C)!=house||!get<uint8_t>(p+0x6D))continue;uint32_t t=get<uint32_t>(p+0x28);if(!t)continue;auto&d=state_.supers[state_.swCount++];d={};read_(t+0x24,d.id,23);name(t,d.name);d.index=int(i);d.frames=assist::remaining(get<int>(p+0x30),get<int>(p+0x38),frame);d.ready=!get<uint8_t>(p+0x70)&&(get<uint8_t>(t+0xE5)?get<int>(p+0x7C)!=0:get<uint8_t>(p+0x6F)!=0);}
  if(!productionAt_||uint32_t(now-productionAt_)>=100){
   productionAt_=now;idleProductionMask=pausedProductionMask=0;state_.factories=state_.busyFactories=state_.pausedFactories=state_.completedFactories=0;
   // YRpp House::Primary_ForBuildings/Defenses/Infantry/Vehicles/Ships/Aircraft.
   const uint32_t offsets[]={0x53BC,0x53CC,0x53B0,0x53B4,0x53B8,0x53AC};
   // Sidebar cameo counts and tab position are local UI state; pausing the
   // simulation must not expire them from the shared world sample.
   idleLines_=pausedLines_=activeTabs_=0;
   for(int i=0;i<6;++i){int tab=i<4?i:3;uint32_t f=get<uint32_t>(house+offsets[i]);auto&previous=factories_[i];
    if(f<0x10000||get<uint32_t>(f+0x6C)!=house){previous={};if(i<4){int n=uiValue<int>(0x87F7E8+0x1544+tab*3988+84,-1);if(n>0&&n<=75)idleLines_|=1u<<i;}continue;}
    uint32_t product=get<uint32_t>(f+0x58);int progress=get<int>(f+0x24),balance=get<int>(f+0x60);
    bool busy=product||get<int>(f+0x50)>0,complete=busy&&progress>=54;
    // +71 records who last changed suspension; Unsuspend leaves it set.
    // +70 is current suspension, +5C is the insufficient-funds hold.
    bool paused=busy&&!complete&&(get<uint8_t>(f+0x70)||get<uint8_t>(f+0x5C));
    bool same=previous.seen&&previous.pointer==f&&previous.product==product;
    bool moving=same&&(progress>previous.progress||balance<previous.balance);
    uint32_t changed=moving?now:same?previous.changed:now;
    if(moving||(!get<uint8_t>(f+0x70)&&uint32_t(now-changed)<600))paused=false;
    previous={f,product,changed,progress,balance,true};
    ++state_.factories;state_.busyFactories+=busy;state_.pausedFactories+=paused;state_.completedFactories+=complete;
    if(busy&&!paused&&!complete)activeTabs_|=1u<<tab;
    if(!busy)idleLines_|=1u<<i;else if(paused)pausedLines_|=1u<<i;
   }
   if(flags&assist::Production)productionReminders(63,idleProductionMask,pausedProductionMask);
   productionTabsY=uiValue<int>(0x87F7E8+0x1544+44,200)-32;if(productionTabsY<100||productionTabsY>state_.height-32)productionTabsY=168;
   captureAircraft(house,frame,now,sourceSerial);
  }
  if((flags&assist::Crates)&&tactical){uint32_t cells=get<uint32_t>(0x87F7E8+316);int capacity=get<int>(0x87F7E8+320);auto overlays=vec(0xA83D80,4096);
   if(cells>=0x10000&&capacity>0&&capacity<=512*512)for(int i=0;i<256&&state_.crateCount<assist::MaxCrates;++i){uint32_t c=0x87F7E8+344+i*16;int x=get<int16_t>(c+12),y=get<int16_t>(c+14);if(x<=0||y<=0||x>=512||y>=512||y*512+x>=capacity||!assist::remaining(get<int>(c),get<int>(c+8),frame))continue;
    uint32_t cell=get<uint32_t>(cells+(y*512+x)*4);int overlay=get<int>(cell+0x44,-1);if(overlay<0||overlay>=int(overlays.size())||!get<uint8_t>(overlays[overlay]+0x2AA))continue;
    int level=get<int8_t>(cell+0x11B),height=get<int>(0x89E7C0,104);if(height<=0||height>1024)height=104;int z=level*height,slope=get<uint8_t>(cell+0x11C);
    if(slope>0&&slope<=19&&(get<uint8_t>(0x89E770)&7)==7){uint32_t a=0x81C900+(slope-1)*40;double scale=get<double>(0x89E740),extra=128*(get<double>(a)+get<double>(a+8))*scale+get<double>(a+16)+get<double>(a+32),maximum=get<double>(a+24);if(std::isfinite(extra)&&maximum>=0&&maximum<=4096)z+=int(std::clamp(extra,0.0,maximum));}
    int sx,sy;if(!project(proj,x*256+128,y*256+128,z,sx,sy))continue;int preview=get<uint8_t>(cell+0x11E);state_.crates[state_.crateCount++]={x,y,sx,sy,preview<19?preview:-1};
   }
  }
  publishEnemies(house,frame,now,showEnemies);publishStatuses(house,frame,now);return state_;
 }
};
}


