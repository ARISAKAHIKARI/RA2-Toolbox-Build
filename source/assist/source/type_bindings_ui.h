// All editable unit bindings are based on the persistent catalogue, not the
// live match snapshot. Runtime selection still resolves IDs against the match.
static int selectedTypeIndex(){
 int pos=int(SendMessageW(typeList,CB_GETCURSEL,0,0));if(pos<0)return -1;
 int index=int(SendMessageW(typeList,CB_GETITEMDATA,pos,0));
 return index>=0&&index<int(savedTypes.types.size())&&savedTypes.types[index].kind!=7?index:-1;
}
static WORD typeBinding(const char* id){for(auto&b:typeBindings)if(b.id==id)return b.key;return 0;}
static WORD comparableKey(WORD k){return MAKEWORD(LOBYTE(k),HIBYTE(k)&(HOTKEYF_CONTROL|HOTKEYF_ALT|HOTKEYF_SHIFT));}
static bool keyConflict(WORD key,const std::string& ignore,bool toggle){
 if(extra.allHotkeys||!LOBYTE(key))return false;key=comparableKey(key);
 for(auto k:hotkeys)if(comparableKey(k)==key)return true;
 for(auto&b:swBindings)if(comparableKey(b.key)==key)return true;
 if(ignore.empty())for(auto&b:typeBindings)if(comparableKey(b.key)==key)return true;
 if(comparableKey(idleAircraftKey)==key)return true;
 if(extra.watchEnabled&&comparableKey(WORD(extra.watchKey))==key)return true;
 for(auto social:socialKeys)if(comparableKey(social)==key)return true;
 if(comparableKey(WORD(extra.variantKey))==key)return true;
 return (!toggle&&comparableKey(csKey)==key)||comparableKey(advancedKey)==key||comparableKey(unloadKey)==key||comparableKey(ejectKey)==key||comparableKey(csHoldKey)==key||comparableKey(magnetronKey)==key;
}
static void syncTypeConfig(){
 if(!shared)return;
 if(memcmp(&shared->featureConfig,&featureTypes,sizeof featureTypes)){InterlockedIncrement(&shared->featureConfigSeq);MemoryBarrier();memcpy(&shared->featureConfig,&featureTypes,sizeof featureTypes);MemoryBarrier();InterlockedIncrement(&shared->featureConfigSeq);}
 if(memcmp(&shared->oreConfig,&oreTypes,sizeof oreTypes)){InterlockedIncrement(&shared->oreConfigSeq);MemoryBarrier();memcpy(&shared->oreConfig,&oreTypes,sizeof oreTypes);MemoryBarrier();InterlockedIncrement(&shared->oreConfigSeq);}
 shared->flags=flags();shared->advancedRangeCells=advancedRangeCells;shared->advancedKey=advancedKey;shared->magnetronKey=magnetronKey;shared->csHoldKey=csHoldKey;shared->pathTimeout=pathTimeout;shared->veteranPercent=veteranPercent;shared->elitePercent=elitePercent;uint8_t mask[MaxTypes]{};
 static uint32_t syncedRevision,syncedMatch;static int syncedCount=-1;
 if(syncedRevision==settingsRevision&&syncedMatch==snap.match&&syncedCount==snap.typeCount&&shared->csMatch==snap.match)return;
 syncedRevision=settingsRevision;syncedMatch=snap.match;syncedCount=snap.typeCount;
 for(int i=0;i<snap.typeCount&&i<MaxTypes;++i)mask[i]=csUnits.count(snap.types[i].id)!=0;
 if(shared->csMatch==snap.match&&!memcmp(shared->csAllowed,mask,sizeof mask))return;
 InterlockedIncrement(&shared->csSeq);MemoryBarrier();shared->csMatch=snap.match;memcpy(shared->csAllowed,mask,sizeof mask);MemoryBarrier();InterlockedIncrement(&shared->csSeq);
}
static void typeBindingFields(){
 ControlUpdate guard;
 int i=selectedTypeIndex();const char*id=i>=0?savedTypes.types[i].id:"";
 SendMessageW(typeKey,HKM_SETHOTKEY,typeBinding(id),0);SendMessageW(typeCS,BM_SETCHECK,csUnits.count(id)?BST_CHECKED:BST_UNCHECKED,0);
 EnableWindow(typeCS,i>=0&&(savedTypes.types[i].kind==16||savedTypes.types[i].kind==40));
}
static void fillTypeBindings(bool force){
 if(SendMessageW(typeList,CB_GETDROPPEDSTATE,0,0))return;
 static uint32_t previousRevision=~0u;if(!force&&previousRevision==catalogueRevision)return;previousRevision=catalogueRevision;
 static std::wstring previous;std::wstring hash=std::to_wstring(catalogueRevision);
 for(const auto&t:savedTypes.types){if(t.kind==7)continue;hash+=wide(t.id)+keyText(typeBinding(t.id))+(csUnits.count(t.id)?L"1":L"0");}
 if(!force&&hash==previous)return;previous=hash;
 int old=selectedTypeIndex();std::string id=old>=0?savedTypes.types[old].id:"";
 SendMessageW(typeList,CB_RESETCONTENT,0,0);int chosen=0;
 for(size_t i=0;i<savedTypes.types.size();++i){auto&t=savedTypes.types[i];if(t.kind==7)continue;
  std::wstring line=std::wstring((const wchar_t*)t.name)+L" ["+wide(t.id)+L"] · "+keyText(typeBinding(t.id))+(csUnits.count(t.id)?L" · CS":L"");
  int members=typekeys::groupSize(typeBindings,typeBinding(t.id));if(members>1)line+=L" · 组合 "+std::to_wstring(members)+L" 种";
  if(t.kind<0)line+=L" · 待识别";
  int pos=int(SendMessageW(typeList,CB_ADDSTRING,0,(LPARAM)line.c_str()));
  if(pos>=0){SendMessageW(typeList,CB_SETITEMDATA,pos,i);if(id==t.id)chosen=pos;}
 }
 if(SendMessageW(typeList,CB_GETCOUNT,0,0)>0)SendMessageW(typeList,CB_SETCURSEL,chosen,0);
 typeBindingFields();
}
static void bindType(bool remove){
 int i=selectedTypeIndex();if(i<0){notify(L"请先选中列表中的单位；离线也可以绑定。");return;}
 std::string id=savedTypes.types[i].id;WORD key=remove?0:WORD(SendMessageW(typeKey,HKM_GETHOTKEY,0,0));
 if(keyConflict(key,id,false)){notify(L"此快捷键已经被其他功能使用，请换一个。");return;}
 auto it=std::find_if(typeBindings.begin(),typeBindings.end(),[&](const Binding&b){return b.id==id;});
 if(!LOBYTE(key)){if(it!=typeBindings.end())typeBindings.erase(it);}
 else if(it!=typeBindings.end())it->key=key;
 else if(typeBindings.size()<512)typeBindings.push_back({id,key});
 else {notify(L"单位快捷键已达上限。");return;}
 typeTap.clear();bool persisted=save();registerKeys();fillTypeBindings(true);
 notify(!persisted?L"保存失败：请检查 Assist.ini 所在目录的写入权限。":
   LOBYTE(key)?L"组合已保存：同快捷键合并选中，单按屏幕内、双按全图。":L"已解除此单位快捷键。");
}

static void featureTypeFields(){ControlUpdate guard;const auto&f=featureTypes.filters[featureTypeIndex];
 SendMessageW(GetDlgItem(ui,791),BM_SETCHECK,f.custom?BST_CHECKED:BST_UNCHECKED,0);
 SetWindowTextW(GetDlgItem(ui,792),featureTypeText[featureTypeIndex].c_str());
}
