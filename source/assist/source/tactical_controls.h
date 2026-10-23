// Included by WM_CREATE. Feature controls are created in visual/Tab order.
// IDs and setting handlers remain stable across the presentation redesign.
int section=0,rowY=0;
auto item=[&](const wchar_t* cls,const wchar_t* text,int x,int width,int id,DWORD style=0,int height=26){return pageControl(section,cls,text,x,rowY,width,height,id,style);};
auto label=[&](const wchar_t* text,int x=20,int width=605){return item(L"STATIC",text,x,width,0);};
auto next=[&](int height=26){rowY+=height+12;};
auto note=[&](const wchar_t* text,int=26){if(pageControls.empty())return;const int y=pageControls.back().rect.y;for(auto i=pageControls.rbegin();i!=pageControls.rend()&&i->page==section&&i->rect.y==y;++i)i->help=text;};
auto heading=[&](int page,int id,const wchar_t* title,const wchar_t* hint){section=page;rowY=0;item(L"STATIC",title,20,605,id,0,30);next(30);note(hint,44);};
auto toggle=[&](int id,const wchar_t* text,bool checked,int x=20,int width=605){auto h=item(L"BUTTON",text,x,width,id,BS_AUTOCHECKBOX|WS_TABSTOP);SendMessageW(h,BM_SETCHECK,checked?BST_CHECKED:BST_UNCHECKED,0);return h;};
auto button=[&](int id,const wchar_t* text,int x=20,int width=605){return item(L"BUTTON",text,x,width,id,BS_PUSHBUTTON|WS_TABSTOP);};
auto key=[&](int id,uint32_t binding,int x=345,int width=280,bool hold=false){auto h=item(HOTKEY_CLASSW,L"",x,width,id,WS_TABSTOP);if(hold){modernui::mouseOnly(h);modernui::holdKey(h);}SendMessageW(h,HKM_SETHOTKEY,binding,0);return h;};
auto edit=[&](int id,const std::wstring& text,int x=290,int width=335,bool numeric=false){return item(L"EDIT",text.c_str(),x,width,id,(numeric?ES_NUMBER:ES_AUTOHSCROLL)|WS_TABSTOP);};
auto combo=[&](int id,int x=290,int width=335,int drop=240){return item(L"COMBOBOX",L"",x,width,id,CBS_DROPDOWNLIST|WS_VSCROLL|WS_TABSTOP,drop);};
auto choices=[&](HWND h,std::initializer_list<const wchar_t*> names,int current){for(auto name:names)SendMessageW(h,CB_ADDSTRING,0,(LPARAM)name);SendMessageW(h,CB_SETCURSEL,current,0);};
auto number=[&](int id,const wchar_t* title,int value,const wchar_t* range){label(title,20,260);auto h=edit(id,std::to_wstring(value),290,90,true);label(range,397,228);next();return h;};
auto mouse=[&](int slot){auto h=extraControls[MouseFirst+slot]=combo(729+slot,510,115,180);choices(h,{L"停用",L"中键",L"侧键 1",L"侧键 2",L"右键",L"左键"},extra.mouseButtons[slot]==6?5:extra.mouseButtons[slot]);return h;};
auto holdPresetControl=[&](int preset,int custom,uint32_t binding){extraControls[preset]=combo(700+preset,240,100,180);choices(extraControls[preset],{L"仅鼠标",L"Alt",L"Ctrl",L"Shift",L"自定义"},holdPreset(binding));extraControls[custom]=key(700+custom,binding,350,148,true);};

heading(0,900,L"键盘快捷操作",L"按一次快捷键执行或切换；选择单位后可直接装车、卸载或启用逃生。 ");
label(L"操作 / 功能开关",20,306);label(L"快捷键",345,280);next();
for(int i:{11,1,2,3,4,5,6,7,0}){
 HWND h=i==0?toggle(100,labels[0],value(L"Modes",L"AutoRepair",0)!=0,20,306):button(100+i,labels[i],20,306);
 if(i==0)repairCheck=h;keys[i]=key(200+i,hotkeys[i]);next();
}
hookChecks[1]=toggle(459,L"智能卸载",hookSmartUnload,20,270);unloadKeyEdit=key(451,unloadKey,305,180);button(453,L"立即卸载",500,125);next();
ejectCheck=toggle(455,L"载员自动逃生",ejectMode,20,306);ejectKeyEdit=key(456,ejectKey);next();
csCheck=toggle(446,L"自动移动攻击（CS）",csMode,20,306);csKeyEdit=key(447,csKey);next();
label(L"喝彩",20,306);key(870,socialKeys[0]);next();
for(int i=1;i<9;++i){auto text=L"嘲讽 "+std::to_wstring(i);label(text.c_str(),20,306);key(870+i,socialKeys[i]);next();}
note(L"绑定后由工具箱接管该组合；聊天和菜单中不接管。嘲讽仍遵循原版冷却与接收方设置。",44);
label(L"轮选空闲飞机",20,306);key(853,idleAircraftKey);next();
label(L"按当前武器选择同类单位",20,306);extraControls[VariantKey]=key(724,extra.variantKey);next();
button(454,L"停止所选单位的自动任务");next();
note(L"组合选兵：多个单位类型可使用同一快捷键；单按选屏幕内，双按选全图。",44);
typeList=combo(440,20,315,300);typeKey=key(441,0,345,180);button(443,L"解除绑定",537,88);next();
label(L"手动添加单位 ID",20,180);typeIdEdit=edit(467,L"",210,315);button(449,L"添加",537,88);next();
tapEdit=number(444,L"选兵双按间隔 / 毫秒",doubleMs,L"20–5000");
typeCS=toggle(445,L"此单位类型参与自动移动攻击（CS）",false);next();
toggle(786,L"快捷键冲突时执行所有匹配功能",extra.allHotkeys);next();
note(L"同时执行多项功能时，同一单位后收到的命令可能替代前一条命令。",44);

heading(1,901,L"键鼠组合操作",L"按住配置键并点击目标；配置键留空时仅用鼠标。每项鼠标键可独立设置。");
label(L"功能开关",20,210);label(L"按住键 / 自定义组合",240,258);label(L"鼠标键",510,115);next();
hookChecks[0]=toggle(458,L"高级移动攻击",hookAdvanced,20,210);advancedKeyEdit=key(450,advancedKey,240,258,true);mouse(pointerinput::MouseAdvanced);next();
magnetronCheck=toggle(477,L"磁电轮换攻击",magnetronMode,20,210);magnetronKeyEdit=key(478,magnetronKey,240,258,true);mouse(pointerinput::MouseMagnetron);next();
 toggle(9042,L"磁电全局无限吸",extra.magnetronGlobal);next();note(L"自动接管空闲磁电，沿用轮换和类型清单；手动移动或攻击优先。关闭后停止自动接续。");
 toggle(899,L"磁电自动摔落",extra.magnetronDrop);next();note(L"吸起的目标正下方有建筑、地面坦克、树木、矿柱或岩石时，下达原版停止命令释放目标。可独立开启；启用轮换时继续寻找下一目标。");
csHoldCheck=toggle(473,L"按键移动攻击（CS）",csHoldEnabled,20,210);csHoldEdit=key(474,csHoldKey,240,258,true);mouse(pointerinput::MouseCS);next();
extraControls[EnterEnabled]=toggle(702,L"高级进驻",extra.enter,20,210);holdPresetControl(EnterPreset,EnterCustom,extra.enterKey);mouse(pointerinput::MouseEnter);next();
extraControls[AirEnabled]=toggle(705,L"飞机分配攻击 · 1",extra.air,20,210);holdPresetControl(AirPreset1,AirCustom1,extra.airKey1);mouse(pointerinput::MouseAirFirst);next();
label(L"飞机分配攻击 · 2",20,210);holdPresetControl(AirPreset2,AirCustom2,extra.airKey2);mouse(pointerinput::MouseAirSecond);next();
extraControls[DiskEnabled]=toggle(719,L"飞碟主武器攻击",extra.disk,20,210);holdPresetControl(DiskPreset,DiskCustom,extra.diskKey);mouse(pointerinput::MouseDisk);next();
extraControls[SellDrainedEnabled]=toggle(720,L"出售被吸取目标",extra.sellDrained,20,210);holdPresetControl(SellPreset,SellCustom,extra.sellKey);mouse(pointerinput::MouseSellDrained);next();
extraControls[SellVehiclesEnabled]=toggle(721,L"出售战车",extra.sellVehicles,20,210);key(789,extra.sellKey,240,258,true);mouse(pointerinput::MouseSellVehicle);next();
note(L"出售战车不支持兰博玩；两种出售操作共用按住键，鼠标键分别设置。",44);
label(L"直接攻击移动",20,210);label(L"仅鼠标",240,258);mouse(pointerinput::MouseAttack);next();
note(L"直接攻击移动只下达一次命令；高级移动攻击会持续重新选择目标。",44);
toggle(9050,L"队形排列（地面单位）",extra.formationEnabled);next();
label(L"队形按住键",20,306);key(9051,extra.formationHold,345,280,true);next();
{const wchar_t* names[]={L"矩形横队",L"分散横队",L"圆形聚集",L"扇形（60°）"};for(int n=0;n<4;++n){label(names[n],20,306);key(9052+n,extra.formationKeys[n]);next();}}
note(L"按住队形键右拖，或双击 Alt 后右拖。方向决定朝向，拖到 320 像素是一横排；直接右击聚集。Esc 取消。队形可共用按键，连续按下循环切换；提交一次退出。",44);
label(L"飞机路径点",20,260);choices(combo(803),{L"关闭",L"兼容路径点",L"原生优先"},extra.nativeAirWaypoints?2:extra.airWaypoints?1:0);next();
label(L"路径点按住键",20,306);{auto h=key(894,extra.airPathKey);modernui::holdKey(h);}next();
note(L"留空跟随游戏当前的路径点按键；自定义按键使用兼容路径点。原生优先在单机使用原生规划，联机自动使用兼容路径点。",44);
item(L"STATIC",L"飞机路径点：关闭",20,605,866);next();
toggle(795,L"Shift 五连点",shiftClickEnabled,20,270);choices(combo(796),{L"自动兼容",L"始终追加四次点击",L"不追加点击"},shiftClickMode);next();
number(797,L"连续点击间隔 / 毫秒",shiftClickGap,L"0–250");
label(L"超级武器目标选择",20,306);label(L"启用快捷键",345,280);next();
for(int i:{9,10}){button(100+i,labels[i],20,306);keys[i]=key(200+i,hotkeys[i]);next();}
swList=combo(410,20,315,260);swHotkey=key(411,0,345,280);next();
note(L"启用超级武器后，使用游戏内目标光标选择位置。",26);

heading(2,902,L"自动化与目标设置",L"调整自动任务的频率、范围及参与单位。操作说明会显示在底部状态栏。");
repairEdit=number(312,L"自动维修间隔 / 毫秒",repairMs,L"500–30000");
toggle(780,L"允许维修通常不可维修的建筑",repairUnrepairable);next();
oreCheck=toggle(470,L"矿车自动卸矿",oreMode);next();
oreStatus=item(L"STATIC",L"等待对局",20,605,0,0,44);next(44);
for(int i=0;i<2;++i){bool miner=i==0;toggle(781+i,miner?L"指定矿车类型":L"指定矿场类型",miner?oreTypes.customMiners:oreTypes.customRefineries);next();auto h=edit(783+i,miner?oreMiners:oreRefineries,20,605);SendMessageW(h,EM_SETLIMITTEXT,12287,0);next();}
note(L"类型 ID 用逗号分隔；未启用清单时自动识别，启用后仅使用所列类型。",44);
label(L"自动装车分配方式",20,260);extraControls[LoadLogic]=combo(700);choices(extraControls[LoadLogic],{L"就近优先",L"兵种均衡"},extra.loadMode);next();
extraControls[LoadRadius]=number(701,L"自动装车范围 / 格",extra.loadRadius,L"1–512");
number(787,L"高级移动攻击检测频率 / 次每秒",advancedScanHz,L"1–60 · 默认 10");
number(818,L"高级移动攻击额外索敌距离 / 格",advancedRangeCells,L"0–30 · 默认 2");
label(L"高级移动攻击目标优先级",20,260);choices(combo(802),{L"单位价值优先",L"弹头伤害比例优先"},advancedTargetPriority);next();
note(L"索敌范围为单位射程加额外距离；忽略伤害比例不超过 2% 的目标。",44);
toggle(804,L"防御建筑优先攻击渗透单位",extra.antiTheft);next();
toggle(885,L"自动躲弹（实验性）",extra.dodgeEnabled,20,280);choices(combo(895,310,305,180),{L"顺向移动 2 格",L"原版散开（Scatter）"},extra.dodgeMode);next();
note(L"对己方可移动地面单位预判敌机攻击与逼近碾压，短距离躲避。支持类型清单，不保证躲过追踪弹或瞬时伤害。",60);
toggle(880,L"防空火控",extra.missileDefense);next();
label(L"目标范围",20,260);choices(combo(9040),{L"仅导弹类",L"所有敌方空中单位"},extra.missileScope);next();
label(L"火力分配",20,260);choices(combo(9041),{L"智能分配（血量与伤害）",L"平均分配"},extra.missileAllocation);next();
number(881,L"火控检测频率 / 次每秒",extra.missileScanHz,L"1–60 · 默认 10");
note(L"优先拦截射程内的敌方导弹，分配火力减少重复锁定；移动中的单位不接管。",44);

// One visible Auto CS switch represents the previous two enable flags.
hookChecks[3]=placedControl({-3,{0,0,1,1}},L"BUTTON",L"",466,BS_AUTOCHECKBOX);
SendMessageW(hookChecks[3],BM_SETCHECK,hookAutoCS?BST_CHECKED:BST_UNCHECKED,0);ShowWindow(hookChecks[3],SW_HIDE);
csStatus=item(L"STATIC",L"移动攻击：等待对局",20,605,0,0,44);next(44);
nativeStatus=item(L"STATIC",L"战术任务：等待对局",20,605,0,0,44);next(44);
label(L"飞机分配攻击数量");next();
extraControls[DefaultFirst]=number(710,L"攻击模式 1 / 架每目标",extra.defaults.first,L"0–512 · 0 为不优先");
extraControls[DefaultSecond]=number(711,L"攻击模式 2 / 架每目标",extra.defaults.second,L"0–512 · 0 为不优先");
extraControls[DefaultMaximum]=number(712,L"每个目标最多承受 / 架",extra.defaults.maximum,L"0–512 · 0 为不限");
extraControls[QuotaType]=combo(713,20,275,260);extraControls[QuotaID]=edit(714,L"",305,185);button(751,L"读取类型",505,120);next();
extraControls[QuotaOverride]=toggle(715,L"为所选目标类型单独设置分配数量",false);next();
extraControls[QuotaFirst]=number(716,L"所选类型 · 攻击模式 1",1,L"0–512");
extraControls[QuotaSecond]=number(717,L"所选类型 · 攻击模式 2",2,L"0–512");
extraControls[QuotaMaximum]=number(718,L"所选类型 · 最大数量",0,L"0–512 · 0 为不限");
heading(5,905,L"单位类型清单",L"每个功能独立设置。关闭限制时按功能本身的条件处理；开启且清单为空时不处理任何类型。");
for(int f=0;f<featuretypes::Count;++f){
 if(f==featuretypes::BurstRefresh)continue; // Retired slot stays in the shared ABI.
 label(featuretypes::name(f),20,350);toggle(9200+f*2,L"仅处理清单中的类型",featureTypes.filters[f].custom,380,245);next();
 auto h=edit(9201+f*2,featureTypeText[f],20,605);SendMessageW(h,EM_SETLIMITTEXT,12287,0);SendMessageW(h,EM_SETCUEBANNER,TRUE,(LPARAM)L"单位类型 ID，以逗号分隔");next();
 note(L"此开关和清单只作用于上方命名的功能；关闭限制时保留清单，开启且留空表示不处理任何类型。");
}

heading(3,903,L"战场信息",L"调整游戏内信息显示、缩放与位置；有效设置立即生效。");

#ifdef RA2_UNIFIED
{auto h=placedControl({-3,{0,0,1,1}},L"COMBOBOX",L"",808,CBS_DROPDOWNLIST);choices(h,{L"关闭",L"半透明（实验性）",L"透明（实验性）",L"自动全图（实验性）"},extra.mapStrategy);}
placedControl({-3,{0,0,1,1}},L"STATIC",L"",809,0);
#else
label(L"黑幕绘制方式",20,260);choices(combo(808),{L"关闭",L"半透明（实验性）",L"透明（实验性）",L"自动全图（实验性）"},extra.mapStrategy);next();
note(L"半透明与不渲染仅改变显示；地图全开在对局就绪后自动执行，可能导致联机不同步。",44);
item(L"STATIC",L"地图显示：关闭",20,605,809);next();
#endif
toggle(799,L"生产预定",extra.constructionQueue);next();

const wchar_t* displayNames[]={L"载具载员与容量",L"铁幕 / 力场剩余时间",L"生产队列空闲提醒",L"显示未选中的载具",L"敌方索敌显示",L"箱子信息",L"战术文字提示"};
const wchar_t* displayIni[]={L"Capacity",L"Iron",L"Production",L"AllTransports",L"Psychic",L"Crates",L"TextPanel"};
#ifdef RA2_UNIFIED
// Keep the original setting owners and IDs; their visible entries live in the
// corresponding suite panes and send edits to this UI thread.
auto hiddenToggle=[&](int id,const wchar_t* text,bool on){auto h=placedControl({-3,{0,0,1,1}},L"BUTTON",text,id,BS_AUTOCHECKBOX);SendMessageW(h,BM_SETCHECK,on?BST_CHECKED:BST_UNCHECKED,0);ShowWindow(h,SW_HIDE);return h;};
for(int i=0;i<7;++i){bool on=value(L"Display",displayIni[i],(i<3||i>=6)?1:0)!=0;if(i==0||i==1||i==3)check[i]=hiddenToggle(300+i,displayNames[i],on);else{check[i]=toggle(300+i,displayNames[i],on);next();}}
extraControls[FrozenEnabled]=hiddenToggle(723,L"冷冻剩余时间",extra.frozen);
extraControls[EnemyMarkersEnabled]=hiddenToggle(785,L"敌方单位提示",extra.enemyMarkers);
hiddenToggle(897,L"敌方载员气泡",extra.passengerBubbles);hiddenToggle(898,L"危险载员常驻提示",extra.dangerPassengers);
hiddenToggle(892,L"蜘蛛寄生标志",extra.parasiteMarker);hiddenToggle(893,L"火力 / 防御 / 速度强化",extra.crateBuffMarkers);
#else
for(int i=0;i<7;++i){check[i]=toggle(300+i,displayNames[i],value(L"Display",displayIni[i],(i<3||i>=6)?1:0)!=0);next();}
extraControls[FrozenEnabled]=toggle(723,L"冷冻剩余时间",extra.frozen);next();
extraControls[EnemyMarkersEnabled]=toggle(785,L"显示敌方单位提示",extra.enemyMarkers);next();
toggle(897,L"敌方载员气泡",extra.passengerBubbles);next();toggle(898,L"危险载员常驻提示",extra.dangerPassengers);next();
toggle(892,L"蜘蛛寄生标志",extra.parasiteMarker);next();
toggle(893,L"箱子强化标志",extra.crateBuffMarkers);next();
#endif
toggle(9060,L"左中局部观察窗（11×11 格）",extra.watchEnabled);next();
toggle(9061,L"自动跟随危险载员的敌方载具",extra.watchDanger);next();
label(L"固定 / 解除观察（悬停或选中）",20,306);key(9062,extra.watchKey);next();
toggle(798,L"空闲飞机指示器",extra.idleAircraft);next();
#ifdef RA2_UNIFIED
rateEdit=placedControl({-3,{0,0,1,1}},L"EDIT",std::to_wstring(fps).c_str(),310,ES_NUMBER);
placedControl({-3,{0,0,1,1}},L"EDIT",std::to_wstring(labelScalePercent).c_str(),816,ES_NUMBER);
#else
rateEdit=number(310,L"信息显示刷新率 / 帧每秒",fps,L"5–500");
number(816,L"载员与状态文字缩放 / %",labelScalePercent,L"50–250 · 默认 100");
#endif
marginEdit=number(311,L"文字提示上边距 / 像素",topMargin,L"0–4096");
toastEdit=number(471,L"战术提示时长 / 毫秒",toastMs,L"500–60000");
number(817,L"建筑预定面板缩放 / %",constructionScalePercent,L"50–250 · 默认 100");
toggle(830,L"战术小地图",minimapEnabled);next();
number(831,L"小地图缩放 / %",minimapScale,L"100–400 · 默认 200");
label(L"战术小地图位置",20,260);choices(combo(850),{L"原雷达左侧",L"画面左上",L"画面左下",L"自定义坐标"},minimapPosition);next();
label(L"自定义位置 X / Y",20,260);edit(851,std::to_wstring(minimapX),290,100,true);edit(852,std::to_wstring(minimapY),405,100,true);next();
note(L"坐标从游戏画面左上角计算；自动避开原雷达和飞机按钮。放大区域不接收点击。",44);
detailCheck=toggle(481,L"显示运行耗时",showDetails);next();

heading(4,904,L"高级与实验性设置",L"实验性功能尚未完成联机验证，可能导致不同步或掉线。建议单独测试。");
#ifdef RA2_UNIFIED
{auto h=placedControl({-3,{0,0,1,1}},L"COMBOBOX",L"",886,CBS_DROPDOWNLIST);choices(h,{L"仅光标",L"光标＋拖屏",L"全部",L"强制高刷（实验性 · 联机待验证）"},extra.scrollRefreshMode);}
#else
label(L"高刷策略（实验性）",20,260);choices(combo(886),{L"仅光标",L"光标＋拖屏",L"全部",L"强制高刷（实验性 · 联机待验证）"},extra.scrollRefreshMode);next();
number(887,L"画面目标刷新率 / 帧每秒",extra.scrollRefreshHz,L"30–500 · 默认 120");
note(L"默认仅光标；拖屏支持右拖与屏幕边缘。“全部”持续重绘，不改变游戏速度，不插帧。",44);
item(L"STATIC",L"高刷策略：仅光标",20,605,888);next();
#endif

anywhereCheck=toggle(476,L"扩展建筑放置范围（实验性）",buildAnywhereMode);next();
item(L"STATIC",L"功能状态：等待连接",20,605,807,0,44);next(44);
toggle(810,L"步兵强制进入建筑（实验性）",extra.c4Test,20,280);key(811,extra.c4Key,310,188,true);choices(combo(812,510,115,180),{L"停用",L"中键",L"侧键 1",L"侧键 2",L"右键",L"左键"},extra.c4Mouse==6?5:extra.c4Mouse);next();
item(L"STATIC",L"步兵强制进入建筑：关闭",20,605,813);next();
toggle(420,L"科技解锁（实验性）",techMode,20,305);techSearch=edit(421,L"",345,280);SendMessageW(techSearch,EM_SETCUEBANNER,TRUE,(LPARAM)L"搜索名称或单位 ID");next();
label(L"实现方式",20,180);choices(combo(896,220,405,180),{L"现有方案",L"经典兼容"},techImplementation);next();
note(L"经典兼容沿用已找回的早期工厂/建造上限检查与每秒补齐；保留原生侧栏。用于战网对照，尚未验证不掉线，也未确认等同于 0.3.2。",44);
techList=item(L"LISTBOX",L"",20,455,422,LBS_NOTIFY|WS_VSCROLL|WS_TABSTOP,152);
int listTop=rowY;button(423,L"生产所选",488,137);next();button(424,L"切换许可",488,137);next();button(425,L"全选",488,65);button(426,L"清空",560,65);next();button(427,L"兼容预设",488,137);rowY=listTop+164;
button(428,L"查看许可清单",20,285);button(814,L"取消所选类型的生产",320,305);next();
note(L"加入原生生产侧栏；仍需匹配阵营的生产设施、电力及可用建造数量。",44);
item(L"STATIC",L"生产进度：等待游戏",20,605,815,0,65);next(65);
toggle(860,L"操控其他玩家单位（实验性）",extra.controlAll);next();

item(L"STATIC",L"跨玩家操控：等待连接",20,605,865);next();
EnableWindow(toggle(861,L"切换操控方（暂不可用）",false),FALSE);next();
EnableWindow(combo(862,20,305,240),FALSE);EnableWindow(button(863,L"切换至所选玩家",340,285),FALSE);next();
EnableWindow(button(864,L"恢复原操控方",20,285),FALSE);next();

extraControls[SellEnemyEnabled]=HWND{};extra.sellEnemy=false;

toggle(788,L"优先执行增强指令",extra.overrideNative);next();
note(L"用于增强操作与普通地面命令冲突的情况；仅对支持的鼠标操作生效。",44);
item(L"STATIC",L"计算模式",20,605,832,0,30);next(30);
note(L"按功能选择工具箱计算或游戏内计算；修改后从下一局开始使用。",44);
for(int i=0;i<compute::Count;++i){if(i==compute::BurstRefresh)continue;label(compute::label(i),20,260);choices(combo(9000+i),{L"工具箱计算",L"游戏内计算"},(computeRequested>>i)&1u);next();}
item(L"STATIC",L"当前计算模式：等待对局",20,605,833,0,166);next(166);
label(L"移动指令兼容模式",20,260);megaQueueCombo=combo(761);choices(megaQueueCombo,{L"标准（推荐）",L"兼容模式（实验性）"},megaQueueMode);next();
label(L"维修与出售兼容模式",20,260);objectQueueCombo=combo(762);choices(objectQueueCombo,{L"标准（推荐）",L"兼容模式（实验性）"},objectQueueMode);next();
note(L"兼容模式需重启游戏和工具箱后生效。通常保持标准模式即可。",44);
// Retain the disabled compatibility control for old configuration handlers;
// it is not presented as an adjustable option.
schedulerEntryCombo=placedControl({-3,{0,0,1,1}},L"COMBOBOX",L"",760,CBS_DROPDOWNLIST);
choices(schedulerEntryCombo,{L"自动"},0);EnableWindow(schedulerEntryCombo,FALSE);ShowWindow(schedulerEntryCombo,SW_HIDE);
