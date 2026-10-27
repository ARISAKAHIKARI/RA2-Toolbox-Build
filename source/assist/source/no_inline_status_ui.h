// WM_CREATE-only UI adapter. Subclasses toolbox STATIC controls, not game code.
// Keeps legacy setting IDs/IPC intact while making this branch's status honest.
{
 struct NoInlineStatusView {
  static LRESULT CALLBACK proc(HWND w,UINT m,WPARAM a,LPARAM b,UINT_PTR id,DWORD_PTR kind){
   if(m==WM_NCDESTROY){RemoveWindowSubclass(w,proc,id);return DefSubclassProc(w,m,a,b);}
   if(m!=WM_SETTEXT)return DefSubclassProc(w,m,a,b);
   std::wstring text;
   // Never mask real residual-patch/conflict diagnostics from an old runtime.
   if(snap.hookInstalled||snap.hookConflicts)return DefSubclassProc(w,m,a,b);
   if(kind==9192){
    text=L"原生命令只读观察：";
    if(!extra.nativeMegaGateway)text+=L"关闭";
    else if(!shared||!commandFresh(GetTickCount()))text+=L"等待对局及连接";
    else if(shared->nativeMegaGatewayStatus==1){
     text+=L"候选 "+std::to_wstring(shared->nativeMegaGatewayAccepted)+
       L" · 未判定区间 "+std::to_wstring(shared->nativeMegaGatewaySkipped)+L"\r\n按帧采样，仅诊断，不接管自动任务";
    }else if(shared->nativeMegaGatewayStatus==-4)text+=L"当前仅支持 LAN / Internet";
    else if(shared->nativeMegaGatewayStatus<0)text+=L"采样失败，已重置观察基线";
    else text+=L"等待游戏就绪";
   }else if(kind==809){
    text=L"地图显示：";
    if(!extra.mapStrategy)text+=L"关闭";
    else if(extra.mapStrategy==3){
     text+=L"原生揭示 · ";
     if(!shared||!commandFresh(GetTickCount()))text+=L"等待对局";
     else if(autoRevealDone&&autoRevealDone==commandMatch())text+=L"已执行";
     else text+=L"等待执行";
     text+=L"（修改探索数据，联机有同步风险）";
    }else text+=L"此分支不可用（需要游戏代码补丁）";
   }else if(kind==866){
    text=L"飞机路径点：";
    if(!(extra.airWaypoints||extra.nativeAirWaypoints))text+=L"关闭";
    else if(extra.airWaypoints||extra.airPathKey||!airway::native())
     text+=!shared||!commandFresh(GetTickCount())?L"兼容事件模式 · 等待对局":L"兼容事件模式（无代码补丁，待实测）";
    else text+=L"原生改判定模式在此分支不可用";
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
    text+=L"此分支不可用（需要游戏代码补丁）";
   }
   return DefSubclassProc(w,m,a,reinterpret_cast<LPARAM>(text.c_str()));
  }
 };
 for(int id:{807,809,865,866,9098,9188,9190,9192,9194}){
  HWND h=GetDlgItem(ui,id);
  if(h&&SetWindowSubclass(h,NoInlineStatusView::proc,0x4E494F42,DWORD_PTR(id)))SetWindowTextW(h,L"");
 }
 for(int id:{476,860,9183,9187,9189,9193,9096}){
  HWND h=GetDlgItem(ui,id);if(h)EnableWindow(h,FALSE);
  for(auto&c:pageControls)if(c.hwnd==h)c.help=L"此分支禁止安装游戏代码补丁，此项不可用。旧配置保留，不代表已生效。";
 }
 for(int id:{808,809}){HWND h=GetDlgItem(ui,id);for(auto&c:pageControls)if(c.hwnd==h)
  c.help=L"透明、半透明和直接全图所需代码补丁被禁用。原生揭示不改指令，但会修改探索数据，仍有联机同步风险。";}
 for(int id:{9097,9098}){HWND h=GetDlgItem(ui,id);for(auto&c:pageControls)if(c.hwnd==h)
  c.help=L"科技加强修改类型数据，不是修改游戏指令。本分支未把它改成只读功能，也不保证联机同步。";}
 SetWindowTextW(GetDlgItem(ui,476),L"随处建造（此分支不可用）");
 SetWindowTextW(GetDlgItem(ui,860),L"跨玩家操控（此分支不可用）");
 SetWindowTextW(GetDlgItem(ui,9183),L"修改原生索敌（此分支不可用）");
 SetWindowTextW(GetDlgItem(ui,9187),L"保留事件所属方（此分支不可用）");
 SetWindowTextW(GetDlgItem(ui,9189),L"对手建筑原生变卖光标（此分支不可用）");
 SetWindowTextW(GetDlgItem(ui,9193),L"命令使用单位所属方（此分支不可用）");
}
