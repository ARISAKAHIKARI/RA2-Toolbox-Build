#include "../../shared/source/debug_entry_checks.h"
// Player tests are explicit manual capabilities. Ordinary planners remain local.
#include "player_test_policy.h"
struct PlayerIdentity {uint32_t p,id,type;int index;uint8_t current;};
struct PlayerContext {uint32_t original,applied,items,scenario;int count,frame;PlayerIdentity houses[32];};
static PlayerContext playerContext;
static bool playerSidebarPending;
static bool playableHouse(uint32_t house){
 Vec houses=vec(0xA80228,32);bool found=false;for(int n=0;n<houses.count;++n)if(item(houses,n)==house)found=true;
 if(!found)return false;
 uint32_t type=0;int index=-1;uint8_t passive=1,ended[4]{};char id[24]{};
 if(!read(house+0x34,&type,4)||type<0x10000||!read(house+0x30,&index,4)||!read(type+0x24,id,sizeof id)||!read(type+0x1A6,&passive,1)||!read(house+0x1F5,ended,sizeof ended))return false;
 bool spectator=house==get<uint32_t>(0xAC1198);Vec nodes=vec(0xA8DA74,32);
 for(int n=0;n<nodes.count;++n){uint32_t node=item(nodes,n);if(node>=0x10000&&get<int>(node+0x6B)==-1&&get<int>(node+0x6F)==index)spectator=true;}
 return playertest::eligible(index,passive,spectator,ended[0]||ended[1]||ended[3],id);
}
static bool enemyBuildingSellAllowed(uint32_t object){
 // This changes only which manual Sell requests the toolbox may construct.
 // It does not change ownership, CurrentPlayer or any receiver-side check.
 return playertest::SellAvailable&&sh&&(sh->flags&SellEnemy)&&lastHouse&&
  object&&worldObjects.contains(object)&&kind(object)==3&&nativeEnemy(object,lastHouse)&&
  playableHouse(get<uint32_t>(object+0x21C));
}
static int destructPlayerTarget(uint32_t house,uint32_t expectedID,uint32_t expectedIndex){
 if(!playertest::DestructAvailable||!lastHouse||!playableHouse(lastHouse)||
    !house||!playableHouse(house)||get<bool>(house+0x1F7))return -4;
 Vec houses=vec(0xA80228,32);
 if(expectedIndex>=uint32_t(houses.count)||item(houses,int(expectedIndex))!=house||
    get<uint32_t>(house+0x10)!=expectedID||get<int>(house+0x30)!=int(expectedIndex))return -7;
 // The unadapted native sender replaces House with CurrentHouse. Foreign
 // Destruct is admitted only while the complete managed preservation group is ready.
 if(eventdelivery::isMultiplayer(get<int>(0xA8B238))&&house!=lastHouse){
  if(!networkHousePreserveReady())return -42;
  if(!networkSenderHouse(int(expectedIndex)))return -4;
 }
 return 1;
}
struct DestructSelectedItem{uint32_t object=0,id=0,table=0,owner=0;};
struct DestructSelectedSnapshot{uint32_t items=0,scenario=0,house=0,houseID=0;int count=0,index=-1,frame=-1;DestructSelectedItem objects[4096]{};};
static DestructSelectedSnapshot destructSelected;static bool destructSelectedArmed=false;
static bool captureDestructSelection(DestructSelectedSnapshot&out){
 out={};Vec v{};if(!read(0xA8ECB8,&v,sizeof v)||v.count<=0||v.count>4096||v.capacity<v.count||v.items<0x10000||
 !read(0xA8B230,&out.scenario,4)||out.scenario<0x10000||!read(0xA8ED84,&out.frame,4)||out.frame<0)return false;
 out.items=v.items;out.count=v.count;
 for(int i=0;i<v.count;++i){auto&o=out.objects[i];uint8_t selected=0,alive=0,onMap=0,limbo=0;int hp=0;
  if(!read(v.items+uint32_t(i)*4,&o.object,4)||o.object<0x10000||!read(o.object,&o.table,4)||
   (o.table!=0x7F5C70&&o.table!=0x7EB058&&o.table!=0x7E3EBC&&o.table!=0x7E22A4)||
   !read(o.object+0x10,&o.id,4)||!o.id||!read(o.object+0x21C,&o.owner,4)||o.owner<0x10000||
   !read(o.object+0x83,&selected,1)||!selected||!read(o.object+0x90,&alive,1)||!alive||
   !read(o.object+0x74,&onMap,1)||!onMap||!read(o.object+0x81,&limbo,1)||limbo||
   !read(o.object+0x6C,&hp,4)||hp<=0)return false;
  if(!out.house)out.house=o.owner;else if(out.house!=o.owner)return false;
 }
 Vec after{};uint32_t scenario=0;int frame=-1;
 return read(0xA8ECB8,&after,sizeof after)&&after.items==v.items&&after.count==v.count&&after.capacity==v.capacity&&
  read(0xA8B230,&scenario,4)&&scenario==out.scenario&&read(0xA8ED84,&frame,4)&&frame==out.frame&&
  read(out.house+0x10,&out.houseID,4)&&out.houseID&&read(out.house+0x30,&out.index,4)&&out.index>=0&&out.index<32;
}
static bool sameDestructSelection(const DestructSelectedSnapshot&a,const DestructSelectedSnapshot&b){
 if(a.items!=b.items||a.scenario!=b.scenario||a.frame!=b.frame||a.count!=b.count||a.house!=b.house||a.houseID!=b.houseID||a.index!=b.index)return false;
 for(int i=0;i<a.count;++i){const auto&x=a.objects[i];const auto&y=b.objects[i];if(x.object!=y.object||x.id!=y.id||x.table!=y.table||x.owner!=y.owner)return false;}return true;
}
static bool authorizePlayerOrder(const intent::EventOrder&o,OrderAuthority authority){
 if(!lastHouse||o.player<0||o.player>=32)return false;
 if(o.domain==vanilla::Domain::NoOptions&&o.eventKind==vanilla::EventType::Destruct){
  // An explicit, current one-shot request is the only Destruct authorization,
  // even for the local House. Other automatic/local senders cannot opt in.
  if(authority!=OrderAuthority::DestructPlayerTest||!sh||sh->stop||sh->command!=DestructPlayer||
     sh->request==sh->ack||sh->requestMatch!=match||DWORD(sampleClock(sh)-sh->requestTick)>3500||
     DWORD(sampleClock(sh)-sh->heartbeat)>=3000)return false;
  int valid=-7;
  if(sh->arg==0){
   static DestructSelectedSnapshot current;
   if(sh->arg2||sh->requestToken||!destructSelectedArmed||!captureDestructSelection(current)||
      !sameDestructSelection(destructSelected,current)||current.index!=o.player){snap.lastSubmitError=7;return false;}
   valid=destructPlayerTarget(current.house,current.houseID,uint32_t(current.index));
  }else{if(sh->requestToken!=uint32_t(o.player))return false;valid=destructPlayerTarget(uint32_t(sh->arg),uint32_t(sh->arg2),sh->requestToken);}
  if(valid==-42)snap.lastSubmitError=42;
  return valid>0;
 }
 if(authority==OrderAuthority::DestructPlayerTest)return false;
 if(authority==OrderAuthority::Local)return o.player==get<int>(lastHouse+0x30);
 if(!playertest::Available)return false;
 if(!sh||sh->stop||DWORD(sampleClock(sh)-sh->heartbeat)>=3000)return false;
 if(authority==OrderAuthority::PlayerTest){if(!controlmode::proxyOrders(sh->controlAll)||o.domain!=vanilla::Domain::MegaMission||!playertest::manualMission(o.mission))return false;}
 else if(authority==OrderAuthority::SellPlayerTest){if(!playertest::SellAvailable||!(sh->flags&SellEnemy)||o.domain!=vanilla::Domain::Object||o.eventKind!=0x16)return false;}
 else return false;
 uint32_t object=worldByID(uint32_t(o.a));if(!object||!live(object)||o.tagA!=0x34)return false;
 if(authority==OrderAuthority::SellPlayerTest&&!enemyBuildingSellAllowed(object))return false;
 uint32_t house=get<uint32_t>(object+0x21C);
 if(!playableHouse(house)||o.player!=get<int>(house+0x30))return false;
 if(authority==OrderAuthority::PlayerTest&&sh->controlCommandOwner&&house!=lastHouse&&!controlCommandOwnerReady(house)){
  snap.lastSubmitError=42;++sh->controlCommandOwnerRejected;return false;
 }
 return true;
}
static int destructPlayerCommand(uint32_t house,uint32_t expectedID,uint32_t expectedIndex){
 snap.lastSubmitError=0;
 if(!gameentry::active(gameentry::Client::Assist)||!simulationReady())return -3;
 destructSelectedArmed=false;
 if(!house){
  if(expectedID||expectedIndex||!captureDestructSelection(destructSelected))return -7;
  house=destructSelected.house;expectedID=destructSelected.houseID;expectedIndex=uint32_t(destructSelected.index);
  destructSelectedArmed=true;
 }
 int valid=destructPlayerTarget(house,expectedID,expectedIndex);
 if(valid<0){destructSelectedArmed=false;if(valid==-42)snap.lastSubmitError=42;return valid;}
 static const uint8_t ctor[]={0x53,0x8B,0x5C,0x24,0x0C,0x8B,0xC3,0x56};
 if(!debugentries::pattern(debugentries::Kind::Call,0x4C66C0,ctor,sizeof ctor,read)){destructSelectedArmed=false;return -12;}
 intent::EventOrder event{};event.domain=vanilla::Domain::NoOptions;
 event.eventKind=vanilla::EventType::Destruct;event.player=int(expectedIndex);
 // Human Houses retain native queued delivery. Singleplayer AI Houses have
 // no native DoList dispatch, so the gateway responds to this one packet.
 bool responded=false;
 const bool accepted=submitConstructed(event,OrderAuthority::DestructPlayerTest,nullptr,&responded);destructSelectedArmed=false;
 return accepted?(responded?2:1):snap.lastSubmitError==42?-42:snap.lastSubmitError==7?-7:snap.lastSubmitError==12?-12:-6;
}
static bool samePlayerWorld(){
 // Readiness can drop temporarily in menus; it is not a world identity.
 auto&c=playerContext;if(!c.original||get<uint32_t>(0xA8B230)!=c.scenario||get<int>(0xA8ED84)<c.frame)return false;
 Vec houses=vec(0xA80228,32);if(houses.items!=c.items||houses.count!=c.count)return false;
 for(int i=0;i<c.count;++i){auto&h=c.houses[i];uint32_t id=0,type=0;int index=-1;
  if(item(houses,i)!=h.p||!read(h.p+0x10,&id,4)||!read(h.p+0x34,&type,4)||!read(h.p+0x30,&index,4)||id!=h.id||type!=h.type||index!=h.index)return false;
 }return get<uint32_t>(0xA83D4C)==c.applied;
}
static void clearPlayerSidebar(){
 // Local sidebar only. Never abandon, refund or mutate a production factory.
 clearFlashes();for(int tab=0;tab<4;++tab){uint32_t strip=Tabs+tab*3988;put<int>(strip+84,0);put<int>(strip+68,0);sidebarRedraw(tab);}
 put<uint32_t>(0x88098C,0);put<uint32_t>(0x880990,0);put<int>(0x880994,-1);
 // Clear the armed superweapon cursor as well as a held construction cameo.
 put<int>(0x8809A0,-1);put<uint8_t>(0x88099A,0);put<uint8_t>(0x88099B,0);put<uint8_t>(0x88099C,0);
}
static void restorePlayerContext(){
 if(!playerContext.original)return;
 if(samePlayerWorld()){
  // Restoring our verified House fields must remain possible if an optional
  // selection API changes. unselect() checks its own callable dependency.
  const bool active=get<bool>(0xA8E9A0);if(active)unselect();
  for(int i=0;i<playerContext.count;++i)put<uint8_t>(playerContext.houses[i].p+0x1EC,playerContext.houses[i].current);
  put<uint32_t>(0xA83D4C,playerContext.original);playerSidebarPending=true;playerContextChanged=true;
  if(active)clearPlayerSidebar();
 }
 playerContext={};
}
static void playerMaintenance(){
 if(playerContext.original){if(!samePlayerWorld()){playerContext={};playerSidebarPending=false;}else{playerContext.frame=get<int>(0xA8ED84);if(!playertest::SwitchAvailable||!sh||sh->stop||!sh->switchPlayerEnabled)restorePlayerContext();}}
}
static int switchPlayerContext(uint32_t target,uint32_t expectedID=0){
 if(!playertest::SwitchAvailable)return -4;
 if(!target){if(!playerContext.original)return -4;restorePlayerContext();return playerContextChanged?1:-7;}
 if(!sh||!sh->switchPlayerEnabled||!playableHouse(target)||get<uint32_t>(target+0x10)!=expectedID)return -4;
 if(target==get<uint32_t>(0xA83D4C))return 1;
 // Switching is one transaction: do not clear selection/sidebar or create
 // restoration ownership until all UI calls needed for the new player work.
 if(!nativeDependenciesReady(0x48DC90,0x6ABC60,0x5F7900,0x4F7870,0x6A6300))return -12;
 if(!playerContext.original){Vec houses=vec(0xA80228,32);if(!houses.count||!playableHouse(lastHouse))return -4;
  auto&c=playerContext;c.original=lastHouse;c.applied=lastHouse;c.items=houses.items;c.count=houses.count;c.scenario=get<uint32_t>(0xA8B230);c.frame=get<int>(0xA8ED84);
  for(int i=0;i<c.count;++i){uint32_t p=item(houses,i);c.houses[i]={p,get<uint32_t>(p+0x10),get<uint32_t>(p+0x34),get<int>(p+0x30),get<uint8_t>(p+0x1EC)};}
 }
 if(!samePlayerWorld())return -7;
 unselect();reset();clearPlayerSidebar();
 put<uint8_t>(playerContext.applied+0x1EC,0);put<uint8_t>(target+0x1EC,1);put<uint32_t>(0xA83D4C,target);
 playerContext.applied=target;playerContext.frame=get<int>(0xA8ED84);playerSidebarPending=true;playerContextChanged=true;
 return 1;
}
static void rebuildPlayerSidebar(uint32_t house){
 if(!playerSidebarPending)return;
 if(!nativeDependenciesReady(0x6ABC60,0x5F7900,0x4F7870,0x6A6300))return;
 playerSidebarPending=false;
 for(int i=0;i<snap.typeCount;++i){auto&t=snap.types[i];uint32_t type=typePointers[i];
  if(!type||!techFactory(type,house))continue;
  // Include the actual queue's current object even when its prerequisites vanished.
  int tab=typeTab(i);if(tab<0||tab>3)continue;
  uint32_t f=get<uint32_t>(house+(t.kind==3?0x53AC:t.kind==16?0x53B0:t.kind==40?(t.naval?0x53B8:0x53B4):(tab==1?0x53CC:0x53BC)));
  uint32_t p=f>=0x10000?get<uint32_t>(f+0x58):0;
  bool producing=p>=0x10000&&typeOf(p,kind(p))==type;
  if(producing||call<int>(0x4F7870,house,(void*)(uintptr_t)type,false,true)>0){call<bool>(0x6A6300,Sidebar,t.kind,t.index);sidebarRedraw(tab);}
 }
 put<uint8_t>(house+0x1FC,1);
}
static void publishPlayers(){
 if(!playertest::Available){sh->playerCount=0;sh->playerCurrent=sh->playerOriginal=-1;sh->playerControlStatus=0;return;}
 static DWORD at=0;static uint32_t epoch=0,local=0;DWORD now=sampleClock(sh);
 if(snap.status==1&&epoch==snap.match&&local==lastHouse&&DWORD(now-at)<250)return;
 at=now;epoch=snap.match;local=lastHouse;
 sh->playerCount=0;sh->playerCurrent=lastHouse?get<int>(lastHouse+0x30):-1;sh->playerOriginal=playerContext.original?get<int>(playerContext.original+0x30):-1;
 if(snap.status!=1||!get<bool>(0xA8E9A0))return;
 Vec houses=vec(0xA80228,32);for(int i=0;i<houses.count&&sh->playerCount<32;++i){uint32_t p=item(houses,i);if(!playableHouse(p))continue;
  auto&info=sh->players[sh->playerCount++];info={};info.pointer=p;info.id=get<uint32_t>(p+0x10);info.index=get<int>(p+0x30);info.human=get<bool>(p+0x1ED);read(p+90154,info.name,40);info.name[20]=0;
 }
}

