#include "placement_event_input.h"
static placementeventhost::Selection placementSelection;
static void resetPlacementEvent(){placementSelection.clear();if(ui){ControlUpdate guard;SendDlgItemMessageW(ui,9251,CB_RESETCONTENT,0,0);}}
static void refreshPlacementOffers(){
 placementSelection.update(pid,commandMatch(),incoming.placementOffers,incoming.placementOfferCount,
  GetTickCount(),incoming.frame,typeListHash(incoming.types,incoming.typeCount),
  enabled&&buildAnywhereMode&&extra.placementImplementation==buildingmodes::PlacementEvent&&shared&&!shared->stop&&lastData&&DWORD(GetTickCount()-lastData)<1500&&
  incoming.status==1&&(incoming.commandStatus==1||incoming.commandStatus==9||incoming.commandStatus==10)&&incoming.placementStatus==2);
}
static void updatePlacementEventUI(){
 if(!ready||!ui)return;
 SetDlgItemTextW(ui,476,buildingmodes::placementCaption(extra.placementImplementation));
 SetDlgItemTextW(ui,420,buildingmodes::technologyCaption(techImplementation));
 HWND list=GetDlgItem(ui,9251);if(!list||SendMessageW(list,CB_GETDROPPEDSTATE,0,0))return;
 const auto before=placementSelection;refreshPlacementOffers();
 bool changed=before.count!=placementSelection.count||before.pid!=placementSelection.pid||before.match!=placementSelection.match||before.chosen!=placementSelection.chosen;
 for(int n=0;!changed&&n<placementSelection.count;++n)changed=!nativeplacement::sameIdentity(before.offers[n],placementSelection.offers[n]);
 if(!changed&&SendMessageW(list,CB_GETCOUNT,0,0)>0)return;
 ControlUpdate guard;SendMessageW(list,CB_RESETCONTENT,0,0);int chosen=-1;
 for(int n=0;n<placementSelection.count;++n){const auto&o=placementSelection.offers[n];std::wstring label=o.tab?L"防御：":L"建筑：";
  if(o.slot>=0&&o.slot<incoming.typeCount&&incoming.types[o.slot].name[0])label+=reinterpret_cast<const wchar_t*>(incoming.types[o.slot].name);
  else label+=L"名称暂不可用";
  SendMessageW(list,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(label.c_str()));
  if(placementSelection.chosen&&nativeplacement::sameIdentity(placementSelection.selected,o))chosen=n;
 }
 if(!placementSelection.count){SendMessageW(list,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"等待已完成建筑"));chosen=0;}
 SendMessageW(list,CB_SETCURSEL,chosen,0);
}
static void selectPlacementEvent(){
 // The displayed list owns these identities. Do not refresh/reindex it before
 // interpreting a CBN_SELCHANGE emitted by that exact visible list.
 const int index=int(SendDlgItemMessageW(ui,9251,CB_GETCURSEL,0,0));
 placementSelection.choose(index);
}
static bool placementEventReady(DWORD now){
 return enabled&&buildAnywhereMode&&extra.placementImplementation==buildingmodes::PlacementEvent&&
  game&&pid&&IsWindow(game)&&foregroundGame()&&shared&&commandFresh(now)&&!incoming.chat&&
  incoming.placementStatus==2&&snap.renderStatus==1&&!formationInputOwns()&&!shared->viewZoomBusy&&
  shared->request==shared->ack&&shared->ack==ack&&!commandQueue.size&&!hasSelection&&
  shared->selectRequest==shared->selectAck&&shared->pointer.request==shared->pointer.ack;
}
static void submitPlacementEvent(){
 const DWORD capturedAt=GetTickCount();DWORD now=capturedAt;if(!placementEventReady(now)){notify(L"放置未提交：请等待游戏前台和命令连接就绪，再重新按键。");return;}
 readSnapshot();refreshPlacementOffers();const auto*offer=placementSelection.current();
 if(!placementEventReady(GetTickCount())||!offer){notify(L"放置未提交：请先在已完成建筑清单选择仍有效的产品。");return;}
 nativeplacement::Request request{};request.offer=*offer;
 auto read=[](uint32_t at,void*out,size_t n){return externalReader.readFresh(at,out,n);};
 const auto visible=external::display::inspect(read);POINT point{};RECT client{};uint32_t tactical=0,house=0;int frame=0;
 if(!visible.visible()||!GetCursorPos(&point)||!ScreenToClient(game,&point)||!GetClientRect(game,&client)||client.right<=0||client.bottom<=0||point.x<0||point.y<0||point.x>=client.right||point.y>=client.bottom||
    !read(0x887324,&tactical,4)||tactical<0x10000||!read(tactical+0xB0,&request.cameraX,4)||!read(tactical+0xB4,&request.cameraY,4)||
    !read(0x8A00A4,&request.width,4)||!read(0x8A00A8,&request.height,4)||!read(0xA83D4C,&house,4)||house!=request.offer.house||!read(0xA8ED84,&frame,4)){
  notify(L"放置未提交：鼠标须在游戏地图内，菜单和失焦时不会放置。");return;
 }
 request.screenX=int(int64_t(point.x)*request.width/client.right);request.screenY=int(int64_t(point.y)*request.height/client.bottom);
 int camera[2]{},dimensions[2]{};uint32_t currentTactical=0,currentHouse=0;
 now=GetTickCount();
 if(DWORD(now-capturedAt)>100||!nativeplacement::screen(request)||!nativeplacement::fresh(request.offer,commandMatch(),now,frame)||
    !read(tactical+0xB0,camera,8)||camera[0]!=request.cameraX||camera[1]!=request.cameraY||
    !read(0x8A00A4,dimensions,8)||dimensions[0]!=request.width||dimensions[1]!=request.height||
    !read(0x887324,&currentTactical,4)||currentTactical!=tactical||!read(0xA83D4C,&currentHouse,4)||currentHouse!=house||
    !external::display::inspect(read).visible()||!placementEventReady(now)){
  notify(L"放置未提交：视野或对局已改变，请重新按键。");return;
 }
 now=GetTickCount();if(DWORD(now-capturedAt)>100||!placementEventReady(now)){notify(L"放置按键已过期，请重新按键。");return;}
 // Direct publication only. Generic send/queue/retry must never own this
 // frozen product and position. Runtime independently validates every field.
 inFlightMatch=commandMatch();inFlightRetries=0;sentAt=now;
 shared->placementRequest=request;shared->requestTypeHash=request.offer.typeHash;
 shared->requestToken=0;shared->command=PlaceCompletedBuilding;shared->arg=request.offer.slot;shared->arg2=0;
 shared->requestTick=capturedAt;shared->requestMatch=inFlightMatch;MemoryBarrier();
 InterlockedIncrement(reinterpret_cast<volatile LONG*>(&shared->request));placementSelection.chosen=false;
 if(ui){ControlUpdate guard;SendDlgItemMessageW(ui,9251,CB_SETCURSEL,WPARAM(-1),0);}wakeGame();
}
