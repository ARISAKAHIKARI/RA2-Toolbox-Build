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
 placementSelection.chosen=false;int tab=-1;externalReader.readFresh(0x87F7E8+0x539C,&tab,4);for(int n=0;n<placementSelection.count;++n)if(placementSelection.offers[n].tab==tab)placementSelection.choose(n);
 EnableWindow(list,FALSE);
 bool changed=before.count!=placementSelection.count||before.pid!=placementSelection.pid||before.match!=placementSelection.match||before.chosen!=placementSelection.chosen||(before.chosen&&placementSelection.chosen&&!nativeplacement::sameIdentity(before.selected,placementSelection.selected));
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
static void selectPlacementEvent(){} // Automatic native-tab selection; list is informational.
static bool placementEventReady(DWORD now){
 return enabled&&buildAnywhereMode&&extra.placementImplementation==buildingmodes::PlacementEvent&&
  game&&pid&&IsWindow(game)&&foregroundGame()&&shared&&commandFresh(now)&&!incoming.chat&&
  incoming.placementStatus==2&&snap.renderStatus==1&&!formationInputOwns()&&!shared->viewZoomBusy&&
  shared->request==shared->ack&&shared->ack==ack&&!commandQueue.size&&!hasSelection&&
  shared->selectRequest==shared->selectAck&&shared->pointer.request==shared->pointer.ack;
}
static void submitPlacementEvent(){
 const DWORD now=GetTickCount();
 auto read=[](uint32_t at,void*out,size_t n){return externalReader.readFresh(at,out,n);};
 if(!external::display::inspect(read).visible()||!placementEventReady(now)){notify(L"放置预览未开启：请等待游戏前台和命令连接就绪。");return;}
 // Runtime reads the native active tab and its exact completed product. No
 // manual list selection and no frozen pointer position crosses this request.
 inFlightMatch=commandMatch();inFlightRetries=0;sentAt=now;
 shared->requestToken=0;shared->requestTypeHash=0;shared->command=PlaceCompletedBuilding;shared->arg=0;shared->arg2=1;
 shared->requestTick=now;shared->requestMatch=inFlightMatch;MemoryBarrier();
 InterlockedIncrement(reinterpret_cast<volatile LONG*>(&shared->request));wakeGame();
}
