// Each colliding action enters the existing request/selection channel in turn.
static void executeHotkey(WPARAM wp,bool fanout){
 if(wp>=1720&&wp<=1722&&extra.viewZoomEnabled){
  extra.viewZoomPercent=wp==1722?100:viewzoom::step(extra.viewZoomPercent,wp==1720?-1:1);
  {ControlUpdate guard;SendDlgItemMessageW(ui,9091,CB_SETCURSEL,viewzoom::index(extra.viewZoomPercent),0);}
  save();syncAdditional();notify(L"视角缩放请求："+std::to_wstring(extra.viewZoomPercent)+L"%");return;
 }
 if(wp==1702){watchhost::pin();return;}
 if(wp>=1710&&wp<=1718){send(wp==1710?Cheer:Taunt,int(wp-1710));return;}
 if(wp==1701){selectNextIdleAircraft();return;}
 if(wp==1700){bool global=(extra.variantKey&0x8000)||variantTap.press(1700,GetTickCount(),snap.match,doubleMs);sendSelection(-2,global);return;}
 if(wp>=1100&&wp<1100+typeBindings.size()){auto&binding=typeBindings[wp-1100];uint8_t mask[MaxTypes]{};
  if(!typekeys::groupMask(typeBindings,binding.key,snap.types,snap.typeCount,mask,MaxTypes)){notify(L"本局没有此组合中的单位类型。");return;}
  bool global=(binding.key&0x8000)||typeTap.press(int(wp),GetTickCount(),snap.match,doubleMs);
  sendSelection(-3,global,mask);return;}
 if(!fanout){typeTap.clear();variantTap.clear();}if(wp==1696){ejectMode=!ejectMode;hookEject=ejectMode;if(ejectMode){hookEject=true;SendMessageW(hookChecks[2],BM_SETCHECK,BST_CHECKED,0);}SendMessageW(ejectCheck,BM_SETCHECK,ejectMode?BST_CHECKED:BST_UNCHECKED,0);save();notify(ejectMode?L"载员自动逃生已开启。":L"载员自动逃生已关闭。");return;}if(wp==1695||wp==1697)return;if(wp==1698){send(SmartUnload);return;}if(wp==1699){csMode=!csMode;if(csMode){hookAutoCS=true;SendMessageW(hookChecks[3],BM_SETCHECK,BST_CHECKED,0);}SendMessageW(csCheck,BM_SETCHECK,csMode?BST_CHECKED:BST_UNCHECKED,0);save();notify(csMode?L"自动移动攻击（CS）已开启，对允许的单位生效。":L"自动移动攻击（CS）已关闭。");return;}
 if(wp>=1000&&wp<1012){int i=int(wp-1000);if(i==0){SendMessageW(repairCheck,BM_SETCHECK,SendMessageW(repairCheck,BM_GETCHECK,0,0)==BST_CHECKED?BST_UNCHECKED:BST_CHECKED,0);save();notify(L"自动维修开关已保存。");}else send(actions[i]);}
 else if(wp>=1020&&wp<1020+swBindings.size()){auto&b=swBindings[wp-1020];bool found=false;for(int i=0;i<snap.swCount;++i)if(b.id==snap.supers[i].id){send(SelectSuper,snap.supers[i].index);found=true;break;}if(!found)notify(L"本局尚未拥有此超级武器。");}return;
}
static void serviceHotkeyBatch(){
 if(formationInputOwns()||!enabled||!foregroundGame()||snap.status!=1||snap.renderStatus!=1||snap.chat){hotkeyPending.clear();hotkeyRegistry.cancel();return;}
 if(!commandFresh(GetTickCount())||!shared)return;
 // Bare modifier shortcuts cannot be registered with RegisterHotKey. Reuse
 // the existing EXE input sampler; autorepeat never advances its press serial.
 static uint32_t seen[256]{};uint8_t keys[256]{};uint32_t serial[256]{};gameinput::keySnapshot(hook,keys,serial);
 for(const auto&g:hotkeyRegistry.groups)if((g.key==16||g.key==17||g.key==18)&&serial[g.key]!=seen[g.key]){seen[g.key]=serial[g.key];if(keys[g.key]){hotkeyroute::Group out;if(hotkeyRegistry.press(g.id,GetTickCount(),commandMatch(),out))hotkeyPending.add(out,GetTickCount(),commandMatch());}}
 hotkeyroute::Group delayed;while(hotkeyRegistry.expired(GetTickCount(),commandMatch(),delayed))hotkeyPending.add(delayed,GetTickCount(),commandMatch());
 for(unsigned n=0;n<hotkeyroute::Pending::Capacity;++n){
  if(hasSelection||commandQueue.size||shared->request!=shared->ack||shared->selectRequest!=shared->selectAck)return;
  hotkeyroute::Action action;if(!hotkeyPending.pop(GetTickCount(),commandMatch(),action))return;
  executeHotkey(action.id,extra.allHotkeys);
 }
}
static void handleHotkey(WPARAM id,LPARAM event){
 if(!enabled||!foregroundGame()||snap.status!=1||snap.renderStatus!=1||snap.chat){typeTap.clear();variantTap.clear();hotkeyPending.clear();return;}
 const auto*g=hotkeyRegistry.find(int(id));if(!g)return;
 WORD key=MAKEWORD(HIWORD(event),((LOWORD(event)&MOD_SHIFT)?HOTKEYF_SHIFT:0)|((LOWORD(event)&MOD_CONTROL)?HOTKEYF_CONTROL:0)|((LOWORD(event)&MOD_ALT)?HOTKEYF_ALT:0));
 if(hotkeyroute::canonical(key)!=g->key)return; // queued notification from an old binding
 if(formationHotkey(key)){hotkeyPending.clear();return;}
 hotkeyroute::Group selected;if(!hotkeyRegistry.press(int(id),GetTickCount(),commandMatch(),selected))return;
 if(!extra.allHotkeys&&selected.actions.size()>1)selected.actions.resize(1);
 if(!hotkeyPending.add(selected,GetTickCount(),commandMatch()))notify(L"快捷键请求队列已满，本次组合未提交。");
 serviceHotkeyBatch();
}

