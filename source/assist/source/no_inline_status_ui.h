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
       L" · 未判定区间 "+std::to_wstring(shared->nativeMegaGatewaySkipped)+L"\r\n仅诊断，不接管自动任务";
    }else if(shared->nativeMegaGatewayStatus==-4)text+=L"当前仅支持 LAN / Internet";
    else if(shared->nativeMegaGatewayStatus<0)text+=L"采样失败，已重置观察基线";
    else text+=L"等待游戏就绪";
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
 for(int id:{807,865,9188,9190,9192,9194}){
  HWND h=GetDlgItem(ui,id);
  if(h&&SetWindowSubclass(h,NoInlineStatusView::proc,0x4E494F42,DWORD_PTR(id)))SetWindowTextW(h,L"");
 }
 for(int id:{476,860,9183,9187,9189,9193,9096}){
  HWND h=GetDlgItem(ui,id);if(h)EnableWindow(h,FALSE);
  for(auto&c:pageControls)if(c.hwnd==h)c.help=L"此分支禁止安装游戏代码补丁，此项不可用。旧配置保留，不代表已生效。";
 }
 SetWindowTextW(GetDlgItem(ui,476),L"随处建造（此分支不可用）");
 SetWindowTextW(GetDlgItem(ui,860),L"跨玩家操控（此分支不可用）");
 SetWindowTextW(GetDlgItem(ui,9183),L"修改原生索敌（此分支不可用）");
 SetWindowTextW(GetDlgItem(ui,9187),L"保留事件所属方（此分支不可用）");
 SetWindowTextW(GetDlgItem(ui,9189),L"对手建筑原生变卖光标（此分支不可用）");
 SetWindowTextW(GetDlgItem(ui,9193),L"命令使用单位所属方（此分支不可用）");
}
