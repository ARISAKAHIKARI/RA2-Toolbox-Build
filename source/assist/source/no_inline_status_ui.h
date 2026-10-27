// WM_CREATE-only UI adapter. This subclasses TOOLBOX controls, never game code.
// Keep settings/hotkeys intact. Captions distinguish missing implementations,
// preserved event paths needing live tests, and deliberate data mutations.
{
 struct NoInlineStatusView {
  static LRESULT CALLBACK proc(HWND w,UINT m,WPARAM a,LPARAM b,UINT_PTR id,DWORD_PTR kind){
   if(m==WM_NCDESTROY){RemoveWindowSubclass(w,proc,id);return DefSubclassProc(w,m,a,b);}
   if(m!=WM_SETTEXT)return DefSubclassProc(w,m,a,b);
   std::wstring text;
   // Never mask a real diagnostic with a reassuring unavailable/disabled label.
   if(snap.hookInstalled||snap.hookConflicts){
    text=L"检测到补丁残留或冲突：严格版不会写回代码，请重启游戏。 ";
    if(b)text+=reinterpret_cast<const wchar_t*>(b);
    return DefSubclassProc(w,m,a,reinterpret_cast<LPARAM>(text.c_str()));
   }
   if(kind==9192){
    text=L"原生命令只读观察：";
    if(!extra.nativeMegaGateway)text+=L"关闭";
    else if(!shared||!commandFresh(GetTickCount()))text+=L"等待对局及连接";
    else if(shared->nativeMegaGatewayStatus==1){
     text+=L"候选 "+std::to_wstring(shared->nativeMegaGatewayAccepted)+
       L" · 未判定区间 "+std::to_wstring(shared->nativeMegaGatewaySkipped)+L"\r\n按帧采样，不接管自动任务；手动接管替代未实现";
    }else if(shared->nativeMegaGatewayStatus==-4)text+=L"当前仅支持 LAN / Internet；仅诊断";
    else if(shared->nativeMegaGatewayStatus<0)text+=L"采样失败，已重置观察基线";
    else text+=L"等待游戏就绪；仅诊断";
   }else if(kind==809){
    text=L"地图显示：";
    if(!extra.mapStrategy)text+=L"关闭";
    else if(extra.mapStrategy==3){
     text+=L"原生揭示 · ";
     if(!shared||!commandFresh(GetTickCount()))text+=L"等待对局";
     else if(autoRevealDone&&autoRevealDone==commandMatch())text+=L"已执行";
     else text+=L"等待执行";
     text+=L"（修改探索数据，联机有同步风险）";
    }else text+=L"此分支不可用（无代码补丁替代未实现）";
   }else if(kind==866){
    text=L"飞机路径点：";
    if(!(extra.airWaypoints||extra.nativeAirWaypoints))text+=L"关闭";
    else if(extra.airWaypoints||extra.airPathKey||!airway::native())
     text+=!shared||!commandFresh(GetTickCount())?L"兼容事件模式 · 等待对局":L"兼容事件模式（待实测，接管验证未完成）";
    else text+=L"原生改判定模式在此分支不可用（无补丁替代未实现）";
   }else if(kind==9098){
    text=L"科技加强：";
    if(shared&&commandFresh(GetTickCount())&&shared->techEnhanceStatus<0)
     text+=L"类型数据修改或恢复失败，请查看日志";
    else if(!extra.techEnhance)
     text+=shared&&shared->techEnhanceStatus==1?L"等待恢复类型数据":L"关闭";
    else if(!shared||!commandFresh(GetTickCount()))text+=L"等待对局";
    else if(shared->techEnhanceStatus==1)
     text+=L"已修改类型数据（"+std::to_wstring(shared->techEnhanceCount)+L" 项）· 联机有同步风险";
    else text+=L"等待修改类型数据 · 联机有同步风险";
   }else{
    switch(kind){
     case 807:text=L"随处建造：";break;
     case 865:text=L"跨玩家操控：";break;
     case 9188:text=L"事件所属方保留：";break;
     case 9190:text=L"原生变卖光标：";break;
     case 9194:text=L"控制命令所属方：";break;
     default:return DefSubclassProc(w,m,a,b);
    }
    text+=L"此分支不可用（无代码补丁替代未实现）";
   }
   return DefSubclassProc(w,m,a,reinterpret_cast<LPARAM>(text.c_str()));
  }
 };
 for(int id:{807,809,865,866,9098,9188,9190,9192,9194}){
  HWND h=GetDlgItem(ui,id);
  if(h&&SetWindowSubclass(h,NoInlineStatusView::proc,0x4E494F42,DWORD_PTR(id)))SetWindowTextW(h,L"");
 }
 // These are wholly patch-dependent switches, not mixed-mode selectors.
 for(int id:{476,860,9183,9187,9189,9193,9096}){
  HWND h=GetDlgItem(ui,id);if(h)EnableWindow(h,FALSE);
  for(auto&c:pageControls)if(c.hwnd==h)c.help=L"未实现无代码补丁替代，此分支不可用。严格版也不会恢复旧补丁，请重启游戏；旧配置及快捷键保留。";
 }
 for(int id:{808,809}){HWND h=GetDlgItem(ui,id);for(auto&c:pageControls)if(c.hwnd==h)
  c.help=L"透明、半透明和直接全图：无补丁替代未实现。原生揭示保留，但修改探索数据，联机仍有同步风险。";}
 for(int id:{803,866}){HWND h=GetDlgItem(ui,id);for(auto&c:pageControls)if(c.hwnd==h)
  c.help=L"兼容事件路线保留、待实测；原生改判定模式的无补丁替代未实现。不能把模式开关当作验证结果。";}
 for(int id:{9097,9098}){HWND h=GetDlgItem(ui,id);for(auto&c:pageControls)if(c.hwnd==h)
  c.help=L"科技加强修改类型数据，不修改游戏指令；不是只读功能，联机验证未完成。数据恢复与代码补丁恢复不是同一件事。";}
 struct PendingControl {int id;const wchar_t* caption;};
 const PendingControl pending[]={
  {100,L"自动维修（待实测）"},{459,L"智能卸载（待实测）"},
  {455,L"载员自动逃生（待实测）"},{458,L"高级移动攻击（待实测）"},
  {477,L"磁电轮换攻击（待实测）"},{9042,L"磁电全局无限吸（待实测）"},
  {702,L"高级进驻（待实测）"},{705,L"飞机分配攻击 · 1（待实测）"},
  {719,L"飞碟主武器攻击（待实测）"},{720,L"出售被吸取目标（待实测）"},
  {721,L"出售战车（待实测）"},{9050,L"队形排列（待实测）"},
  {799,L"生产预定（待实测）"},{470,L"矿车自动卸矿（待实测）"},
  {804,L"防御建筑优先拦截（待实测）"},
  {420,L"科技解锁（无补丁替代待审计）"}
 };
 for(const auto&entry:pending){
  HWND h=GetDlgItem(ui,entry.id);if(!h)continue;
  SetWindowTextW(h,entry.caption);
  for(auto&c:pageControls)if(c.hwnd==h)c.help=L"本分支验证未完成；不能视为已迁移或联机稳定。原生手动命令接管的等价替代尚未实现。 "+c.help;
 }
 for(int id:{700,701}){HWND h=GetDlgItem(ui,id);for(auto&c:pageControls)if(c.hwnd==h)
  c.help=L"自动装车：原生 Enter 事件路线保留，待实测；满载、人工接管及联机同步验证未完成。 "+c.help;}
 SetWindowTextW(GetDlgItem(ui,900),L"快捷操作 · 严格无代码写入实验版");
 SetWindowTextW(GetDlgItem(ui,902),L"自动化 · 未完成实机与联机验证");
 SetWindowTextW(GetDlgItem(ui,454),L"停止所选自动任务（原生手动接管替代未实现）");
 SetWindowTextW(GetDlgItem(ui,476),L"随处建造（未实现）");
 SetWindowTextW(GetDlgItem(ui,860),L"跨玩家操控（未实现）");
 SetWindowTextW(GetDlgItem(ui,9183),L"修改原生索敌（未实现）");
 SetWindowTextW(GetDlgItem(ui,9187),L"保留事件所属方（未实现）");
 SetWindowTextW(GetDlgItem(ui,9189),L"对手建筑原生变卖光标（未实现）");
 SetWindowTextW(GetDlgItem(ui,9193),L"命令使用单位所属方（未实现）");
}
