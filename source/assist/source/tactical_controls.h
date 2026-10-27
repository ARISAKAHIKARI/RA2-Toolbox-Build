// Keep the established layout unchanged; apply this branch's capabilities last.
#include "tactical_controls_layout.h"
SetWindowTextW(GetDlgItem(ui,9191),L"原生命令只读观察（不接管自动任务）");
for(auto&c:pageControls){int id=GetDlgCtrlID(c.hwnd);if(id==9191||id==9192)
 c.help=L"只读采样 OutList，统计可见的本地普通命令候选。工具自身发令、清空、换局或不明确的变化不作判断；不证明玩家命令已执行，也不取消自动任务。";}
#include "no_inline_status_ui.h"
