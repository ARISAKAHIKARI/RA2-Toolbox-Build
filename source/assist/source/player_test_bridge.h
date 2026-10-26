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
static bool authorizePlayerOrder(const intent::EventOrder&o,OrderAuthority authority){
 if(!lastHouse||o.player<0||o.player>=32)return false;
 if(authority==OrderAuthority::Local)return o.player==get<int>(lastHouse+0x30);
 if(!playertest::Available)return false;
 if(!sh||sh->stop||DWORD(sampleClock(sh)-sh->heartbeat)>=3000)return false;
 if(authority==OrderAuthority::PlayerTest){if(!sh->controlAll||o.domain!=vanilla::Domain::MegaMission||!playertest::manualMission(o.mission))return false;}
 else if(authority==OrderAuthority::SellPlayerTest){if(!playertest::SellAvailable||!(sh->flags&SellEnemy)||o.domain!=vanilla::Domain::Object||o.eventKind!=0x16)return false;}
 else return false;
 uint32_t object=worldByID(uint32_t(o.a));if(!object||!live(object)||o.tagA!=0x34)return false;
 if(authority==OrderAuthority::SellPlayerTest&&!enemyBuildingSellAllowed(object))return false;
 uint32_t house=get<uint32_t>(object+0x21C);
 return playableHouse(house)&&o.player==get<int>(house+0x30);
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
 if(!playerSidebarPending)return;playerSidebarPending=false;
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

