static void loadAdditionalSettings(){
 for(int i=0;i<featuretypes::Count;++i){auto&f=featureTypes.filters[i];std::wstring custom=std::wstring(featuretypes::key(i))+L"Custom";wchar_t text[13000]{};
  GetPrivateProfileStringW(L"FeatureTypes",featuretypes::key(i),featuretypes::initial(i),text,13000,path(L"Assist.ini").c_str());featureTypeText[i]=text;
  oreconfig::parse(featureTypeText[i],f.ids,f.count);f.custom=value(L"FeatureTypes",custom.c_str(),0)!=0;
 }

 extra.experimentalReveal=value(L"Experimental",L"RevealMap",0)!=0;
 extra.enemyMarkers=value(L"Display",L"EnemyMarkers",0)!=0;
 extra.allHotkeys=value(L"Hotkeys",L"ExecuteAllMatches",0)!=0;
 // Missing per-feature values inherit the old category setting exactly once.
 for(int i=0;i<pointerinput::MouseCount;++i){
  const wchar_t*legacy=i<pointerinput::MouseAttack?L"EnhancedButton":i==pointerinput::MouseAttack?L"AttackMoveButton":L"SellButton";
  int fallback=std::clamp(value(L"Mouse",legacy,pointerinput::mouseDefault(i)),0,4);
  extra.mouseButtons[i]=std::clamp(value(L"Mouse",pointerinput::mouseSetting(i),fallback),0,4);
 }

 extra.loadMode=std::clamp(value(L"AutoLoad",L"Logic",0),0,1);extra.loadRadius=std::clamp(value(L"AutoLoad",L"RangeCells",30),1,512);
 extra.enter=value(L"Cooperative",L"AdvancedEnter",0)!=0;extra.air=value(L"Cooperative",L"AircraftSpread",0)!=0;
 extra.enterKey=value(L"Cooperative",L"EnterHoldKey",18)&65535;extra.airKey1=value(L"Cooperative",L"AirHoldKey1",17)&65535;extra.airKey2=value(L"Cooperative",L"AirHoldKey2",0)&65535;

 extra.diskKey=value(L"SpecialClick",L"DiskHoldKey",17)&65535;extra.sellKey=value(L"SpecialClick",L"SellHoldKey",17)&65535;
 extra.disk=value(L"SpecialClick",L"DiskPrimary",0)!=0;extra.sellDrained=value(L"SpecialClick",L"SellDrained",0)!=0;extra.sellVehicles=value(L"SpecialClick",L"SellVehicles",0)!=0;extra.sellEnemy=false;
 extra.frozen=value(L"Display",L"Frozen",1)!=0;extra.variantKey=value(L"TypeSelection",L"VariantKey",'T'|(2<<8))&65535;
 extra.defaults=cooperative::bounded({value(L"AircraftQuota",L"First",1),value(L"AircraftQuota",L"Second",2),value(L"AircraftQuota",L"Maximum",0)});
 wchar_t entries[32768]{};GetPrivateProfileSectionW(L"AircraftQuotaOverrides",entries,32768,path(L"Assist.ini").c_str());
 for(wchar_t*p=entries;*p;p+=wcslen(p)+1){std::wstring line=p;size_t e=line.find(L'=');if(e==std::wstring::npos)continue;std::string id;for(size_t j=0;j<e;++j)if(line[j]>32&&line[j]<127)id+=char(line[j]);if(!typecatalog::validId(id)||targetQuotas.size()>=512)continue;
  int a,b,c;if(swscanf(line.c_str()+e+1,L"%d,%d,%d",&a,&b,&c)!=3)continue;targetQuotas[id]=cooperative::bounded({a,b,c});
 }
}
static bool saveAdditionalSettings(){
 for(int i=0;i<featuretypes::Count;++i){std::wstring custom=std::wstring(featuretypes::key(i))+L"Custom";
  write(L"FeatureTypes",custom.c_str(),featureTypes.filters[i].custom);
  saveWriteOK=(WritePrivateProfileStringW(L"FeatureTypes",featuretypes::key(i),featureTypeText[i].c_str(),path(L"Assist.ini").c_str())!=FALSE)&&saveWriteOK;
 }

 write(L"Experimental",L"RevealMap",extra.experimentalReveal);
 write(L"Display",L"EnemyMarkers",extra.enemyMarkers);
 write(L"Hotkeys",L"ExecuteAllMatches",extra.allHotkeys);
 for(int i=0;i<pointerinput::MouseCount;++i)write(L"Mouse",pointerinput::mouseSetting(i),extra.mouseButtons[i]);

 write(L"AutoLoad",L"Logic",extra.loadMode);write(L"AutoLoad",L"RangeCells",extra.loadRadius);
 write(L"Cooperative",L"AdvancedEnter",extra.enter);write(L"Cooperative",L"AircraftSpread",extra.air);write(L"Cooperative",L"EnterHoldKey",extra.enterKey);write(L"Cooperative",L"AirHoldKey1",extra.airKey1);write(L"Cooperative",L"AirHoldKey2",extra.airKey2);
 write(L"SpecialClick",L"DiskHoldKey",extra.diskKey);write(L"SpecialClick",L"SellHoldKey",extra.sellKey);
 write(L"SpecialClick",L"DiskPrimary",extra.disk);write(L"SpecialClick",L"SellDrained",extra.sellDrained);write(L"SpecialClick",L"SellVehicles",extra.sellVehicles);write(L"SpecialClick",L"SellEnemyBuildings",extra.sellEnemy);
 write(L"Display",L"Frozen",extra.frozen);write(L"TypeSelection",L"VariantKey",extra.variantKey);
 write(L"AircraftQuota",L"First",extra.defaults.first);write(L"AircraftQuota",L"Second",extra.defaults.second);write(L"AircraftQuota",L"Maximum",extra.defaults.maximum);
 std::wstring block;for(const auto&entry:targetQuotas){const auto&q=entry.second;block+=wide(entry.first.c_str())+L"="+std::to_wstring(q.first)+L","+std::to_wstring(q.second)+L","+std::to_wstring(q.maximum);block.push_back(0);}block.push_back(0);if(block.size()==1)block.push_back(0);
 return WritePrivateProfileSectionW(L"AircraftQuotaOverrides",block.c_str(),path(L"Assist.ini").c_str())!=FALSE;
}
static std::string quotaID(){wchar_t text[64]{};GetWindowTextW(extraControls[QuotaID],text,64);std::string id;for(wchar_t c:std::wstring(text)){if(c<33||c>126)return {};id+=char(c);}return typecatalog::validId(id)?id:std::string{};}
static void readTargetQuota(){ControlUpdate guard;auto id=quotaID();if(id.empty()){notify(L"请输入有效的游戏对象 ID。");return;}auto found=targetQuotas.find(id);auto q=found==targetQuotas.end()?extra.defaults:found->second;
 SendMessageW(extraControls[QuotaOverride],BM_SETCHECK,found==targetQuotas.end()?BST_UNCHECKED:BST_CHECKED,0);
 const int controls[]={QuotaFirst,QuotaSecond,QuotaMaximum},values[]={q.first,q.second,q.maximum};for(int i=0;i<3;++i)SetWindowTextW(extraControls[controls[i]],std::to_wstring(values[i]).c_str());
}
static bool extraNumber(int control,int minimum,int maximum,int&out){BOOL ok;int n=GetDlgItemInt(ui,700+control,&ok,FALSE);if(!ok||n<minimum||n>maximum)return false;out=n;return true;}
static bool readAdditionalControls(){
 AdditionalSettings next=extra;next.loadMode=SendMessageW(extraControls[LoadLogic],CB_GETCURSEL,0,0)==1?1:0;
 if(!extraNumber(LoadRadius,1,512,next.loadRadius)||!extraNumber(DefaultFirst,0,512,next.defaults.first)||!extraNumber(DefaultSecond,0,512,next.defaults.second)||!extraNumber(DefaultMaximum,0,512,next.defaults.maximum)){notify(L"请输入有效的范围（1–512）和受击数（0–512）。");return false;}
 auto binding=[](int preset,int custom){return presetKey(int(SendMessageW(extraControls[preset],CB_GETCURSEL,0,0)),uint32_t(SendMessageW(extraControls[custom],HKM_GETHOTKEY,0,0)));};
 next.enterKey=binding(EnterPreset,EnterCustom);next.airKey1=binding(AirPreset1,AirCustom1);next.airKey2=binding(AirPreset2,AirCustom2);
 if(!extra.allHotkeys&&next.airKey1&&next.airKey1==next.airKey2){notify(L"飞机攻击的两个按键需要不同。");return false;}
 next.enter=SendMessageW(extraControls[EnterEnabled],BM_GETCHECK,0,0)==BST_CHECKED;next.air=SendMessageW(extraControls[AirEnabled],BM_GETCHECK,0,0)==BST_CHECKED;
 extra=next;return true;
}
static void fillExtraTypes(bool force=false){
 static uint32_t revision=~0u;if(!extraControls[QuotaType]||(!force&&revision==catalogueRevision))return;revision=catalogueRevision;
 int selected=int(SendMessageW(extraControls[QuotaType],CB_GETCURSEL,0,0));std::string keep=selected>=0&&selected<int(quotaTypeIDs.size())?quotaTypeIDs[selected]:"";
 SendMessageW(extraControls[QuotaType],CB_RESETCONTENT,0,0);quotaTypeIDs.clear();int restore=-1;
 for(const auto&t:savedTypes.types){std::string id=t.id;if(!typecatalog::validId(id))continue;std::wstring label=(const wchar_t*)t.name;label+=L" ["+wide(id.c_str())+L"]";int row=int(SendMessageW(extraControls[QuotaType],CB_ADDSTRING,0,(LPARAM)label.c_str()));if(row<0)continue;quotaTypeIDs.push_back(id);if(id==keep)restore=row;}
 if(restore>=0)SendMessageW(extraControls[QuotaType],CB_SETCURSEL,restore,0);
}
static uint32_t additionalFlags(){uint32_t f=extra.frozen?Frozen:0;if(extra.experimentalReveal)f|=ExperimentalReveal;if(extra.enter)f|=AdvancedEnter;if(extra.air)f|=AircraftSpread;if(extra.disk)f|=DiskPrimary;if(extra.sellDrained)f|=SellDrained;if(extra.sellVehicles)f|=SellVehicles;if(extra.sellEnemy)f|=SellEnemy;return f;}
static void syncAdditional(){if(!shared)return;shared->loadMode=extra.loadMode;shared->loadRadius=extra.loadRadius;shared->enterKey=extra.enterKey;shared->airKey1=extra.airKey1;shared->airKey2=extra.airKey2;
 if(externalSampler.flashCount&&shared->productionRequest==shared->productionAck&&commandFresh(GetTickCount())){shared->productionCount=externalSampler.flashCount;memcpy(shared->productionTypes,externalSampler.flashes,size_t(externalSampler.flashCount)*sizeof(int));shared->productionMatch=snap.match;shared->productionTick=GetTickCount();MemoryBarrier();InterlockedIncrement((volatile LONG*)&shared->productionRequest);externalSampler.flashCount=0;}
}
static void serviceCooperative(){
 if(!shared||!commandFresh(GetTickCount()))return;DWORD now=GetTickCount();
 uint32_t receipt=shared->cooperativeAck;
 if(receipt!=cooperativeReceipt){
  MemoryBarrier();cooperativeReceipt=receipt;
  for(size_t i=0;i<submittedCooperative.size()&&i<cooperative::MaxSources;++i)
   if(shared->cooperativeAcceptedBits[i/32]&(uint32_t(1)<<(i%32)))acceptedCooperative.push_back(submittedCooperative[i]);
  retryCooperative=shared->cooperativeResult==-7&&
   cooperative::retryRemaining(cooperativePending,submittedCooperative.data(),int(submittedCooperative.size()),shared->cooperativeProcessed)&&
   freshness::intentAlive(cooperativePending.tick,now);
  submittedCooperative.clear();
  if(!retryCooperative)notify(L"协同指令本批已提交 "+std::to_wstring(shared->cooperativeAccepted)+L" 个"+(shared->cooperativeRejected?L"；失效或已被手动接管 "+std::to_wstring(shared->cooperativeRejected)+L" 个。":L"。"));
 }
 if(shared->cooperativePlan!=shared->cooperativeAck)return;
 static cooperative::Request request;
 long seq=shared->cooperativeSeq;if(seq&1)return;
 MemoryBarrier();memcpy(&request,&shared->cooperativeRequest,sizeof request);MemoryBarrier();
 if(seq!=shared->cooperativeSeq)return;
 if(request.count>cooperative::MaxSources&&request.serial!=cooperativeDelivered){cooperativeDelivered=request.serial;retryCooperative=false;notify(L"选区超过 512 个，增强请求未提交。");return;}
 bool retry=retryCooperative&&request.serial==cooperativePending.serial&&request.match==cooperativePending.match;
 if(retry)request=cooperativePending;else {retryCooperative=false;acceptedCooperative.clear();}
 if(!cooperative::valid(request)||(!retry&&request.serial==cooperativeDelivered)||request.match!=snap.match||DWORD(now-request.tick)<8)return;
 if(!freshness::intentAlive(request.tick,now)){
  cooperativeDelivered=request.serial;retryCooperative=false;notify(L"协同请求已超时，请重新下令。");return;
 }
 // Guard reads bypass the task cache. Cached re-reading of frame/house would
 // merely compare a snapshot with itself and hide a game transition.
 // Sampling may span frames within the source-age budget. Requiring every
 // field in one frame would starve large selections; live checks remain final.
 externalReader.begin();uint32_t houseBefore=0,houseAfter=0;int frameBefore=-1,frameAfter=-1;
 DWORD acquired=GetTickCount();
 if(!externalReader.readFresh(0xA83D4C,&houseBefore,sizeof houseBefore)||houseBefore<0x10000||
    !externalReader.readFresh(0xA8ED84,&frameBefore,sizeof frameBefore)||frameBefore<0)return;
 auto input=external::cooperativeInput(external::SharedReadView{&externalReader},request,[](uint32_t type){char id[24]{};externalReader.read(type+0x24,id,23);auto found=targetQuotas.find(id);return found==targetQuotas.end()?extra.defaults:found->second;});
 if(retry&&request.mode!=cooperative::Enter)for(auto&t:input.targets)
  for(const auto&o:acceptedCooperative)if(o.target==t.id&&o.type==t.type)++t.prior;
 // Apply host filters before allocation so excluded sources never reserve capacity.
 const auto sourceFeature=request.mode==cooperative::Enter?featuretypes::EnterSource:featuretypes::Aircraft;
 auto permitted=[&](featuretypes::Feature f,uint32_t type){char id[24]{};return !featureTypes.filters[f].custom||(type>=0x10000&&externalReader.read(type+0x24,id,23)&&featuretypes::allowed(featureTypes,f,id));};
 for(size_t i=0;i<input.units.size();++i){bool sourceAllowed=permitted(sourceFeature,input.units[i].type);
  for(size_t j=0;j<input.targets.size();++j)if(!sourceAllowed||(request.mode==cooperative::Enter&&!permitted(featuretypes::EnterTarget,input.targets[j].type)))input.compatible[i*input.targets.size()+j]=0;
 }
 auto assignments=cooperative::allocate(input);
 if(!externalReader.readFresh(0xA8ED84,&frameAfter,sizeof frameAfter)||
    !externalReader.readFresh(0xA83D4C,&houseAfter,sizeof houseAfter)||houseAfter!=houseBefore||frameAfter<frameBefore||
    !freshness::valid(request.match,snap.match,acquired,frameBefore,GetTickCount(),frameAfter,freshness::Cooperative)||
    !freshness::intentAlive(request.tick,GetTickCount())||shared->cooperativeRequest.serial!=request.serial||shared->cooperativeSeq!=seq){wakeGame();return;}
 cooperativeDelivered=request.serial;retryCooperative=false;
 if(assignments.empty()){notify(L"屏幕内没有可分配的同类空位或有效敌方目标。");return;}
 cooperativePending=request;submittedCooperative=assignments;
 memcpy(shared->cooperativeOrders,assignments.data(),assignments.size()*sizeof(cooperative::Assignment));
 shared->cooperativeCount=int(assignments.size());shared->cooperativeMode=request.mode;shared->cooperativeSerial=request.serial;
 shared->cooperativeTick=acquired;shared->cooperativeFrame=frameBefore;shared->cooperativeMatch=request.match;
 MemoryBarrier();InterlockedIncrement((volatile LONG*)&shared->cooperativePlan);wakeGame();
 if(assignments.size()<size_t(request.count))notify(L"已分配 "+std::to_wstring(assignments.size())+L" 个；其余单位受容量、受击上限或目标兼容性限制。");
}
