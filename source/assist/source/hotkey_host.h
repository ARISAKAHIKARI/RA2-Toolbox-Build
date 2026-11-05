#include "middle_input_host.h"
// Selection-dependent attacks are resolved only at the live input edge.
static void ownGroundHotkey(hotkeyroute::Group&group,bool edge){
 const int id=1724+selectedmode::OwnGround;bool found=false;
 for(int action:group.actions)found|=action==id;if(!found)return;
 const bool exclusive=group.actions.size()==1;
 for(size_t n=0;n<group.actions.size();)if(group.actions[n]==id){group.actions.erase(group.actions.begin()+n);if(n<group.bindings.size())group.bindings.erase(group.bindings.begin()+n);}else ++n;
 if(edge&&exclusive)send(AttackOwnGround);
 else notify(edge?L"攻击脚下请使用独立快捷键，避免同时改变选区。":L"攻击脚下按键已过期，请重新按键。");
}
// Each colliding action enters the existing request/selection channel in turn.
static bool dispatchHotkeyGroup(hotkeyroute::Group group,bool inputEdge=true){
 if(destructownerhost::immediate(group,[]{
  if(destructOwnerKeyConflict(WORD(extra.destructSelectedOwnerKey))){notify(L"自毁必须使用独立快捷键，请更换组合。");return;}
  submitDestructSelectedOwner();
 }))return true;
 ownGroundHotkey(group,inputEdge);
 placementeventhost::immediate(group,[]{submitPlacementEvent();});
 return group.actions.empty()||hotkeyPending.add(group,GetTickCount(),commandMatch());
}
static void executeHotkey(WPARAM wp,bool fanout){
 if(wp==1724+selectedmode::OwnGround){notify(L"攻击脚下按键已失效，请重新按键。");return;}
 if(wp==destructownerhost::Hotkey){notify(L"自毁按键已失效，请在游戏中重新按键。");return;}
 if(wp==1734&&extra.sidebarEnabled){extra.sidebarMode=(extra.sidebarMode+1u)%3u;{ControlUpdate guard;SendDlgItemMessageW(ui,9404,CB_SETCURSEL,extra.sidebarMode,0);}changedSetting();syncAdditional();return;}
 if(wp==placementeventhost::Hotkey){notify(L"放置按键已失效，请在游戏中重新按键。");return;}
 if(wp>=1720&&wp<=1722&&extra.viewZoomEnabled){
  extra.viewZoomPercent=wp==1722?100:viewzoom::step(extra.viewZoomPercent,wp==1720?-1:1);
  {ControlUpdate guard;SendDlgItemMessageW(ui,9091,CB_SETCURSEL,viewzoom::index(extra.viewZoomPercent),0);}
  save();syncAdditional();notify(L"视角缩放请求："+(viewzoom::caption(extra.viewZoomPercent)?std::wstring(viewzoom::caption(extra.viewZoomPercent)):std::to_wstring(extra.viewZoomPercent)+L"%"));return;
 }
 if(wp==1723){send(StickyGuard);return;}
 if(wp>=1724&&wp<1724+selectedmode::Count){send(selectedmode::command(int(wp-1724)));return;}
 if(wp>=1730&&wp<=1732){watchhost::pin(extra.watchKeys[wp-1730]);return;}
 if(wp>=1710&&wp<=1718){send(wp==1710?Cheer:Taunt,int(wp-1710));return;}
 if(wp==1701){selectNextIdleAircraft();return;}
 if(wp==1700){bool legacy=keygesture::mode(extra.variantKey)==keygesture::Mode::Legacy;bool global=(extra.variantKey&0x8000)||(legacy&&variantTap.press(1700,GetTickCount(),snap.match,doubleMs));if(!legacy)variantTap.clear();sendSelection(-2,global);return;}
 if(wp>=1100&&wp<1100+typeBindings.size()){auto&binding=typeBindings[wp-1100];uint8_t mask[MaxTypes]{};
  if(!typekeys::groupMask(typeBindings,binding.key,snap.types,snap.typeCount,mask,MaxTypes)){notify(L"本局没有此组合中的单位类型。");return;}
  bool legacy=typekeys::implicitDouble(typeBindings,binding.key);bool global=(binding.key&0x8000)||(legacy&&typeTap.press(int(wp),GetTickCount(),snap.match,doubleMs));if(!legacy)typeTap.clear();
  sendSelection(-3,global,mask);return;}
 if(!fanout){typeTap.clear();variantTap.clear();}if(wp==1696){ejectMode=!ejectMode;hookEject=ejectMode;if(ejectMode){hookEject=true;SendMessageW(hookChecks[2],BM_SETCHECK,BST_CHECKED,0);}SendMessageW(ejectCheck,BM_SETCHECK,ejectMode?BST_CHECKED:BST_UNCHECKED,0);save();notify(ejectMode?L"载员自动逃生已开启。":L"载员自动逃生已关闭。");return;}if(wp==1695||wp==1697)return;if(wp==1698){send(SmartUnload);return;}if(wp==1699){if(!featureavailability::AutoCS){notify(L"自动 CS 暂不可用。");return;}csMode=!csMode;if(csMode){hookAutoCS=true;SendMessageW(hookChecks[3],BM_SETCHECK,BST_CHECKED,0);}SendMessageW(csCheck,BM_SETCHECK,csMode?BST_CHECKED:BST_UNCHECKED,0);save();notify(csMode?L"自动移动攻击（CS）已开启，对允许的单位生效。":L"自动移动攻击（CS）已关闭。");return;}
 if(wp>=1000&&wp<1012){int i=int(wp-1000);if(i==0){SendMessageW(repairCheck,BM_SETCHECK,SendMessageW(repairCheck,BM_GETCHECK,0,0)==BST_CHECKED?BST_UNCHECKED:BST_CHECKED,0);save();notify(L"自动维修开关已保存。");}else send(actions[i]);}
 else if(wp>=1020&&wp<1020+swBindings.size()){auto&b=swBindings[wp-1020];bool found=false;for(int i=0;i<snap.swCount;++i)if(b.id==snap.supers[i].id){send(SelectSuper,snap.supers[i].index);found=true;break;}if(!found)notify(L"本局尚未拥有此超级武器。");}return;
}
static void serviceHotkeyBatch(){
 const bool context=enabled&&foregroundGame()&&snap.status==1&&snap.renderStatus==1&&!snap.chat;
 const bool connected=shared&&commandFresh(GetTickCount());
 // The OS-hotkey fallback shares the native tracker's context cancellation.
 // No first tap may survive a hidden preview, focus loss, chat or a pause.
 if(!context||!connected||!shared->formationPreview.active)formationTap.clear();
 if(formationInputOwns()||runtimeMiddleOwnsNow()||!context){typeTap.clear();variantTap.clear();hotkeyPending.clear();hotkeyRegistry.cancel();return;}
 hotkeyPending.expire(GetTickCount(),commandMatch());
 if(!connected){formationTap.clear();typeTap.clear();variantTap.clear();hotkeyPending.clear();hotkeyRegistry.cancel();return;}
 uint8_t keys[256]{};for(unsigned i=0;i<256;++i)keys[i]=(GetAsyncKeyState(i)&0x8000)!=0;
 hotkeyRegistry.sample(keys,GetTickCount(),commandMatch(),[](hotkeyroute::Group out){if(!extra.allHotkeys)hotkeyroute::firstPerGesture(out);dispatchHotkeyGroup(std::move(out));});
 hotkeyroute::Group delayed;while(hotkeyRegistry.expired(GetTickCount(),commandMatch(),delayed)){if(!extra.allHotkeys)hotkeyroute::firstPerGesture(delayed);
  destructownerhost::immediate(delayed,[]{notify(L"自毁按键与延迟手势冲突，请配置独立按键后重试。");});
  placementeventhost::immediate(delayed,[]{notify(L"放置按键与延迟手势冲突，请配置独立按键后重试。");});dispatchHotkeyGroup(std::move(delayed),false);}
 for(unsigned n=0;n<hotkeyroute::Pending::Capacity;++n){
  if(hasSelection||commandQueue.size||shared->request!=shared->ack||shared->selectRequest!=shared->selectAck)return;
  hotkeyroute::Action action;if(!hotkeyPending.pop(GetTickCount(),commandMatch(),action))return;
  executeHotkey(action.id,extra.allHotkeys);
 }
}
static void handleHotkey(WPARAM id,LPARAM event){
 if(!enabled||!foregroundGame()||snap.status!=1||snap.renderStatus!=1||snap.chat){typeTap.clear();variantTap.clear();hotkeyPending.clear();hotkeyRegistry.cancel();return;}
 const auto*g=hotkeyRegistry.find(int(id));if(!g)return;
 // External display readiness is not a connected command session. In particular,
 // match=0 is not queue exhaustion. Never buffer actions across a reconnect.
 if(!shared||!commandMatch()||!commandFresh(GetTickCount())){
  hotkeyPending.clear();hotkeyRegistry.cancel();static uint32_t notifiedAt=0;uint32_t now=GetTickCount();
  if(!notifiedAt||uint32_t(now-notifiedAt)>=2500){notifiedAt=now;notify(L"快捷键暂不可用：游戏命令连接尚未就绪，恢复后请重新按键。");}return;
 }
 if(runtimeMiddleOwnsNow()){typeTap.clear();variantTap.clear();hotkeyPending.clear();hotkeyRegistry.cancel();return;}
 WORD key=MAKEWORD(HIWORD(event),((LOWORD(event)&MOD_SHIFT)?HOTKEYF_SHIFT:0)|((LOWORD(event)&MOD_CONTROL)?HOTKEYF_CONTROL:0)|((LOWORD(event)&MOD_ALT)?HOTKEYF_ALT:0));
 if(hotkeyroute::canonical(key)!=g->key)return; // queued notification from an old binding
 if(formationHotkey(key)){hotkeyPending.clear();return;}
 hotkeyroute::Group selected;if(!hotkeyRegistry.press(int(id),GetTickCount(),commandMatch(),selected))return;
 if(!extra.allHotkeys)hotkeyroute::firstPerGesture(selected);
 // WM_HOTKEY can itself wait in the Windows queue. An old place edge must
 // not resolve the current cursor after a long blocked UI callback.
 if(DWORD(GetTickCount()-DWORD(GetMessageTime()))>150){
  ownGroundHotkey(selected,false);
  destructownerhost::immediate(selected,[]{notify(L"自毁按键已过期，请重新按键。");});
  placementeventhost::immediate(selected,[]{notify(L"放置按键已过期，请重新按键。");});
 }
 if(!dispatchHotkeyGroup(std::move(selected)))notify(L"快捷键请求队列已满，本次组合未提交。");
 serviceHotkeyBatch();
}



