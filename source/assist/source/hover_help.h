// Instructions describe visible behavior; implementation diagnostics belong in logs.
static std::wstring controlHelp(int id,const std::wstring& label){
 if(id>=200&&id<212)id-=100;
 if(id>=9000&&id<9000+compute::Count)return L"计算模式：工具箱计算便于分担游戏负载；游戏内计算可减少数据传递等待。修改后从下一局生效。";
 if(id>=729&&id<=738){const int feature[]={450,478,474,702,705,708,719,736,720,721};if(id!=736)id=feature[id-729];}
 switch(id){
 case 885:return L"自动躲弹（实验性）：检测威胁己方地面单位的已发射空袭弹体与近距离碾压，通过原版散开命令避让；转向或移动期间不重复下令。不限星级；可用类型清单限制。不会赋予免伤，地形、移动速度和联机延迟都会影响效果。";
 case 886:case 887:case 888:return L"高刷策略（实验性）：仅光标沿用光标页设置；光标＋拖屏补充右拖/边缘滚屏绘制；全部持续刷新画面。三种范围共用光标页目标刷新率，最高 500 FPS，自动跟随显示器，不改变逻辑帧率，不插帧；实际刷新受游戏与渲染器限制。";
 case 882:case 883:case 884:return L"强制刷弹（实验性）：自动模式按类型清单，按键模式只处理当前选中单位。未完成连射时尝试停止并重启原目标；按当前普通或精英武器判断，Burst<2 不处理。效果取决于引擎和联机延迟。";
 case 100:case 312:case 780:return L"自动维修：定期维修受损的己方建筑，快捷键用于开关。通常不可维修的建筑需要另行允许。";
 case 101:return L"选择当前可用的己方作战单位。";
 case 102:return L"选择没有乘员的己方运输载具。";
 case 103:return L"选择受损的己方单位。";
 case 104:return L"选择当前没有采矿任务的己方矿车。";
 case 105:return L"选择空闲的己方工程师。";
 case 106:return L"选择并定位空闲的己方运输载具。";
 case 107:return L"清除本地战术信标。";
 case 108:case 739:return L"地图全开：先在高级设置中勾选允许，再按快捷键或点击执行。实验性功能，可能导致联机不同步；关闭许可不会恢复已揭示区域。";
 case 109:case 110:case 410:case 411:return L"超级武器：按快捷键进入目标选择，再用鼠标选择位置。需要拥有该武器并完成充能。";
 case 111:case 700:case 701:return L"自动装车：选择乘员后按快捷键，在设定范围内按就近或兵种均衡方式分配到己方载具。";
 case 450:case 458:return L"高级移动攻击：按住配置键并点击地面；留空则仅用鼠标。优先攻击当前射程内目标，进入射程后停止前进；目标消失后继续原路线。支持单独 Ctrl、Shift、Alt。";
 case 787:return L"高级移动攻击转火频率：稳定攻击时每秒重新选择目标的次数，默认 10。移动中的射程进入判断逐逻辑帧检查。";
 case 818:return L"高级移动攻击额外索敌距离：在武器射程之外增加的格数，默认 2。范围外只接近、不提前下达攻击；始终优先已在实际射程内的目标。伤害比例不超过 2% 时忽略。";
 case 802:return L"高级移动攻击：先选当前实际射程内的目标，再按单位价值或弹头伤害比例排序。持续转火直到新的手动命令接管。";
 case 451:case 453:case 459:return L"智能卸载：选中己方载具后执行。乘员离车后自动部署或寻找可执行任务的目标，包括工程师占领；手动新命令可中止。";
 case 454:return L"停止所选单位的高级移动攻击、磁电轮换等持续任务。随后可正常下达游戏命令。";
 case 455:case 456:return L"载员自动逃生：跟踪已选中的己方载具；载具被毁后，为支持部署的幸存乘员分别尝试部署。快捷键用于开关。";
 case 477:case 478:return L"磁电轮换攻击：按住配置键并点击地面，按武器冷却轮换目标。持续执行，直到下达新命令或停止任务。";
 case 446:case 447:case 466:return L"自动移动攻击（自动 CS）：对允许的单位，将普通右键移动衔接为攻击移动。快捷键切换开关；参与的类型在组合选兵处设置。";
 case 473:case 474:return L"按键移动攻击（按键 CS）：按住配置键并用指定鼠标键点击地面。配置键留空时，仅点击该鼠标键即可执行。";
 case 470:case 781:case 782:case 783:case 784:return L"矿车自动卸矿（自动倒矿）：满载矿车自动选择矿场并排队。已到场矿车可有限优先进场；矿车和矿场类型均可指定。";
 case 702:case 703:case 704:return L"高级进驻：点击目标，将所选单位分散到屏幕内同类目标。支持载具、驻军建筑、坦克碉堡及维修厂或船厂；最多 512 个单位。";
 case 705:case 706:case 707:case 708:case 709:return L"飞机分配攻击：按模式键并点击敌方目标，将飞机分配给同类目标。两种模式的分配数量可在战术设置中调整。";
 case 710:case 711:case 712:case 713:case 714:case 715:case 716:case 717:case 718:case 751:return L"飞机攻击分配：设置每个目标分配的飞机数量，可按目标类型单独覆盖。优先数量为 0 表示不优先，最大数量为 0 表示不限。";
 case 719:case 725:case 726:return L"飞碟主武器攻击：按住配置键并点击建筑，使用主武器攻击建筑所在位置。配置键留空时仅用鼠标。";
 case 720:case 727:case 728:return L"出售被吸取目标：按住配置键并点击符合出售条件的己方目标。与出售战车共用配置键，退款遵循游戏规则。";
 case 721:case 789:return L"出售战车：兰博玩不支持。与出售被吸取目标共用配置键，鼠标键独立；目标位置和覆盖物仍需符合游戏出售条件。";
 case 722:return L"出售其他玩家建筑：当前版本不可用。";
 case 736:return L"直接攻击移动：仅按所选鼠标键点击地面，下达一次攻击移动。不会启用持续自动索敌。";
 case 788:return L"优先执行增强指令（覆盖原生指令）：当增强操作与普通地面命令冲突时，尝试抑制普通命令。仅支持部分鼠标操作，默认关闭。";
 case 795:case 796:case 797:return L"Shift 五连点：按住 Shift 点击生产图标，一次购买五个。自动模式兼容已有五连点；移动鼠标、松键或切出游戏即停止追加。";
 case 880:case 881:return L"防空单位智能拦截：按原版武器和含高度的射程判定，优先拦截敌方 V3、无畏级、雷鸣潜艇等可攻击导弹。默认每秒检测 10 次，可调整频率与单位清单。";
 case 804:return L"防御建筑优先攻击渗透单位（防偷点杀）：射程内优先锁定工程师、爆破和潜入单位。已锁定此类威胁时保持目标。";
 case 476:return L"扩展建筑放置范围（任意建造）：放宽建筑间距离限制，仍须满足地形和占用条件。默认关闭，可能导致联机不同步或掉线。";
 case 810:case 811:case 812:case 813:return L"步兵 C4：选中步兵，按住配置键点击敌方建筑；默认 Alt+C 加中键。实验性功能，可能导致联机不同步或掉线。";
 case 808:case 809:return L"地图渲染（实验性）：半透明保留淡化黑幕，透明隐藏黑幕；自动全图在每局就绪时揭示地图。三种均为实验性功能，联机可能不同步。";
 case 806:case 866:return L"飞机路径点：单机使用原生规划；联机按住 Z 设置路径，松开后依次执行，攻击命令结束路径设置。开启时暂停另一项路径点设置。";
 case 803:return L"飞机路径点：选中飞机，按住 Z 连续点击地面，松键后依次执行。攻击结束路径设置，新命令可接管；每架最多 64 点。";
 case 799:return L"生产预定：四类各 5 项，点击预定面板顶部分栏切换。正在建造或已预定的前置科技可解锁灰色预定；条件满足后自动生产，取消前置会清理关联预定。右键退顶层，Shift＋右键清空。";
 case 870:return L"喝彩：配置的快捷键替代原生按键，通过游戏原版喝彩事件发送；保留冷却，聊天与菜单中不接管。";
 case 871:case 872:case 873:case 874:case 875:case 876:case 877:case 878:return L"嘲讽 1–8：配置组合后替代原生快捷键。仅用于联机；保留原版冷却，本局关闭嘲讽或接收方静音时不会强行播放。";
 case 853:case 798:return L"空闲飞机：满弹停靠或满弹自动返航计为空闲。点击指示器或按快捷键逐架选择定位；默认 Ctrl+Alt+P，隐藏指示器仍可使用。";
 case 892:return L"在被寄生单位当前位置显示约 14×14 的蜘蛛图标，随寄生者当前所属玩家变色。";
 case 893:return L"火力、装甲、速度箱子强化以约 7×7 小图标显示在单位下沿，限制在本格内；不把星级加成算作箱子。";
 case 785:return L"敌方位置标记：显示当前画面内的敌方单位和建筑，包括未揭示区域。名称按玩家颜色显示，使用信息显示刷新率。";
 case 786:return L"快捷键冲突时执行所有匹配功能：关闭时阻止重复绑定；开启后依次执行。同一单位后收到的命令可能替代前一条。";
 case 790:case 791:case 792:return L"功能单位清单：选择功能后指定参与的单位类型 ID，用逗号分隔。未启用时自动识别；启用后留空表示不处理任何类型。";
 case 440:case 441:case 443:case 467:case 449:return L"组合选兵：同一快捷键可绑定多个单位类型。单按选择屏幕内，双按选择全图；解除绑定仅影响当前类型。";
 case 444:return L"选兵双按间隔：两次按键在此时间内视为双按，范围 20–5000 毫秒。";
 case 445:return L"允许当前选中类型参与自动移动攻击（CS）；仅适用于支持的步兵和载具。";
 case 724:return L"按当前武器选择同类单位：依据普通武器识别，忽略精英武器。单按选屏幕内，双按选全图。";
 case 300:case 303:return L"载员信息：显示载具的乘员数量与容量，可选择同时显示未选中的载具。游戏菜单中自动隐藏。";
 case 301:case 723:return L"在单位上方显示铁幕、力场或冻结剩余时间，文字大小由状态文字缩放控制。";
 case 302:return L"生产队列空闲提醒：建筑和载具页签在空闲、暂停或等待处理时闪烁，恢复生产后停止。防御和步兵页签不闪烁。";
 case 304:return L"敌方索敌显示：显示敌方单位当前锁定己方单位的目标连线。";
 case 305:return L"箱子信息：显示可读取的箱子位置与内容提示。";
 case 306:case 471:return L"战术文字提示：显示功能状态与战术播报，持续时间以毫秒设置。";
 case 310:case 311:return L"信息显示：刷新率控制载员、状态时间和敌方标记等显示更新；上边距控制文字提示位置。";
 case 816:return L"载员与状态文字缩放：范围 50–250%，默认 100%。文字随游戏画面缩放，仍保持在单位对应位置。";
 case 830:case 831:case 850:case 851:case 852:return L"战术小地图：独立绘制地形、玩家颜色标记和当前视野框；支持缩放和位置设置，自动避开飞机按钮与原雷达。鼠标穿透，菜单中自动隐藏。";
 case 817:return L"建筑预定面板缩放：范围 50–250%，默认 100%。同步缩放图标、编号和点击区域，不改变游戏侧栏大小。";
 case 481:return L"显示运行耗时：在状态栏查看指令响应、读取和画面更新的耗时。";
 case 860:case 865:return L"选择其他玩家单位：当前版本不可用。";
 case 861:case 862:case 863:case 864:return L"切换操控方：当前版本不可用。";
 case 814:case 815:case 420:case 421:case 422:case 423:case 424:case 425:case 426:case 427:case 428:return L"科技生产：启用后补充原生侧栏，也可从列表生产；仍需匹配阵营的生产设施、电力及可用建造数量。实验性功能，联机效果尚未验证。";
 case 761:case 762:return L"指令兼容模式：通常使用标准模式。兼容选项为实验性功能；修改后须重启游戏和工具箱。";
 case 832:case 833:return L"计算模式按功能独立设置，修改后从下一局生效。当前对局继续使用进入本局时的设置。";
 }
 return label;
}
static void updateHoverHelp(){
 POINT point{};GetCursorPos(&point);RECT client{};GetClientRect(ui,&client);POINT local=point;ScreenToClient(ui,&local);
 std::wstring next;
 if(local.x>=15&&local.x<client.right-15&&local.y>=uilayout::BodyTop&&local.y<uilayout::bodyBottom(client.bottom))
 for(const auto&c:pageControls){if(c.page<0||!IsWindowVisible(c.hwnd))continue;RECT rect{};GetWindowRect(c.hwnd,&rect);
  // Combo dropdown height is not part of its collapsed feature row.
  rect.bottom=rect.top+c.visibleHeight;
  if(point.y>=rect.top&&point.y<rect.bottom&&!c.help.empty()){next=c.help;if(PtInRect(&rect,point))break;}
 }
 if(next!=hoverHelp){hoverHelp=next;SetWindowTextW(statusLabel,hoverHelp.empty()?statusText.c_str():hoverHelp.c_str());}
}
