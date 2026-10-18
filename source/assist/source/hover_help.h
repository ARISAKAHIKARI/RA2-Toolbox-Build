// A row is one feature area, including its label, hotkey and mouse selector.
static std::wstring controlHelp(int id,const std::wstring& label){
 if(id>=200&&id<212)id-=100;
 if(id>=729&&id<=738){const int feature[]={450,478,474,702,705,708,719,0,720,721};if(id==736)return L"直接攻击移动：按所选鼠标键发送原版攻击移动；不启用持续自动索敌。";id=feature[id-729];}
 switch(id){
 case 100:case 312:case 780:return L"自动维修：定期为受损己方建筑提交原版维修；不可维修建筑需另勾选允许开关。";
 case 101:return L"作战单位：按快捷键选择当前可用的己方作战单位。";
 case 102:return L"空载具：选择没有乘员的己方运输载具。";
 case 103:return L"受损单位：选择受损的己方单位。";
 case 104:return L"闲置矿车：选择当前没有采矿任务的己方矿车。";
 case 105:return L"闲置工程师：选择空闲的己方工程师。";
 case 106:return L"闲置运输载具：选择当前空闲的己方运输载具。";
 case 107:return L"清理信标：移除本地战术信标标记。";
 case 108:case 739:return L"地图全开：实验性功能，先勾选允许开关，再执行地图全开快捷键。";
 case 109:return L"铁幕选兵：使用配置的快捷键选择铁幕相关单位。";
 case 110:return L"超时空选兵：使用配置的快捷键选择超时空相关单位。";
 case 111:case 700:case 701:return L"自动装车：选择乘员后执行快捷键，按范围和分配逻辑安排进入己方载具。";
 case 450:case 458:case 787:case 802:return L"高级移动攻击：按住快捷键＋所选鼠标键点地面；空键直接点击。射程＋3格索敌，排除伤害比例≤2%，可按价值或弹头比例优先；默认每秒10次；持续转火，直到新命令接管。";
 case 451:case 453:case 459:return L"智能卸载：当前版本暂不可用，入口已停用；逃生助手独立运行。";
 case 454:return L"取消当前自动任务：释放已选单位的高级移动、磁电轮换等持续任务，由原版命令接管。";
 case 455:case 456:return L"逃生助手：仅跟踪已选己方载具；被摧毁后，支持部署的乘员分别尝试原版部署，不等待同格其他乘员。";
 case 477:case 478:return L"磁电轮换攻击：开启后按住快捷键＋所选鼠标键点地面；出手后按武器冷却轮换目标，持续到新命令接管。";
 case 446:case 447:case 466:return L"自动 CS：原版右键移动生效后追加攻击移动；仅处理允许的己方单位，保留原始命令。";
 case 473:case 474:return L"按键 CS：启用后按住快捷键＋所选鼠标键点地面；快捷键留空时直接点击。";
 case 470:case 781:case 782:case 783:case 784:return L"自动倒矿：满载矿车按矿场容量排队；已到场矿车可有限插队，避免反复插队死锁；类型清单可自定义。";
 case 702:case 703:case 704:return L"高级进驻：按住快捷键＋所选鼠标键点击建筑或载具，按原版合法性与容量分配选中乘员；上限512个源对象。";
 case 705:case 706:case 707:case 708:case 709:case 710:case 711:case 712:case 713:case 714:case 715:case 716:case 717:case 718:return L"飞机均匀攻击：按模式键＋鼠标键点击敌方目标，给同类目标分配飞机；优先数与最大受击数可按类型覆盖，最大0为不限。";
 case 719:case 725:case 726:return L"飞碟主武器：按住快捷键＋所选鼠标键点击建筑，向所在格追加原版强制攻击，以使用主武器。";
 case 720:case 727:case 728:return L"变卖吸取目标：按住变卖键＋所选鼠标键点击合法己方目标；保留原版出售条件和退款逻辑。";
 case 721:case 789:return L"变卖战车（兰博玩不支持）：与上项共用按住键，鼠标键独立设置；仅提交原版支持的出售事件，仍受原版覆盖物和位置条件限制。";
 case 722:return L"变卖敌方建筑：当前版本停用。";
 case 788:return L"覆盖原生指令（默认关）：仅在统一消息入口识别到兼容的高级地面命令时尝试屏蔽原始点击；不删改OutList，其他路径保留原版。";
 case 795:case 796:case 797:return L"Shift 五连点：按住Shift点击生产侧栏，原生一次后补四次；移鼠标、松开Shift或切出游戏即取消。自动模式遇到Ares时使用其原生五连点。";
 case 804:return L"自动防偷点杀：默认哨戒炮、碉堡、盖特机炮；功能单位清单可改建筑ID。射程内优先攻击敌方工程师、C4、伊文和潜入单位；已锁定这些威胁时保留目标。经统一管理器提交原版攻击，不写目标指针。";
 case 476:return L"任意建造：默认关闭，显式勾选后放宽己方建筑 PassesProximityCheck 距离判断；不改 Prerequisite、不直接生成建筑，保留地形和占用检查。修改游戏判断，联机危险；异版本或其他插件占用时拒绝覆盖。";
 case 808:case 809:return L"绘制测试：只跳过黑幕/迷雾遮罩绘制；不写视野、不自动执行旧地图全开，也不解锁原版选中/攻击/雷达。默认关闭，未完成真实联机验证；关闭后恢复。";
 case 805:case 806:case 807:return L"飞机原生路径点：默认关闭；接通原生规划资格，到达路径点时尝试原生下一节点。使用游戏的路径点键（默认 Z），暂停旧外部队列。修改游戏判断，联机危险；关闭后恢复自己安装的补丁，不覆盖其他插件。";
 case 803:return L"外部飞机路径点（开启原生模式时暂停）：选中飞机，按住Z连续点击地面，松开Z后逐段执行；攻击或Ctrl强制攻击终结设置，普通新命令接管。每架最多64点，经统一GameRuntime提交原版移动。";
 case 799:return L"建筑预定：原生生产面板左侧显示可预定图标，灰色项是当前建筑放下后解锁的预览；点击预定一项，再点同项或右键取消。每类仅1项，原建筑放下并满足原版条件后继续生产。";
 case 798:return L"空闲飞机：小地图左侧显示飞机图标与空闲/总数；满弹停在机场或满弹自动返航计为空闲。点击轮流选择并定位一架，所有选择经GameRuntime提交。";
 case 785:return L"敌方位置标记：显示屏幕内敌方单位和建筑，包括未揭示区域；仅显示名称与玩家颜色，共用覆盖层刷新率。";
 case 786:return L"热键冲突时全部执行：开启后按匹配顺序提交所有动作；同一单位的后续命令可能覆盖前项。";
 case 790:case 791:case 792:return L"功能单位清单：选择功能并勾选自定义后，仅处理列出的单位ID；逗号分隔，空清单表示不处理。";
 case 440:case 441:case 442:case 443:case 467:case 449:return L"自动选兵：同一快捷键可绑定多个类型；单按选择屏幕内单位，双按选择全图单位。";
 case 724:return L"同类武器变体选兵：根据当前普通武器选择同类单位，忽略精英武器；单按屏幕、双按全图。";
 case 300:case 303:return L"载员覆盖层：显示载具当前乘员数与容量；菜单中隐藏，刷新率由显示设置控制。";
 case 301:return L"铁幕覆盖层：在单位上方显示铁幕剩余时间。";
 case 302:return L"队列空闲提醒：建筑、防御、步兵、战车四个页签在对应队列空闲、暂停或完成待处理时持续闪烁，恢复生产后停止。";
 case 304:return L"心灵覆盖层：显示支持的心灵控制和相关状态标记。";
 case 305:return L"箱子覆盖层：显示可读取的箱子位置与信息。";
 case 306:case 471:return L"文字面板：显示战术状态与播报；提示持续时间单位为毫秒。";
 case 310:case 311:return L"覆盖层显示：设置刷新率与上边距，单位载员和敌方标记使用同一刷新节奏。";
 case 420:case 423:case 424:case 425:case 426:case 427:return L"科技全开：当前仍受生产连续性限制；仅使用界面列出的可用路径，不改写单位Prerequisite。";
 case 761:case 762:return L"事件后端：统一GameRuntime内构造原版事件；此设置下次启动生效。";
 }
 if(!label.empty())return label+L"；设置修改后自动生效并保存。";
 return {};
}
static void updateHoverHelp(){
 POINT point{};GetCursorPos(&point);RECT window{};GetWindowRect(ui,&window);
 std::wstring next;
 if(PtInRect(&window,point))for(const auto&c:pageControls){if(c.page<0||!IsWindowVisible(c.hwnd))continue;RECT rect{};GetWindowRect(c.hwnd,&rect);
  if(point.y>=rect.top&&point.y<rect.bottom&&point.x>=window.left+15&&point.x<window.right-15&&!c.help.empty()){next=c.help;if(PtInRect(&rect,point))break;}
 }
 if(next!=hoverHelp){hoverHelp=next;SetWindowTextW(statusLabel,hoverHelp.empty()?statusText.c_str():hoverHelp.c_str());}
}
