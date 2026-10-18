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
#include "display_state.h"
#include "enemy_markers.h"
#include "idle_aircraft.h"
#include <unordered_map>
namespace external {
struct Projection {int cameraX=0,cameraY=0,width=0,height=0;double zScale=0.0866025403784;};
inline bool project(const Projection&p,int x,int y,int z,int&sx,int&sy){
 if(std::abs(int64_t(x))>1048576||std::abs(int64_t(y))>1048576||std::abs(int64_t(z))>1048576||!std::isfinite(p.zScale)||p.zScale<0||p.zScale>1)return false;
 sx=int((int64_t(x)-y)*30/256)-p.cameraX;sy=int((int64_t(x)+y)*15/256)-int(z*p.zScale+int(z>=728)+.5)-p.cameraY;
 return sx>=-80&&sx<p.width+80&&sy>=-60&&sy<p.height+60;
}
template<class Read> class TacticalSampler {
 Read read_;assist::Snapshot state_{};layout::Result probe_{};uint32_t house_=0,serial_=0;int lastFrame_=-1;
 int sceneFrame_=-1,sceneX_=0,sceneY_=0,sceneWidth_=0,sceneHeight_=0;uint32_t sceneFlags_=0,selectionHash_=0,sceneAt_=0;
 struct LabelSource {uint32_t object=0,id=0,type=0,owner=0;int kind=-1;bool selected=false;};
 LabelSource labelSources_[assist::MaxLabels]{};display::State display_{};
 enemies::Frame enemies_{};bool sceneEnemies_=false;int sceneEnemyWidth_=0,sceneEnemyHeight_=0;uint32_t enemyOwnerSignature_=0,sceneEnemyOwners_=0;
 struct EnemyOwner {uint32_t pointer=0,color=0;int index=-1;};
 EnemyOwner enemyOwners_[32]{};int enemyOwnerCount_=0;
 struct EnemyTypeName {uint16_t text[40]{};};std::unordered_map<uint32_t,EnemyTypeName> enemyNames_;
 uint32_t typePointers_[assist::MaxTypes]{};std::unordered_set<uint32_t> stopped_;
 struct FactoryState {uint32_t pointer=0;int type=-1;bool busy=false,paused=false,complete=false,seen=false;};FactoryState factories_[4];uint32_t productionAt_=0;
 template<class T>T get(uint32_t a,T fallback={}){T v{};return read_(a,&v,sizeof v)?v:fallback;}
 std::vector<uint32_t> vec(uint32_t a,int maximum=16384){
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
   project(proj,get<int>(m.object+0x9C,INT32_MIN),get<int>(m.object+0xA0,INT32_MIN),get<int>(m.object+0xA4,INT32_MIN),x,y);
   if(x==INT32_MIN||y==INT32_MIN)return false; // Offscreen coordinates remain valid.
   m.x=x;m.y=y;m.color=owner->color;m.player=owner->index;
  }return true;
 }
 void addEnemy(uint32_t p,uint32_t t,int k,uint32_t owner,const Projection&proj){
  const auto*enemy=enemyOwner(owner);if(!enemy||t<0x10000||(k!=3&&get<uint32_t>(p+0x11C,0xffffffffu)))return;
  uint32_t id=get<uint32_t>(p+0x10);if(!id)return;int x,y;
  if(!project(proj,get<int>(p+0x9C,INT32_MIN),get<int>(p+0xA0,INT32_MIN),get<int>(p+0xA4,INT32_MIN),x,y))return;
  if(enemies_.count>=enemies::Maximum){++enemies_.omitted;return;}
  auto&mark=enemies_.items[enemies_.count++];mark={};mark.object=p;mark.id=id;mark.type=t;mark.owner=owner;
  mark.color=enemy->color;mark.player=enemy->index;mark.kind=k;mark.x=x;mark.y=y;
  auto found=enemyNames_.find(t);if(found==enemyNames_.end()){
   if(enemyNames_.size()>=1024)enemyNames_.clear();EnemyTypeName value{};name(t,value.text);found=enemyNames_.emplace(t,value).first;
  }std::memcpy(mark.name,found->second.text,sizeof mark.name);
 }
 void publishEnemies(uint32_t house,int frame,uint32_t now,bool enabled){
  enemies_.match=state_.match;enemies_.house=house;enemies_.frame=frame;enemies_.tick=now;
  if(!enabled){enemies_.count=0;return;}
  auto latest=[&](uint32_t at,void*out,size_t n){return enemies::latest(read_,at,out,n,0);};
  int after=-1;uint32_t currentHouse=0,tactical=0;int cameraX=INT32_MIN,cameraY=INT32_MIN;uint8_t started=0;
  bool valid=latest(0xA8ED84,&after,4)&&latest(0xA83D4C,&currentHouse,4)&&latest(0xA8E9A0,&started,1)&&
   latest(0x887324,&tactical,4)&&tactical>=0x10000&&latest(tactical+0xB0,&cameraX,4)&&latest(tactical+0xB4,&cameraY,4);
  if(!valid||!started||currentHouse!=house||after<frame||after-frame>2||cameraX!=state_.cameraX||cameraY!=state_.cameraY||!display::inspect(latest).visible()){
   enemies_.count=0;sceneFrame_=-1;
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
 uint32_t idleProductionMask=0,idleAircraftTick=0;int idleAircraftCount=0,aircraftTotal=0;uint32_t idleAircraftIDs[8192]{};int productionTabsY=168;
 int flashCount=0,flashes[4]{};
 explicit TacticalSampler(Read read):read_(read){}
 layout::Result probe()const{return probe_;}
 display::State displayState()const{return display_;}
 const enemies::Frame& enemyMarkers()const{return enemies_;}
 void reset(){idleProductionMask=idleAircraftTick=0;idleAircraftCount=aircraftTotal=0;probe_={};house_=serial_=0;lastFrame_=-1;state_={};sceneFrame_=-1;sceneFlags_=selectionHash_=sceneAt_=0;flashCount=0;productionAt_=0;for(auto&f:factories_)f={};stopped_.clear();enemies_={};sceneEnemies_=false;enemyNames_.clear();enemyOwnerCount_=0;}
 void stopped(const std::vector<uint32_t>&ids){stopped_.clear();stopped_.insert(ids.begin(),ids.end());}
 void refreshCamera(uint32_t now){
  if(state_.status!=1)return;uint32_t tactical=get<uint32_t>(0x887324);if(tactical<0x10000)return;
  int x=get<int>(tactical+0xB0),y=get<int>(tactical+0xB4),dx=x-state_.cameraX,dy=y-state_.cameraY;
  if(!dx&&!dy)return;state_.cameraX=x;state_.cameraY=y; // Camera-only updates cannot freshen the object sample.
  for(int i=0;i<state_.labels;++i){state_.label[i].x-=dx;state_.label[i].y-=dy;}
  for(int i=0;i<state_.crateCount;++i){state_.crates[i].x-=dx;state_.crates[i].y-=dy;}
  for(int i=0;i<state_.threatCount;++i){auto&t=state_.threats[i];t.x-=dx;t.y-=dy;t.tx-=dx;t.ty-=dy;}
  for(int i=0;i<enemies_.count;++i){enemies_.items[i].x-=dx;enemies_.items[i].y-=dy;}
 }
 const assist::Snapshot&sample(uint32_t now,uint32_t flags,bool enemyMarkers=false){
  state_.sampledTick=now;state_.renderStatus=0;display_=display::inspect(read_);
  probe_=layout::inspect(read_);
  if(probe_.stage!=layout::Stage::Ready){
   enemies_.count=0;
   idleProductionMask=idleAircraftTick=0;idleAircraftCount=aircraftTotal=0;state_.status=0;state_.labels=state_.selected=state_.transports=state_.used=state_.capacity=0;
   state_.typeCount=state_.swCount=state_.threatCount=state_.crateCount=0;
   house_=0;return state_;
  }
  uint32_t house=get<uint32_t>(0xA83D4C);int frame=get<int>(0xA8ED84,-1);
  auto houses=vec(0xA80228,32);if(std::find(houses.begin(),houses.end(),house)==houses.end()){
   enemies_.count=0;
   probe_={layout::Stage::MissingLocalHouseInList,0xA80228,house};
   idleProductionMask=idleAircraftTick=0;idleAircraftCount=aircraftTotal=0;state_.status=0;state_.labels=state_.typeCount=state_.swCount=0;
   state_.threatCount=state_.crateCount=0;house_=0;return state_;
  }
  bool fresh=house!=house_||frame<lastFrame_;if(fresh){++serial_;house_=house;types();for(auto&f:factories_)f={};flashCount=0;productionAt_=now-100;enemies_.count=0;enemyNames_.clear();}lastFrame_=frame;
  state_.status=1;state_.renderStatus=display_.visible()?1:0;state_.frame=frame;state_.match=serial_;state_.session=get<int>(0xA8B238);state_.chat=get<uint8_t>(0xA8BC60+25);state_.credits=get<int>(house+0x30C);
  uint32_t tactical=get<uint32_t>(0x887324);Projection proj;proj.width=state_.width=get<int>(0xA8EB84);proj.height=state_.height=get<int>(0xA8EB88);
  int priorX=state_.cameraX,priorY=state_.cameraY;
  proj.cameraX=state_.cameraX=get<int>(tactical+0xB0);proj.cameraY=state_.cameraY=get<int>(tactical+0xB4);proj.zScale=get<double>(0xB0CD48,0.0866025403784);
  const bool showEnemies=enemyMarkers&&state_.renderStatus==1;Projection enemyProj=proj;
  if(showEnemies){enemyOwners(houses,house);int w=get<int>(0xB0CE30),h=get<int>(0xB0CE34);
   if(w>0&&w<=proj.width)enemyProj.width=w;if(h>0&&h<=proj.height)enemyProj.height=h;
  }enemies_.width=enemyProj.width;enemies_.height=enemyProj.height;
  uint32_t selectedHash=2166136261u;for(uint32_t p:vec(0xA8ECB8,4096))selectedHash=(selectedHash^p)*16777619u;
  bool cameraChanged=proj.cameraX!=sceneX_||proj.cameraY!=sceneY_;
  bool scene=fresh||showEnemies!=sceneEnemies_||frame!=sceneFrame_||flags!=sceneFlags_||selectedHash!=selectionHash_||proj.width!=sceneWidth_||proj.height!=sceneHeight_||(showEnemies&&(enemyProj.width!=sceneEnemyWidth_||enemyProj.height!=sceneEnemyHeight_||enemyOwnerSignature_!=sceneEnemyOwners_))||(cameraChanged&&uint32_t(now-sceneAt_)>=16);
  if(!scene&&refreshLabels(proj)&&(!showEnemies||refreshEnemies(enemyProj))){
   int dx=proj.cameraX-priorX,dy=proj.cameraY-priorY;
   for(int i=0;i<state_.crateCount;++i){state_.crates[i].x-=dx;state_.crates[i].y-=dy;}
   for(int i=0;i<state_.threatCount;++i){auto&t=state_.threats[i];t.x-=dx;t.y-=dy;t.tx-=dx;t.ty-=dy;}
   publishEnemies(house,frame,now,showEnemies);return state_;
  }
  sceneAt_=now;
  sceneFrame_=frame;sceneFlags_=flags;selectionHash_=selectedHash;sceneX_=proj.cameraX;sceneY_=proj.cameraY;sceneWidth_=proj.width;sceneHeight_=proj.height;
  sceneEnemies_=showEnemies;sceneEnemyWidth_=enemyProj.width;sceneEnemyHeight_=enemyProj.height;sceneEnemyOwners_=enemyOwnerSignature_;enemies_.count=enemies_.omitted=0;
  state_.labels=state_.selected=state_.transports=state_.used=state_.capacity=state_.threatCount=state_.crateCount=0;
  GameView<Read> view(read_);auto objects=vec(0xA8EC78);uint32_t allies=get<uint32_t>(house+0x5788);
  // A valid process handle and match flag do not prove that the YR 1.001
  // object ABI applies. Report a mismatch rather than silently showing zero
  // units if a populated array has no recognizable TechnoClass vtables.
  if(objects.size()>=4){bool anyKnown=false;
   for(size_t i=0;i<objects.size()&&i<128;++i){if(objects[i]&&kind(objects[i])>=0){anyKnown=true;break;}}
   if(!anyKnown){probe_={layout::Stage::UnrecognizedUnitLayout,0xA8EC78,uint32_t(objects.size())};
    enemies_.count=0;
    idleProductionMask=idleAircraftTick=0;idleAircraftCount=aircraftTotal=0;state_.status=0;state_.labels=state_.typeCount=state_.swCount=0;
    state_.threatCount=state_.crateCount=0;return state_;}
  }
  for(auto p:objects){int k=p?kind(p):-1;if(k<0||get<int>(p+0x6C)<=0||!get<uint8_t>(p+0x74)||get<uint8_t>(p+0x81)||!get<uint8_t>(p+0x90))continue;
   uint32_t owner=get<uint32_t>(p+0x21C),t=type(p,k);bool local=owner==house,selected=local&&get<uint8_t>(p+0x83);if(selected)++state_.selected;
   if(showEnemies)addEnemy(p,t,k,owner,enemyProj);
   int cap=local&&t&&k!=3&&(flags&assist::Capacity)?get<int>(t+0x5E0):0;
   bool capacity=cap>0&&cap<=1024&&(selected||(flags&assist::AllTransports));int used=0;
   if(capacity){used=passengers(p,cap);if(selected){++state_.transports;state_.used+=used;state_.capacity+=cap;}}
   if((flags&assist::Psychic)&&view.enemy(p,house)&&state_.threatCount<assist::MaxThreats){uint32_t target=get<uint32_t>(p+0x2B4);if(view.live(target)&&get<uint32_t>(target+0x21C)==house){int sx,sy,tx,ty;if(project(proj,get<int>(p+0x9C),get<int>(p+0xA0),get<int>(p+0xA4),sx,sy)&&project(proj,get<int>(target+0x9C),get<int>(target+0xA0),get<int>(target+0xA4),tx,ty))state_.threats[state_.threatCount++]={get<uint32_t>(p+0x10),get<uint32_t>(target+0x10),sx,sy,tx,ty};}}
   int iron=(flags&assist::Iron)?assist::remaining(get<int>(p+0x18C),get<int>(p+0x194),frame):0;
   int freeze=(flags&assist::Frozen)?frozen(p):0;
   uint32_t id=get<uint32_t>(p+0x10);bool advanced=local&&stopped_.count(id);
   if(!capacity&&!iron&&!freeze&&!advanced)continue;int sx,sy;if(!tactical||!project(proj,get<int>(p+0x9C),get<int>(p+0xA0),get<int>(p+0xA4),sx,sy)||state_.labels>=assist::MaxLabels)continue;
   uint8_t rgb[3]={255,255,255};if(owner>=0x10000)read_(owner+22265,rgb,3);uint32_t color=0xff000000u|(uint32_t(rgb[0])<<16)|(uint32_t(rgb[1])<<8)|rgb[2];
   labelSources_[state_.labels]={p,id,t,owner,k,selected};auto&l=state_.label[state_.labels++];l={id,sx,sy,used,capacity?cap:0,iron,get<int>(p+0x1C4)!=0,advanced,color,freeze};
  }
  state_.swCount=0;auto supers=vec(house+0x254,256);for(size_t i=0;i<supers.size()&&state_.swCount<assist::MaxSW;++i){uint32_t p=supers[i];if(!p||get<uint32_t>(p+0x2C)!=house||!get<uint8_t>(p+0x6D))continue;uint32_t t=get<uint32_t>(p+0x28);if(!t)continue;auto&d=state_.supers[state_.swCount++];d={};read_(t+0x24,d.id,23);name(t,d.name);d.index=int(i);d.frames=assist::remaining(get<int>(p+0x30),get<int>(p+0x38),frame);d.ready=!get<uint8_t>(p+0x70)&&(get<uint8_t>(t+0xE5)?get<int>(p+0x7C)!=0:get<uint8_t>(p+0x6F)!=0);}
  if(!productionAt_||uint32_t(now-productionAt_)>=100){
   productionAt_=now;idleProductionMask=0;state_.factories=state_.busyFactories=state_.pausedFactories=state_.completedFactories=0;
   // YRpp House::Primary_ForBuildings/Defenses/Infantry/Vehicles/Ships/Aircraft.
   const uint32_t offsets[]={0x53BC,0x53CC,0x53B0,0x53B4,0x53B8,0x53AC};
   bool active[4]{},exists[4]{};for(int tab=0;tab<4;++tab){int n=get<int>(0x87F7E8+0x1544+tab*3988+84,-1);exists[tab]=n>0&&n<=75;}
   for(int i=0;i<6;++i){int tab=i<4?i:3;uint32_t f=get<uint32_t>(house+offsets[i]);if(f<0x10000||get<uint32_t>(f+0x6C)!=house)continue;
    ++state_.factories;bool busy=get<uint32_t>(f+0x58)||get<int>(f+0x50)>0;bool paused=busy&&(get<uint8_t>(f+0x5C)||get<uint8_t>(f+0x71)),complete=busy&&get<int>(f+0x24)>=54;
    state_.busyFactories+=busy;state_.pausedFactories+=paused;state_.completedFactories+=complete;exists[tab]=true;active[tab]|=busy&&!paused&&!complete;
   }
   if(flags&assist::Production)for(int tab=0;tab<4;++tab)if((tab==0||tab==3)&&exists[tab]&&!active[tab])idleProductionMask|=1u<<tab;
   productionTabsY=get<int>(0x87F7E8+0x1544+44,200)-32;if(productionTabsY<100||productionTabsY>state_.height-32)productionTabsY=168;
   idleAircraftCount=aircraftTotal=0;aircraft::Inspect<Read> planes{read_};
   for(uint32_t p:vec(0xA8E390,8192))if(planes.valid(p,house)){++aircraftTotal;if(idleAircraftCount<8192&&planes.idle(p,house))idleAircraftIDs[idleAircraftCount++]=get<uint32_t>(p+0x10);}
   std::sort(idleAircraftIDs,idleAircraftIDs+idleAircraftCount);idleAircraftTick=now;
  }
  if((flags&assist::Crates)&&tactical){uint32_t cells=get<uint32_t>(0x87F7E8+316);int capacity=get<int>(0x87F7E8+320);auto overlays=vec(0xA83D80,4096);
   if(cells>=0x10000&&capacity>0&&capacity<=512*512)for(int i=0;i<256&&state_.crateCount<assist::MaxCrates;++i){uint32_t c=0x87F7E8+344+i*16;int x=get<int16_t>(c+12),y=get<int16_t>(c+14);if(x<=0||y<=0||x>=512||y>=512||y*512+x>=capacity||!assist::remaining(get<int>(c),get<int>(c+8),frame))continue;
    uint32_t cell=get<uint32_t>(cells+(y*512+x)*4);int overlay=get<int>(cell+0x44,-1);if(overlay<0||overlay>=int(overlays.size())||!get<uint8_t>(overlays[overlay]+0x2AA))continue;
    int level=get<int8_t>(cell+0x11B),height=get<int>(0x89E7C0,104);if(height<=0||height>1024)height=104;int z=level*height,slope=get<uint8_t>(cell+0x11C);
    if(slope>0&&slope<=19&&(get<uint8_t>(0x89E770)&7)==7){uint32_t a=0x81C900+(slope-1)*40;double scale=get<double>(0x89E740),extra=128*(get<double>(a)+get<double>(a+8))*scale+get<double>(a+16)+get<double>(a+32),maximum=get<double>(a+24);if(std::isfinite(extra)&&maximum>=0&&maximum<=4096)z+=int(std::clamp(extra,0.0,maximum));}
    int sx,sy;if(!project(proj,x*256+128,y*256+128,z,sx,sy))continue;int preview=get<uint8_t>(cell+0x11E);state_.crates[state_.crateCount++]={x,y,sx,sy,preview<19?preview:-1};
   }
  }
  publishEnemies(house,frame,now,showEnemies);return state_;
 }
};
}

