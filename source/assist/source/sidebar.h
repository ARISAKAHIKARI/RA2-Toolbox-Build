// Native sidebar helpers. Full-tech unlock is currently disabled: refreshTech
// reports -20 and does not patch eligibility, prerequisites or cancellation.
// The helpers below are also used by ordinary production and cameo feedback.
static constexpr uint32_t Sidebar=0x87F7E8, Tabs=Sidebar+0x1544;
#include "sidebar_policy.h"
static bool techPending;
static int typeSlot(uint32_t type){for(int i=0;i<snap.typeCount;++i)if(typePointers[i]==type)return i;return -1;}
static int typeTab(int i){auto&t=snap.types[i];return callFast<int>(0x6ABC60,t.kind,t.index,0);}
static void sidebarRedraw(int tab){if(tab<0||tab>3)return;put<bool>(Tabs+tab*3988+60,true);put<bool>(Sidebar+0x53A6,true);put<bool>(Sidebar+0x53A7,true);}
static int canonicalKind(int k){return sidebarpolicy::canonicalKind(k);}
static bool hasCameo(int kind,int index){for(int tab=0;tab<4;++tab){uint32_t strip=Tabs+tab*3988;int count=get<int>(strip+84);if(count<0||count>75)continue;
 for(int j=0;j<count;++j){uint32_t c=strip+88+j*52;if(get<int>(c)==index&&canonicalKind(get<int>(c+4))==kind)return true;}}return false;
}
static bool techSessionAllowed(){return decisions::canProduceInSession(eventdelivery::isMultiplayer(get<int>(0xA8B238)),sh&&(sh->flags&UnsafeTech));}
static uint32_t techFactory(uint32_t type,uint32_t house){
 // YRpp ObjectTypeClass::FindFactory(allowOccupied,requirePower,requireCanBuild,house).
 // A busy or unpowered factory still owns a cameo; the native click/production
 // code handles its operational state. Missing factories must remove cameos.
 uint32_t table=0,method=0;if(!read(type,&table,4)||!read(table+0x94,&method,4)||method<0x10000)return 0;
 return call<uint32_t>(method,type,true,false,false,(void*)(uintptr_t)house);
}
static void restoreTech(){techPending=false;snap.techOpen=0;}
static void refreshTech(uint32_t,DWORD now){techRefresh=now;restoreTech();snap.techError=-20;snap.techVisible=snap.techUnavailable=0;}
struct FlashRef {uint32_t address;int kind,index,until;};static FlashRef flashRefs[4*75];
static void clearFlashes(){for(auto&f:flashRefs){if(f.address&&get<int>(f.address-48)==f.index&&get<int>(f.address-44)==f.kind&&get<int>(f.address)==f.until)put<int>(f.address,0);f={};}}
static void flashCameo(int type,int frame){if(type<0||type>=snap.typeCount)return;int tab=typeTab(type);if(tab!=0&&tab!=3)return;
 uint32_t strip=Tabs+tab*3988;int count=get<int>(strip+84);if(count<0||count>75)return;auto&t=snap.types[type];
 for(int j=0;j<count;++j){uint32_t c=strip+88+j*52;if(get<int>(c)==t.index&&canonicalKind(get<int>(c+4))==t.kind){int until=frame+90;put<int>(c+48,until);flashRefs[tab*75+j]={c+48,get<int>(c+4),t.index,until};sidebarRedraw(tab);return;}}
}
struct ProductionState {uint32_t factory;int type;bool busy,paused,complete,seen;};static ProductionState prodState[4];
static void scanProduction(uint32_t house,int frame,uint32_t flags){
 snap.factories=snap.busyFactories=snap.pausedFactories=snap.completedFactories=0;snap.notice=0;
 // Building tab and vehicle tab. Defenses and infantry are intentionally absent.
 const int offsets[4]={0x53BC,0x53B4,0x53B8,0x53AC};
 if(!(flags&Production))clearFlashes();
 for(int i=0;i<4;++i){auto&prev=prodState[i];uint32_t f=get<uint32_t>(house+offsets[i]);
  if(f<0x10000||get<uint32_t>(f+0x6C)!=house){prev={};continue;}
  ++snap.factories;uint32_t p=get<uint32_t>(f+0x58);int k=p>=0x10000?kind(p):-1;int type=k>=0?typeSlot(typeOf(p,k)):-1;
  bool occupied=p||get<int>(f+0x50)>0,paused=occupied&&(get<bool>(f+0x5C)||get<bool>(f+0x71)),complete=occupied&&get<int>(f+0x24)>=54;
  snap.busyFactories+=occupied;snap.pausedFactories+=paused;snap.completedFactories+=complete;
  bool event=prev.seen&&prev.factory==f&&((prev.busy&&!occupied&&!prev.complete)||(!prev.paused&&paused)||(!prev.complete&&complete));
  if((flags&Production)&&event){int changedType=type>=0?type:prev.type;int changedTab=changedType>=0?typeTab(changedType):-1;
   if(changedTab==0||changedTab==3){flashCameo(changedType,frame);++snap.noticeSerial;}
  }
  if(type<0)type=prev.factory==f?prev.type:-1;prev={f,type,occupied,paused,complete,(flags&Production)!=0};
 }
}
static void resetProduction(){memset(prodState,0,sizeof prodState);}
