#include <utility>
#include "../../shared/source/key_gesture.h"
// UI changes apply immediately. Only disk writes are debounced; runtime
// revision changes must not wait for the profile file to be flushed.
static constexpr UINT_PTR AutoSaveTimer=3901;
static bool autoSavePending=false;
static void flushAutoSettings(){
 if(!autoSavePending)return;autoSavePending=false;KillTimer(ui,AutoSaveTimer);
 if(!save())notify(L"设置已经生效，但自动保存失败，请检查 Assist.ini 写入权限。");
}
static void changedSetting(){
 ++settingsRevision;++paintRevision;autoSavePending=true;
 SetTimer(ui,AutoSaveTimer,300,nullptr);syncTypeConfig();
}
static void syncSecondaryLayoutControls(){
 ControlUpdate guard;
 const std::pair<int,int> fields[]={
  {9164,extra.watchSecondaryX},{9165,extra.watchSecondaryY},
  {9166,minimapSecondaryX},{9167,minimapSecondaryY},{9168,minimapSecondaryWidth}
 };
 for(auto[id,value]:fields)if(HWND control=GetDlgItem(ui,id))SetWindowTextW(control,std::to_wstring(value).c_str());
}
static bool autoSettingChanged(int id,int notification){
 if(!ready||updatingControls)return false;
 const bool edit=notification==EN_CHANGE,select=notification==CBN_SELCHANGE;
 if(id==9253&&notification==BN_CLICKED){
  extra.destructSelectedOwner=SendMessageW(GetDlgItem(ui,id),BM_GETCHECK,0,0)==BST_CHECKED;
  registerKeys();changedSetting();updatePlayerTestUI(true);
  if(!extra.destructSelectedOwner)setConsoleFeedback(GetDlgItem(ui,9186),L"");return true;
 }
 if(id==9254&&edit){
  const WORD next=WORD(SendMessageW(GetDlgItem(ui,id),HKM_GETHOTKEY,0,0));const auto old=extra.destructSelectedOwnerKey;
  const bool conflict=destructOwnerKeyConflict(next);
  if(conflict){ControlUpdate guard;SendMessageW(GetDlgItem(ui,id),HKM_SETHOTKEY,old,0);notify(L"自毁必须使用独立快捷键；已保留原按键。");return true;}
  extra.destructSelectedOwnerKey=next;registerKeys();changedSetting();return true;
 }
 if(id==9420&&edit){extra.overlayKey=uint32_t(SendMessageW(GetDlgItem(ui,id),HKM_GETHOTKEY,0,0));changedSetting();return true;}
 if(id==9160&&notification==BN_CLICKED){infoAdaptive=SendMessageW(GetDlgItem(ui,id),BM_GETCHECK,0,0)==BST_CHECKED;syncInformationRate(true);changedSetting();return true;}
 if(id>=9000&&id<9000+compute::Count&&select){int mode=int(SendMessageW(GetDlgItem(ui,id),CB_GETCURSEL,0,0));uint32_t bit=1u<<(id-9000);
  if(mode==1)computeRequested|=bit;else computeRequested&=~bit;changedSetting();syncAdditional();notify(L"计算模式已自动保存，下一局生效；当前对局继续使用原设置。");return true;
 }
 if((id==860||id==722)&&notification==BN_CLICKED){
  bool on=SendMessageW(GetDlgItem(ui,id),BM_GETCHECK,0,0)==BST_CHECKED;
  if(id==860)extra.controlAll=on;else extra.sellEnemy=playertest::SellAvailable&&on;
  pointerMatches.clear();changedSetting();syncAdditional();return true;
 }
 if(id==861&&notification==BN_CLICKED){
  extra.switchPlayerEnabled=false;
  SendMessageW(GetDlgItem(ui,861),BM_SETCHECK,BST_UNCHECKED,0);
  pointerMatches.clear();changedSetting();syncAdditional();notify(L"此功能暂不可用。");return true;
 }
 if((id==897||id==898||id==899)&&notification==BN_CLICKED){bool on=SendMessageW(GetDlgItem(ui,id),BM_GETCHECK,0,0)==BST_CHECKED;if(id==897)extra.passengerBubbles=on;else if(id==898)extra.dangerPassengers=on;else extra.magnetronDrop=on;changedSetting();syncAdditional();return true;}
 if(id==9065&&select){int next=int(SendMessageW(GetDlgItem(ui,id),CB_GETCURSEL,0,0));if(next>=0&&next<=1){minimapMode=next;changedSetting();}return true;}
 if(id==850&&select){int next=int(SendMessageW(GetDlgItem(ui,id),CB_GETCURSEL,0,0));if(next>=0&&next<=3){minimapPosition=next;changedSetting();}return true;}
 if((id==9100||id==9101)&&select){int next=int(SendMessageW(GetDlgItem(ui,id),CB_GETCURSEL,0,0));if(next>=0&&next<=2){(id==9100?minimapAlignX:minimapAlignY)=next;changedSetting();}return true;}
 if((id==9162||id==9163)&&notification==BN_CLICKED){(id==9162?minimapSecondary:extra.watchSecondary)=SendMessageW(GetDlgItem(ui,id),BM_GETCHECK,0,0)==BST_CHECKED;registerKeys();syncInformationRate(true);changedSetting();return true;}
 if(id==830&&notification==BN_CLICKED){minimapEnabled=SendMessageW(GetDlgItem(ui,id),BM_GETCHECK,0,0)==BST_CHECKED;if(!minimapEnabled)minimap::hidePrimary();changedSetting();return true;}
 if((id==9090&&notification==BN_CLICKED)||(id==9091&&select)||(id==9092&&notification==BN_CLICKED)){
  if(id==9090)extra.viewZoomEnabled=SendMessageW(GetDlgItem(ui,id),BM_GETCHECK,0,0)==BST_CHECKED;
  if(id==9091){int index=int(SendMessageW(GetDlgItem(ui,id),CB_GETCURSEL,0,0));extra.viewZoomPercent=viewzoom::Levels[std::clamp(index,0,viewzoom::Count-1)];}
  if(id==9092){extra.viewZoomPercent=100;SendMessageW(GetDlgItem(ui,9091),CB_SETCURSEL,viewzoom::index(100),0);}
  registerKeys();changedSetting();syncAdditional();return true;
 }
 if(id>=9440&&id<=9447){int index=(id-9440)/2;
  if((id%2)==0&&edit){extra.moGestures.keys[index]=uint32_t(SendMessageW(GetDlgItem(ui,id),HKM_GETHOTKEY,0,0));pointerMatches.clear();changedSetting();syncAdditional();return true;}
  if((id%2)==1&&select){int physical=int(SendMessageW(GetDlgItem(ui,id),CB_GETCURSEL,0,0));if(physical>=0&&physical<=5){extra.moGestures.buttons[index]=physical==5?6:physical;pointerMatches.clear();changedSetting();syncAdditional();}return true;}
 }
 if(id>=9450&&id<=9455&&edit){uint32_t binding=uint32_t(SendMessageW(GetDlgItem(ui,id),HKM_GETHOTKEY,0,0));if(id<=9452)extra.viewZoomKeys[id-9450]=binding;else if(id==9453)extra.sidebarKey=binding;else extra.pauseKeys[id-9454]=binding;registerKeys();changedSetting();syncAdditional();return true;}
 if(id>=9400&&id<=9402&&notification==BN_CLICKED){uint32_t bit=1u<<(id-9400);if(SendMessageW(GetDlgItem(ui,id),BM_GETCHECK,0,0)==BST_CHECKED)extra.moFeatures|=bit;else extra.moFeatures&=~bit;pointerMatches.clear();changedSetting();syncAdditional();return true;}
 if(id==9403&&notification==BN_CLICKED){if(SendMessageW(GetDlgItem(ui,id),BM_GETCHECK,0,0)==BST_CHECKED)extra.moFeatures|=16u;else extra.moFeatures&=~16u;changedSetting();syncAdditional();return true;}
 if(id==9463&&notification==BN_CLICKED){extra.smallMapClamp=SendMessageW(GetDlgItem(ui,id),BM_GETCHECK,0,0)==BST_CHECKED;changedSetting();syncAdditional();return true;}
 if(id==9462&&notification==BN_CLICKED){extra.sidebarEnabled=SendMessageW(GetDlgItem(ui,id),BM_GETCHECK,0,0)==BST_CHECKED;registerKeys();changedSetting();syncAdditional();return true;}
 if(id==9404&&select){extra.sidebarMode=uint32_t(std::clamp(int(SendMessageW(GetDlgItem(ui,id),CB_GETCURSEL,0,0)),0,2));changedSetting();syncAdditional();return true;}
 if(id==9492&&notification==BN_CLICKED){
  const bool on=SendMessageW(GetDlgItem(ui,id),BM_GETCHECK,0,0)==BST_CHECKED;
  if(on&&(extra.mapStrategy!=0||extra.manualReveal)){ControlUpdate guard;SendMessageW(GetDlgItem(ui,id),BM_SETCHECK,BST_UNCHECKED,0);notify(L"请先关闭其他黑幕绘制方式与手动地图揭示。");return true;}
  extra.mapVisibility=on;changedSetting();syncAdditional();return true;
 }
 if(id==9490&&notification==BN_CLICKED){extra.superweaponGrant=SendMessageW(GetDlgItem(ui,id),BM_GETCHECK,0,0)==BST_CHECKED;changedSetting();syncAdditional();return true;}
 if(id==9405&&notification==BN_CLICKED){extra.superweaponPanel=SendMessageW(GetDlgItem(ui,id),BM_GETCHECK,0,0)==BST_CHECKED;changedSetting();syncAdditional();return true;}
 if(id==886&&select){extra.scrollRefreshMode=0;SendMessageW(GetDlgItem(ui,id),CB_SETCURSEL,0,0);changedSetting();syncAdditional();return true;}
 if(id==9096&&select){int mode=int(SendMessageW(GetDlgItem(ui,id),CB_GETCURSEL,0,0));if(mode>=0&&mode<=buildingmodes::PlacementEvent){extra.placementImplementation=mode;resetPlacementEvent();changedSetting();syncAdditional();registerKeys();updatePlacementEventUI();}return true;}
 if(id==9251&&select){selectPlacementEvent();return true;}
 if(id==9252&&edit){WORD next=WORD(SendMessageW(GetDlgItem(ui,id),HKM_GETHOTKEY,0,0));const auto old=extra.placementEventKey;extra.placementEventKey=0;
  bool conflict=keyConflict(next,"",false);extra.placementEventKey=old;
  if(conflict){ControlUpdate guard;SendMessageW(GetDlgItem(ui,id),HKM_SETHOTKEY,old,0);notify(L"快捷键冲突；已保留原按键，请换一个组合。");return true;}
  extra.placementEventKey=next;registerKeys();changedSetting();return true;
 }
 if(id==9097&&notification==BN_CLICKED){extra.techEnhance=SendMessageW(GetDlgItem(ui,id),BM_GETCHECK,0,0)==BST_CHECKED;changedSetting();syncAdditional();return true;}
 if(id==9472&&notification==BN_CLICKED){extra.techEnhanceCompareLevel=SendMessageW(GetDlgItem(ui,id),BM_GETCHECK,0,0)==BST_CHECKED;changedSetting();syncAdditional();return true;}
 if(id==9470&&select){extra.techEnhanceImplementation=std::clamp(int(SendMessageW(GetDlgItem(ui,id),CB_GETCURSEL,0,0)),0,1);changedSetting();syncAdditional();return true;}
 if(id>=9480&&id<=9485&&edit){BOOL ok=FALSE;const int value=int(GetDlgItemInt(ui,id,&ok,FALSE));const int lo=id==9483?1:2,hi=id==9483?8:16;
  if(!ok||value<lo||value>hi){setStatus(L"此项有效范围："+std::to_wstring(lo)+L"–"+std::to_wstring(hi));return true;}
  uint32_t*fields[]={&extra.moOptions.range,&extra.moOptions.squad,&extra.moOptions.patrolSize,&extra.moOptions.nodeStep,&extra.moOptions.loopClose,&extra.moOptions.loopStep};
  *fields[id-9480]=uint32_t(value);changedSetting();syncAdditional();return true;}
 if(id==9486&&notification==BN_CLICKED){extra.moOptions.commit=SendMessageW(GetDlgItem(ui,id),BM_GETCHECK,0,0)==BST_CHECKED;changedSetting();syncAdditional();return true;}
 if(id==810&&notification==BN_CLICKED){extra.c4Test=SendMessageW(GetDlgItem(ui,id),BM_GETCHECK,0,0)==BST_CHECKED;pointerMatches.clear();changedSetting();return true;}
 if(id==9180&&select){int mode=int(SendMessageW(GetDlgItem(ui,id),CB_GETCURSEL,0,0));if(mode>=0&&mode<=1){extra.c4Mode=uint32_t(mode);infantryEntryResult.clear();pointerMatches.clear();changedSetting();}return true;}
 if(id>=9181&&id<=9183&&notification==BN_CLICKED){bool on=SendMessageW(GetDlgItem(ui,id),BM_GETCHECK,0,0)==BST_CHECKED;if(id==9181)extra.missileReserve=on;else if(id==9182)extra.lowDamageFilter=on;else extra.lowDamageNetwork=on;changedSetting();syncAdditional();return true;}
 if((id==9187||id==9189)&&notification==BN_CLICKED){
  bool on=SendMessageW(GetDlgItem(ui,id),BM_GETCHECK,0,0)==BST_CHECKED;
  if(id==9187)extra.networkHousePreserve=on;else extra.nativeSellCursor=on;
  pointerMatches.clear();changedSetting();syncAdditional();updatePlayerTestUI(true);return true;
 }
 if(id==9191&&notification==BN_CLICKED){extra.nativeMegaGateway=SendMessageW(GetDlgItem(ui,id),BM_GETCHECK,0,0)==BST_CHECKED;changedSetting();syncAdditional();return true;}
 if(id==9198&&notification==BN_CLICKED){extra.nativeCombatQueries=SendMessageW(GetDlgItem(ui,id),BM_GETCHECK,0,0)==BST_CHECKED;changedSetting();syncAdditional();return true;}
 if(id==9193&&notification==BN_CLICKED){extra.controlCommandOwner=SendMessageW(GetDlgItem(ui,id),BM_GETCHECK,0,0)==BST_CHECKED;pointerMatches.clear();changedSetting();syncAdditional();return true;}
 if(id==9250&&select){int mode=int(SendMessageW(GetDlgItem(ui,id),CB_GETCURSEL,0,0));if(mode<0||mode>2)return true;
  extra.controlImplementation=mode;pointerMatches.clear();changedSetting();syncAdditional();
  notify(mode==0?L"已选择不改指令的控制实现；断点不足时会显示错误，不自动切回代码补丁。":mode==2?L"已选择 Master 基本指令修改；开启后直接修改三处游戏指令，不启用命令所属方或事件所属方保留。旧状态还原完成后生效，高危，仍有联机风险。":L"已选择原生判定 Hook；开启后会修改游戏代码，仍有联机风险。");return true;
 }
 if(id==9196&&notification==BN_CLICKED){extra.crcMapShadow=SendMessageW(GetDlgItem(ui,id),BM_GETCHECK,0,0)==BST_CHECKED;changedSetting();syncAdditional();return true;}
 if(id==9195&&select){int policy=int(SendMessageW(GetDlgItem(ui,id),CB_GETCURSEL,0,0));if(policy>=0&&policy<=2){extra.aircraftAllocationPolicy=policy;pointerMatches.clear();changedSetting();syncAdditional();}return true;}
 if(id==811&&edit){extra.c4Key=uint32_t(SendMessageW(GetDlgItem(ui,id),HKM_GETHOTKEY,0,0));changedSetting();return true;}
 if(id==812&&select){int selected=int(SendMessageW(GetDlgItem(ui,id),CB_GETCURSEL,0,0));if(selected>=0&&selected<=5){extra.c4Mouse=selected==5?6:selected;changedSetting();}return true;}
 if(id==808&&select&&extra.mapVisibility&&SendMessageW(GetDlgItem(ui,id),CB_GETCURSEL,0,0)!=0){ControlUpdate guard;SendMessageW(GetDlgItem(ui,id),CB_SETCURSEL,shroudpolicy::uiIndex(extra.mapStrategy),0);notify(L"请先关闭硬件黑幕显示实验。");return true;}
 if(id==808&&select){int strategy=shroudpolicy::strategyFromUI(int(SendMessageW(GetDlgItem(ui,id),CB_GETCURSEL,0,0)));if(strategy<0)return true;
  extra.mapStrategy=strategy;extra.renderReveal=shroudpolicy::runtimeMode(strategy);extra.experimentalReveal=shroudpolicy::revealEnabled(extra.manualReveal,strategy);autoRevealNewMatch=shroudpolicy::automaticReveal(strategy);
  for(int i=0;i<commandQueue.size;)if(commandQueue.slots[i].command==RevealMap)commandQueue.discard(i);else ++i;
  autoRevealMatch=autoRevealDone=autoRevealRetry=0;changedSetting();syncAdditional();notify(strategy==3?L"地图全开将在对局就绪后自动执行；此模式有联机不同步风险。":L"地图显示策略已更新。");return true;}
 if(id==9199&&notification==BN_CLICKED&&extra.mapVisibility&&SendMessageW(GetDlgItem(ui,id),BM_GETCHECK,0,0)==BST_CHECKED){ControlUpdate guard;SendMessageW(GetDlgItem(ui,id),BM_SETCHECK,BST_UNCHECKED,0);notify(L"请先关闭硬件黑幕显示实验。");return true;}
 if(id==9199&&notification==BN_CLICKED){
  extra.manualReveal=SendMessageW(GetDlgItem(ui,id),BM_GETCHECK,0,0)==BST_CHECKED;
  extra.experimentalReveal=shroudpolicy::revealEnabled(extra.manualReveal,extra.mapStrategy);
  if(!extra.experimentalReveal)for(int i=0;i<commandQueue.size;)if(commandQueue.slots[i].command==RevealMap)commandQueue.discard(i);else ++i;
  changedSetting();syncAdditional();registerKeys();notify(extra.manualReveal?L"已允许手动揭示；按快捷键或点击立即揭示才会执行。":L"手动揭示已关闭。");return true;
 }

 if(id==895&&select){int mode=int(SendMessageW(GetDlgItem(ui,id),CB_GETCURSEL,0,0));if(mode>=0&&mode<=1){extra.dodgeMode=mode;changedSetting();syncAdditional();}return true;}
 if(id==885&&notification==BN_CLICKED){extra.dodgeEnabled=SendMessageW(GetDlgItem(ui,id),BM_GETCHECK,0,0)==BST_CHECKED;changedSetting();syncAdditional();return true;}
 if((id==9040||id==9041)&&select){int v=int(SendMessageW(GetDlgItem(ui,id),CB_GETCURSEL,0,0));if(v>=0&&v<=1){(id==9040?extra.missileScope:extra.missileAllocation)=v;changedSetting();syncAdditional();}return true;}
 if(id==9042&&notification==BN_CLICKED){extra.magnetronGlobal=SendMessageW(GetDlgItem(ui,id),BM_GETCHECK,0,0)==BST_CHECKED;changedSetting();syncAdditional();return true;}
 if((id==9060||id==9061)&&notification==BN_CLICKED){(id==9060?extra.watchEnabled:extra.watchDanger)=SendMessageW(GetDlgItem(ui,id),BM_GETCHECK,0,0)==BST_CHECKED;registerKeys();changedSetting();return true;}
 if(id==9095&&edit){extra.stickyKey=uint32_t(SendMessageW(GetDlgItem(ui,id),HKM_GETHOTKEY,0,0));registerKeys();changedSetting();return true;}
 if(id==9094&&notification==BN_CLICKED){send(StickyGuard);return true;}
 if(id>=9170&&id<9170+2*selectedmode::Count){int index=(id-9170)/2;
  if((id-9170)%2&&edit){extra.missionModeKeys[index]=uint32_t(SendMessageW(GetDlgItem(ui,id),HKM_GETHOTKEY,0,0));registerKeys();changedSetting();return true;}
  if(!((id-9170)%2)&&notification==BN_CLICKED){send(selectedmode::command(index));return true;}
 }
 if(id>=9062&&id<=9064&&edit){extra.watchKeys[id-9062]=uint32_t(SendMessageW(GetDlgItem(ui,id),HKM_GETHOTKEY,0,0));registerKeys();changedSetting();return true;}
 if(id==9056&&select){int v=int(SendMessageW(GetDlgItem(ui,id),CB_GETCURSEL,0,0));if(v==0||v==1){extra.formationMouse=v==1?1:2;changedSetting();syncAdditional();}return true;}
 if(id==9050&&notification==BN_CLICKED){extra.formationEnabled=SendMessageW(GetDlgItem(ui,id),BM_GETCHECK,0,0)==BST_CHECKED;changedSetting();syncAdditional();return true;}
 if(((id>=9051&&id<=9055)||id==9057||id==9058)&&edit){uint32_t k=uint32_t(SendMessageW(GetDlgItem(ui,id),HKM_GETHOTKEY,0,0));if(id==9051)extra.formationHold=k;else extra.formationKeys[id>=9057?id-9053:id-9052]=k;changedSetting();syncAdditional();return true;}
 if(id==880&&notification==BN_CLICKED){extra.missileDefense=SendMessageW(GetDlgItem(ui,id),BM_GETCHECK,0,0)==BST_CHECKED;changedSetting();syncAdditional();return true;}
 if(id==804&&notification==BN_CLICKED){extra.antiTheft=SendMessageW(GetDlgItem(ui,id),BM_GETCHECK,0,0)==BST_CHECKED;changedSetting();return true;}
 if(id==803&&select){int mode=int(SendMessageW(GetDlgItem(ui,id),CB_GETCURSEL,0,0));if(mode>=0&&mode<=4){extra.airWaypoints=mode==1;extra.nativeAirWaypoints=mode>=2;extra.nativeAirNetwork=mode>=3?uint32_t(mode-2):0;airway::reset();pointerMatches.clear();changedSetting();syncAdditional();}return true;}
 if(id==894&&edit){extra.airPathKey=uint32_t(SendMessageW(GetDlgItem(ui,id),HKM_GETHOTKEY,0,0));airway::reset();pointerMatches.clear();changedSetting();syncAdditional();return true;}
 if(id==802&&select){int priority=int(SendMessageW(GetDlgItem(ui,id),CB_GETCURSEL,0,0));if(priority>=0&&priority<=1){advancedTargetPriority=priority;changedSetting();}return true;}
 if(id==799&&notification==BN_CLICKED){extra.constructionQueue=SendMessageW(GetDlgItem(ui,id),BM_GETCHECK,0,0)==BST_CHECKED;changedSetting();return true;}
 if(id>=9070&&id<=9075&&notification==BN_CLICKED){uint32_t bit=1u<<(id-9070);if(SendMessageW(GetDlgItem(ui,id),BM_GETCHECK,0,0)==BST_CHECKED)extra.productionReminderMask|=bit;else extra.productionReminderMask&=~bit;changedSetting();return true;}
 if(id==798&&notification==BN_CLICKED){extra.idleAircraft=SendMessageW(GetDlgItem(ui,id),BM_GETCHECK,0,0)==BST_CHECKED;changedSetting();return true;}
 if(id==795&&notification==BN_CLICKED){shiftClickEnabled=SendMessageW(GetDlgItem(ui,id),BM_GETCHECK,0,0)==BST_CHECKED;changedSetting();return true;}
 if(id==796&&select){int mode=int(SendMessageW(GetDlgItem(ui,id),CB_GETCURSEL,0,0));if(mode>=0&&mode<=2){shiftClickMode=mode;changedSetting();}return true;}
 if(id==789&&edit){extra.sellKey=uint32_t(SendMessageW(GetDlgItem(ui,id),HKM_GETHOTKEY,0,0));ControlUpdate guard;SendMessageW(extraControls[SellPreset],CB_SETCURSEL,holdPreset(extra.sellKey),0);SendMessageW(extraControls[SellCustom],HKM_SETHOTKEY,extra.sellKey,0);changedSetting();return true;}
 if(id==788&&notification==BN_CLICKED){extra.overrideNative=SendMessageW(GetDlgItem(ui,id),BM_GETCHECK,0,0)==BST_CHECKED;changedSetting();return true;}
 if(id==786&&notification==BN_CLICKED){extra.allHotkeys=SendMessageW(GetDlgItem(ui,id),BM_GETCHECK,0,0)==BST_CHECKED;typeTap.clear();registerKeys();changedSetting();return true;}
 if((id==892||id==893)&&notification==BN_CLICKED){bool on=SendMessageW(GetDlgItem(ui,id),BM_GETCHECK,0,0)==BST_CHECKED;if(id==892)extra.parasiteMarker=on;else extra.crateBuffMarkers=on;changedSetting();return true;}
 if(id==785&&notification==BN_CLICKED){extra.enemyMarkers=SendMessageW(GetDlgItem(ui,id),BM_GETCHECK,0,0)==BST_CHECKED;changedSetting();return true;}
 if(id>=9200&&id<9200+2*featuretypes::Count){int f=(id-9200)/2;
  if(!(id&1)&&notification==BN_CLICKED){featureTypes.filters[f].custom=SendMessageW(GetDlgItem(ui,id),BM_GETCHECK,0,0)==BST_CHECKED;changedSetting();}
  else if((id&1)&&edit){wchar_t text[13000]{};GetWindowTextW(GetDlgItem(ui,id),text,13000);auto next=featureTypes.filters[f];if(!oreconfig::parse(std::wstring(text),next.ids,next.count)){setStatus(L"类型 ID 最多 512 项，每项 23 字符。");return true;}featureTypes.filters[f]=next;featureTypeText[f]=text;changedSetting();}
  return true;
 }

 if(id==780&&notification==BN_CLICKED){repairUnrepairable=SendMessageW(GetDlgItem(ui,id),BM_GETCHECK,0,0)==BST_CHECKED;changedSetting();return true;}
 if((id==781||id==782)&&notification==BN_CLICKED){(id==781?oreTypes.customMiners:oreTypes.customRefineries)=SendMessageW(GetDlgItem(ui,id),BM_GETCHECK,0,0)==BST_CHECKED;changedSetting();return true;}
 if((id==783||id==784)&&edit){wchar_t text[13000]{};GetWindowTextW(GetDlgItem(ui,id),text,13000);auto next=oreTypes;bool miner=id==783;
 if(!oreconfig::parse(std::wstring(text),miner?next.miners:next.refineries,miner?next.minerCount:next.refineryCount)){setStatus(L"类型 ID 只能用英文、数字、下划线或短横线；最多 512 项，每项 23 字符。");return true;}
 oreTypes=next;(miner?oreMiners:oreRefineries)=text;changedSetting();return true;}

 auto number=[&](int lo,int hi,int&target){BOOL ok=FALSE;int value=int(GetDlgItemInt(ui,id,&ok,FALSE));
  if(!ok||value<lo||value>hi){setStatus(L"此项有效范围："+std::to_wstring(lo)+L"–"+std::to_wstring(hi)+L"；输入有效值后自动生效。");return false;}
  target=value;return true;};
 int* numeric=nullptr;int lo=0,hi=0;
 switch(id){case 851:numeric=&minimapX;hi=32768;break;case 852:numeric=&minimapY;hi=32768;break;case 831:numeric=&minimapScale;lo=5;hi=200;break;
 case 9164:numeric=&extra.watchSecondaryX;hi=10000;break;case 9165:numeric=&extra.watchSecondaryY;hi=10000;break;
 case 9166:numeric=&minimapSecondaryX;hi=10000;break;case 9167:numeric=&minimapSecondaryY;hi=10000;break;
 case 9168:numeric=&minimapSecondaryWidth;lo=160;hi=4096;break;
 case 310:numeric=&fps;lo=5;hi=500;break;case 311:numeric=&topMargin;hi=4096;break;
 case 816:numeric=&labelScalePercent;lo=50;hi=250;break;
 case 817:numeric=&constructionScalePercent;lo=50;hi=250;break;
 case 797:numeric=&shiftClickGap;lo=0;hi=250;break;
 case 818:numeric=&advancedRangeCells;lo=0;hi=30;break;
#ifndef RA2_UNIFIED
 case 887:numeric=&extra.scrollRefreshHz;lo=30;hi=500;break;
#endif
 case 881:numeric=&extra.missileScanHz;lo=1;hi=60;break;
 case 787:numeric=&advancedScanHz;lo=1;hi=60;break;
 case 312:numeric=&repairMs;lo=500;hi=30000;break;case 444:numeric=&doubleMs;lo=20;hi=5000;break;
 case 471:numeric=&toastMs;lo=500;hi=60000;break;case 701:numeric=&extra.loadRadius;lo=1;hi=512;break;
 case 710:numeric=&extra.defaults.first;hi=512;break;case 711:numeric=&extra.defaults.second;hi=512;break;
 case 712:numeric=&extra.defaults.maximum;hi=512;break;}
 if(numeric&&edit){if(number(lo,hi,*numeric)){if(id==310)syncInformationRate(true);if(id==444)typeTap.clear();changedSetting();if(id==887)syncAdditional();}return true;}
 if(id==714&&edit){if(!quotaID().empty())readTargetQuota();return true;}
 if((id==715&&notification==BN_CLICKED)||(id>=716&&id<=718&&edit)){
  auto type=quotaID();if(type.empty()){setStatus(L"请先选择类型或输入有效的对象 ID。");return true;}
  if(id==715&&SendMessageW(extraControls[QuotaOverride],BM_GETCHECK,0,0)!=BST_CHECKED){
   targetQuotas.erase(type);readTargetQuota();changedSetting();return true;
  }
  cooperative::Quota q;if(!extraNumber(QuotaFirst,0,512,q.first)||!extraNumber(QuotaSecond,0,512,q.second)||!extraNumber(QuotaMaximum,0,512,q.maximum)){
   setStatus(L"受击数有效范围为 0–512；输入有效值后自动生效。");return true;}
  if(id!=715)SendMessageW(extraControls[QuotaOverride],BM_SETCHECK,BST_CHECKED,0);
  if(SendMessageW(extraControls[QuotaOverride],BM_GETCHECK,0,0)==BST_CHECKED)targetQuotas[type]=q;else targetQuotas.erase(type);
  changedSetting();return true;
 }
 if(id==700&&select){int mode=int(SendMessageW(extraControls[LoadLogic],CB_GETCURSEL,0,0));if(mode>=0&&mode<=1){extra.loadMode=mode==1;changedSetting();}return true;}
 if((id==703||id==706||id==708)&&select||(id==704||id==707||id==709)&&edit){
  // Validate only this binding, so a partially typed number elsewhere cannot
  // block a key change. Direct field edits retain their complete gesture and
  // select the matching preset without round-tripping through the old choice.
  int slot=id<=704?0:id<=707?1:2;
  const int presets[]={EnterPreset,AirPreset1,AirPreset2},customs[]={EnterCustom,AirCustom1,AirCustom2};
  ControlUpdate guard;
  const auto raw=uint32_t(SendMessageW(extraControls[customs[slot]],HKM_GETHOTKEY,0,0));
  uint32_t key=edit?raw:presetKey(int(SendMessageW(extraControls[presets[slot]],CB_GETCURSEL,0,0)),raw);
  if(edit)SendMessageW(extraControls[presets[slot]],CB_SETCURSEL,holdPreset(key),0);
  uint32_t* target=slot==0?&extra.enterKey:slot==1?&extra.airKey1:&extra.airKey2;
  uint32_t other=slot==1?extra.airKey2:slot==2?extra.airKey1:0;
  if(!extra.allHotkeys&&keygesture::physical(key)&&keygesture::normalize(key,keygesture::Mode::Hold)==keygesture::normalize(other,keygesture::Mode::Hold)){SendMessageW(extraControls[presets[slot]],CB_SETCURSEL,holdPreset(*target),0);SendMessageW(extraControls[customs[slot]],HKM_SETHOTKEY,*target,0);notify(L"飞机攻击的两个按键需要不同。");return true;}
  *target=key;SendMessageW(extraControls[customs[slot]],HKM_SETHOTKEY,key,0);changedSetting();return true;
 }
 if((id==725||id==727)&&select||(id==726||id==728)&&edit){
  const bool disk=id<=726;int preset=disk?DiskPreset:SellPreset,custom=disk?DiskCustom:SellCustom;
  ControlUpdate guard;
  const auto raw=uint32_t(SendMessageW(extraControls[custom],HKM_GETHOTKEY,0,0));
  auto key=edit?raw:presetKey(int(SendMessageW(extraControls[preset],CB_GETCURSEL,0,0)),raw);
  if(edit)SendMessageW(extraControls[preset],CB_SETCURSEL,holdPreset(key),0);
  (disk?extra.diskKey:extra.sellKey)=key;if(!disk)SendMessageW(GetDlgItem(ui,789),HKM_SETHOTKEY,key,0);SendMessageW(extraControls[custom],HKM_SETHOTKEY,key,0);changedSetting();return true;
 }
 if(id>=729&&id<729+pointerinput::MouseCount&&select){
  const int slot=id-729;int key=int(SendMessageW(extraControls[MouseFirst+slot],CB_GETCURSEL,0,0));
  if(key<0||key>5)return true;
  extra.mouseButtons[slot]=key==5?6:key;changedSetting();return true;
 }
 if(!edit)return false;
 WORD* target=nullptr;HWND widget=GetDlgItem(ui,id);
 if(id>=870&&id<=878)target=&socialKeys[id-870];
 else if(id>=200&&id<212)target=&hotkeys[id-200];
 else switch(id){case 853:target=&idleAircraftKey;break;case 447:target=&csKey;break;case 450:target=&advancedKey;break;case 451:target=&unloadKey;break;
 case 456:target=&ejectKey;break;case 474:target=&csHoldKey;break;case 478:target=&magnetronKey;break;}
 if(target){WORD next=WORD(SendMessageW(widget,HKM_GETHOTKEY,0,0)),old=*target;*target=0;
  bool conflict=keyConflict(next,"",false);*target=old;
  if(conflict){ControlUpdate guard;SendMessageW(widget,HKM_SETHOTKEY,old,0);notify(L"快捷键冲突；已保留原按键，请换一个组合。");return true;}
  // Editing a binding configures the feature; only its explicit switch enables it.
  *target=next;typeTap.clear();registerKeys();changedSetting();return true;
 }
 if(id==724){WORD next=WORD(SendMessageW(widget,HKM_GETHOTKEY,0,0));uint32_t old=extra.variantKey;extra.variantKey=0;
  bool conflict=keyConflict(next,"",false);extra.variantKey=old;
  if(conflict){ControlUpdate guard;SendMessageW(widget,HKM_SETHOTKEY,old,0);notify(L"快捷键冲突；已保留原按键，请换一个组合。");return true;}
  extra.variantKey=next;registerKeys();changedSetting();return true;}
 if(id==411){ControlUpdate guard;
  std::string type=selectedSuperID();
  if(type.empty()){notify(L"进入对局并获得超武后即可绑定。");return true;}
  WORD old=binding(type.c_str()),next=WORD(SendMessageW(widget,HKM_GETHOTKEY,0,0));
  for(auto&b:swBindings)if(b.id==type)b.key=0;
  bool conflict=keyConflict(next,"",false);for(auto&b:swBindings)if(b.id==type)b.key=old;
  if(conflict){SendMessageW(widget,HKM_SETHOTKEY,old,0);notify(L"快捷键冲突；已保留原按键，请换一个组合。");return true;}
  bindSuper();return true;}
 if(id==441){ControlUpdate guard;int index=selectedTypeIndex();
  if(index<0){notify(L"请先选择单位类型。");return true;}
  const char*type=savedTypes.types[index].id;WORD next=WORD(SendMessageW(widget,HKM_GETHOTKEY,0,0));
  if(keyConflict(next,type,false)){SendMessageW(widget,HKM_SETHOTKEY,typeBinding(type),0);notify(L"快捷键冲突；已保留原按键，请换一个组合。");return true;}
  bindType(false);return true;}
 return false;
}



