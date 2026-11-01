#pragma once
#include <map>
#include "dependency_controls.h"

// Included after PageControl and extra_settings_io.h. All mutation paths share
// this availability model, including the suite's forwarded display requests.
static settingsdeps::State currentSettingDependencies(){
 using settingsdeps::Need;settingsdeps::State s;
 s.set(Need::Render,renderUpgradeEnabled);
#ifdef _WIN64
 s.set(Need::Host64,true);
#endif
 s.set(Need::GPU,renderVulkanEnabled);s.set(Need::Detail,effectiveRenderDetail()!=0);
 s.set(Need::Quality,renderProcessing);s.set(Need::Reconstruction,renderXbrzEnabled);
 s.set(Need::Missile,extra.missileDefense);s.set(Need::LowDamage,extra.lowDamageFilter);
 s.set(Need::ControlAll,extra.controlAll);s.set(Need::Formation,extra.formationEnabled);
 s.set(Need::NativeControl,extra.nativeControlRequested());s.set(Need::NetworkControlAllowed,!extra.masterControlRequested());
 s.set(Need::Watch,extra.watchEnabled||extra.watchSecondary);s.set(Need::WatchSecondary,extra.watchSecondary);
 s.set(Need::Minimap,minimapEnabled||minimapSecondary);s.set(Need::MinimapPrimary,minimapEnabled);
 s.set(Need::MinimapSecondary,minimapSecondary);s.set(Need::CustomMinimap,minimapPosition==3);
 s.set(Need::ShiftClick,shiftClickEnabled);s.set(Need::AppendClicks,shiftClickMode!=2);
 s.set(Need::Ore,oreMode);s.set(Need::CustomMiners,oreTypes.customMiners);s.set(Need::CustomRefineries,oreTypes.customRefineries);
 s.set(Need::Production,check[2]&&SendMessageW(check[2],BM_GETCHECK,0,0)==BST_CHECKED);
 s.set(Need::ManualRate,!infoAdaptive);s.set(Need::Capacity,check[0]&&SendMessageW(check[0],BM_GETCHECK,0,0)==BST_CHECKED);
 s.set(Need::Advanced,hookAdvanced);s.set(Need::C4,extra.c4Test);s.set(Need::Dodge,extra.dodgeEnabled);
 s.set(Need::Construction,extra.constructionQueue);s.set(Need::BuildAnywhere,buildAnywhereMode);
 s.set(Need::Repair,repairCheck&&SendMessageW(repairCheck,BM_GETCHECK,0,0)==BST_CHECKED);
 s.set(Need::ManualReveal,extra.manualReveal);
 s.set(Need::Aircraft,extra.air);s.set(Need::AircraftQuotas,extra.aircraftAllocationPolicy==0);
 s.set(Need::QuotaTarget,!quotaID().empty());
 s.set(Need::QuotaOverride,extraControls[QuotaOverride]&&SendMessageW(extraControls[QuotaOverride],BM_GETCHECK,0,0)==BST_CHECKED);
 for(int f=0;f<featuretypes::Count;++f)s.set(settingsdeps::typeListNeed(f),featureTypes.filters[f].custom!=0);
 return s;
}
static std::map<int,settingsdeps::ControlBinding> dependencyBindings;
struct DependencyCaption {std::wstring original,applied;};
static std::map<HWND,DependencyCaption> dependencyCaptions;
static std::map<HWND,std::wstring> dependencyTips;
static std::wstring dependencyWindowText(HWND w){int n=GetWindowTextLengthW(w);std::vector<wchar_t> value(size_t(n)+1);GetWindowTextW(w,value.data(),n+1);return value.data();}
static DependencyCaption& dependencyCaption(HWND w){
 auto found=dependencyCaptions.find(w);if(found==dependencyCaptions.end()){auto text=dependencyWindowText(w);found=dependencyCaptions.emplace(w,DependencyCaption{text,text}).first;}return found->second;
}
static void syncSettingDependencies(bool force){
 if(!ready||!ui||updatingControls)return;
 static DWORD polled=0;const DWORD now=GetTickCount();if(!force&&polled&&DWORD(now-polled)<120)return;polled=now;
 const auto state=currentSettingDependencies();static uint64_t previous=~uint64_t(0);
 if(!force&&state.available==previous)return;previous=state.available;
 ControlUpdate guard;std::map<HWND,std::wstring> captions;
 for(int id:settingsdeps::Controls){
  HWND control=GetDlgItem(ui,id);if(!control)continue;
  auto found=dependencyBindings.find(id);if(found==dependencyBindings.end())found=dependencyBindings.emplace(id,settingsdeps::ControlBinding(control)).first;
  const auto availability=settingsdeps::decision(id,state);found->second.refresh(availability);
  const PageControl* placed=nullptr;for(const auto&c:pageControls)if(c.hwnd==control){placed=&c;break;}
  std::wstring help=placed?placed->help:L"";
  if(help.empty())help=dependencyWindowText(control);
  if(!availability.enabled())help=std::wstring(L"当前不可用：")+settingsdeps::reason(availability.missing)+(availability.missing==settingsdeps::Need::Unavailable?L"。相关快捷键与旧启用设置不执行。\r\n":L"。已保存的子设置保留，开启父功能后恢复。\r\n")+help;
  if(dependencyTips[control]!=help){dependencyTips[control]=help;modernui::tip(control,help);}
  auto addCaption=[&](HWND w){auto&text=dependencyCaption(w);captions[w]=availability.enabled()?text.original:std::wstring(L"[")+settingsdeps::reason(availability.missing)+L"] "+text.original;};
  wchar_t cls[32]{};GetClassNameW(control,cls,32);if(!_wcsicmp(cls,L"BUTTON"))addCaption(control);
  // Combo/edit captions have ID zero. Keep the reason visible beside their
  // disabled field, rather than relying exclusively on hover tooltips.
  else if(placed)for(const auto&c:pageControls)if(c.page==placed->page&&c.rect.y==placed->rect.y&&c.rect.x<placed->rect.x&&!GetDlgCtrlID(c.hwnd)){
   GetClassNameW(c.hwnd,cls,32);if(!_wcsicmp(cls,L"STATIC"))addCaption(c.hwnd);
  }
 }
 for(const auto&entry:captions){auto&caption=dependencyCaption(entry.first);if(caption.applied!=entry.second){caption.applied=entry.second;SetWindowTextW(entry.first,entry.second.c_str());}}
}
static bool rejectUnavailableSetting(int id,int notification){
 if(notification!=BN_CLICKED&&notification!=CBN_SELCHANGE&&notification!=EN_CHANGE)return false;
 const auto availability=settingsdeps::decision(id,currentSettingDependencies());if(availability.enabled())return false;
 {ControlUpdate guard;auto found=dependencyBindings.find(id);if(found!=dependencyBindings.end())found->second.rejectChange(availability);}
 notify(std::wstring(settingsdeps::reason(availability.missing))+L"；原有子设置已保留。");return true;
}
struct DependencyCommandRefresh {
 bool refresh=ready&&!updatingControls;
 ~DependencyCommandRefresh(){if(refresh)syncSettingDependencies(true);}
};
