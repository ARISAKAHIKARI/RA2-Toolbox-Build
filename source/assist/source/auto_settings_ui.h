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
static bool autoSettingChanged(int id,int notification){
 if(!ready||updatingControls)return false;
 const bool edit=notification==EN_CHANGE,select=notification==CBN_SELCHANGE;
 if(id>=9000&&id<9000+compute::Count&&select){int mode=int(SendMessageW(GetDlgItem(ui,id),CB_GETCURSEL,0,0));uint32_t bit=1u<<(id-9000);
  if(mode==1)computeRequested|=bit;else computeRequested&=~bit;changedSetting();syncAdditional();notify(L"计算模式已自动保存，下一局生效；当前对局继续使用原设置。");return true;
 }
 if((id==860||id==722)&&notification==BN_CLICKED){
  bool on=SendMessageW(GetDlgItem(ui,id),BM_GETCHECK,0,0)==BST_CHECKED;
  if(id==860)extra.controlAll=on;else extra.sellEnemy=on;
  pointerMatches.clear();changedSetting();syncAdditional();return true;
 }
 if(id==861&&notification==BN_CLICKED){
  extra.switchPlayerEnabled=false;
  SendMessageW(GetDlgItem(ui,861),BM_SETCHECK,BST_UNCHECKED,0);
  pointerMatches.clear();changedSetting();syncAdditional();notify(L"此功能暂不可用。");return true;
 }
 if(id==850&&select){int next=int(SendMessageW(GetDlgItem(ui,id),CB_GETCURSEL,0,0));if(next>=0&&next<=3){minimapPosition=next;changedSetting();}return true;}
 if(id==830&&notification==BN_CLICKED){minimapEnabled=SendMessageW(GetDlgItem(ui,id),BM_GETCHECK,0,0)==BST_CHECKED;if(!minimapEnabled)minimap::hide();changedSetting();return true;}
 if(id==886&&select){int mode=int(SendMessageW(GetDlgItem(ui,id),CB_GETCURSEL,0,0));if(mode>=0&&mode<=2){extra.scrollRefreshMode=mode;changedSetting();syncAdditional();}return true;}
 if(id==810&&notification==BN_CLICKED){extra.c4Test=SendMessageW(GetDlgItem(ui,id),BM_GETCHECK,0,0)==BST_CHECKED;pointerMatches.clear();changedSetting();return true;}
 if(id==811&&edit){extra.c4Key=uint32_t(SendMessageW(GetDlgItem(ui,id),HKM_GETHOTKEY,0,0));changedSetting();return true;}
 if(id==812&&select){int selected=int(SendMessageW(GetDlgItem(ui,id),CB_GETCURSEL,0,0));extra.c4Mouse=selected==5?6:selected;changedSetting();return true;}
 if(id==808&&select){int strategy=int(SendMessageW(GetDlgItem(ui,id),CB_GETCURSEL,0,0));if(strategy<0||strategy>3)return true;
  extra.mapStrategy=strategy;extra.renderReveal=strategy==1?2:strategy==2?1:0;extra.experimentalReveal=strategy==3;autoRevealNewMatch=strategy==3;
  for(int i=0;i<commandQueue.size;)if(commandQueue.slots[i].command==RevealMap)commandQueue.discard(i);else ++i;
  autoRevealMatch=autoRevealDone=autoRevealRetry=0;changedSetting();syncAdditional();notify(strategy==3?L"地图全开将在对局就绪后自动执行；此模式有联机不同步风险。":L"地图显示策略已更新。");return true;}

 if(id==895&&select){int mode=int(SendMessageW(GetDlgItem(ui,id),CB_GETCURSEL,0,0));if(mode>=0&&mode<=1){extra.dodgeMode=mode;changedSetting();syncAdditional();}return true;}
 if(id==885&&notification==BN_CLICKED){extra.dodgeEnabled=SendMessageW(GetDlgItem(ui,id),BM_GETCHECK,0,0)==BST_CHECKED;changedSetting();syncAdditional();return true;}
 if(id==880&&notification==BN_CLICKED){extra.missileDefense=SendMessageW(GetDlgItem(ui,id),BM_GETCHECK,0,0)==BST_CHECKED;changedSetting();syncAdditional();return true;}
 if(id==804&&notification==BN_CLICKED){extra.antiTheft=SendMessageW(GetDlgItem(ui,id),BM_GETCHECK,0,0)==BST_CHECKED;changedSetting();return true;}
 if(id==803&&select){int mode=int(SendMessageW(GetDlgItem(ui,id),CB_GETCURSEL,0,0));if(mode>=0&&mode<=2){extra.airWaypoints=mode==1;extra.nativeAirWaypoints=mode==2;airway::reset();pointerMatches.clear();changedSetting();syncAdditional();}return true;}
 if(id==894&&edit){extra.airPathKey=uint32_t(SendMessageW(GetDlgItem(ui,id),HKM_GETHOTKEY,0,0));airway::reset();pointerMatches.clear();changedSetting();syncAdditional();return true;}
 if(id==802&&select){advancedTargetPriority=int(SendMessageW(GetDlgItem(ui,id),CB_GETCURSEL,0,0));changedSetting();return true;}
 if(id==799&&notification==BN_CLICKED){extra.constructionQueue=SendMessageW(GetDlgItem(ui,id),BM_GETCHECK,0,0)==BST_CHECKED;changedSetting();return true;}
 if(id==798&&notification==BN_CLICKED){extra.idleAircraft=SendMessageW(GetDlgItem(ui,id),BM_GETCHECK,0,0)==BST_CHECKED;changedSetting();return true;}
 if(id==795&&notification==BN_CLICKED){shiftClickEnabled=SendMessageW(GetDlgItem(ui,id),BM_GETCHECK,0,0)==BST_CHECKED;changedSetting();return true;}
 if(id==796&&select){shiftClickMode=int(SendMessageW(GetDlgItem(ui,id),CB_GETCURSEL,0,0));changedSetting();return true;}
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
 switch(id){case 851:numeric=&minimapX;hi=32768;break;case 852:numeric=&minimapY;hi=32768;break;case 831:numeric=&minimapScale;lo=100;hi=400;break;case 310:numeric=&fps;lo=5;hi=500;break;case 311:numeric=&topMargin;hi=4096;break;
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
 if(numeric&&edit){if(number(lo,hi,*numeric)){if(id==310)clockState(foregroundGame()&&snap.status==1&&enabled,fps);if(id==444)typeTap.clear();changedSetting();if(id==887)syncAdditional();}return true;}
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
 if(id==700&&select){extra.loadMode=int(SendMessageW(extraControls[LoadLogic],CB_GETCURSEL,0,0))==1;changedSetting();return true;}
 if((id==703||id==706||id==708)&&select||(id==704||id==707||id==709)&&edit){
  // Validate only this binding, so a partially typed number elsewhere cannot
  // block a key change. A custom key also selects its custom preset.
  int slot=id<=704?0:id<=707?1:2;
  const int presets[]={EnterPreset,AirPreset1,AirPreset2},customs[]={EnterCustom,AirCustom1,AirCustom2};
  ControlUpdate guard;if(edit)SendMessageW(extraControls[presets[slot]],CB_SETCURSEL,4,0);
  uint32_t key=presetKey(int(SendMessageW(extraControls[presets[slot]],CB_GETCURSEL,0,0)),uint32_t(SendMessageW(extraControls[customs[slot]],HKM_GETHOTKEY,0,0)));
  uint32_t* target=slot==0?&extra.enterKey:slot==1?&extra.airKey1:&extra.airKey2;
  uint32_t other=slot==1?extra.airKey2:slot==2?extra.airKey1:0;
  if(!extra.allHotkeys&&key&&key==other){SendMessageW(extraControls[presets[slot]],CB_SETCURSEL,holdPreset(*target),0);SendMessageW(extraControls[customs[slot]],HKM_SETHOTKEY,*target,0);notify(L"飞机攻击的两个按键需要不同。");return true;}
  *target=key;SendMessageW(extraControls[customs[slot]],HKM_SETHOTKEY,key,0);changedSetting();return true;
 }
 if((id==725||id==727)&&select||(id==726||id==728)&&edit){
  const bool disk=id<=726;int preset=disk?DiskPreset:SellPreset,custom=disk?DiskCustom:SellCustom;
  ControlUpdate guard;if(edit)SendMessageW(extraControls[preset],CB_SETCURSEL,4,0);
  auto key=presetKey(int(SendMessageW(extraControls[preset],CB_GETCURSEL,0,0)),uint32_t(SendMessageW(extraControls[custom],HKM_GETHOTKEY,0,0)));
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
  *target=next;if(id==451){hookSmartUnload=true;SendMessageW(hookChecks[1],BM_SETCHECK,BST_CHECKED,0);}if(id==450){hookAdvanced=true;SendMessageW(hookChecks[0],BM_SETCHECK,BST_CHECKED,0);}typeTap.clear();registerKeys();changedSetting();return true;
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

