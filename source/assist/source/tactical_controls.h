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
extraControls[EnterEnabled]=toggle(702,L"高级进驻",extra.enter,20,210);holdPresetControl(EnterPreset,EnterCustom,extra.enterKey);mouse(pointerinput::MouseEnter);next();
extraControls[AirEnabled]=toggle(705,L"飞机分配攻击 · 1",extra.air,20,210);holdPresetControl(AirPreset1,AirCustom1,extra.airKey1);mouse(pointerinput::MouseAirFirst);next();
label(L"飞机分配攻击 · 2",20,210);holdPresetControl(AirPreset2,AirCustom2,extra.airKey2);mouse(pointerinput::MouseAirSecond);next();
label(L"飞机分配策略（1 / 2 共用）",20,260);extraControls[AircraftPolicy]=combo(9195);choices(extraControls[AircraftPolicy],{L"按数量配额（原模式 1 / 2）",L"5格战车 · 预计伤害 120%",L"单目标 120% · 派出后取消选中"},extra.aircraftAllocationPolicy);next();
note(L"5格策略只选点选中心5格圆形内敌方战车（含边界），预计有效伤害足额后改分；没有其他未覆盖的可攻击目标时才追加。单目标策略达到生命值120%即停，仅成功提交的飞机取消选中，未派出飞机保持任务和选中；左/右键须开启“优先执行增强指令”接管原点击，独占中键/侧键可用。单目标不支持Shift排队（按住Shift时不派出）。预计伤害不保证实际命中。",76);
extraControls[DiskEnabled]=toggle(719,L"飞碟主武器攻击",extra.disk,20,210);holdPresetControl(DiskPreset,DiskCustom,extra.diskKey);mouse(pointerinput::MouseDisk);next();
extraControls[SellDrainedEnabled]=toggle(720,L"出售被吸取目标",extra.sellDrained,20,210);holdPresetControl(SellPreset,SellCustom,extra.sellKey);mouse(pointerinput::MouseSellDrained);next();
extraControls[SellVehiclesEnabled]=toggle(721,L"出售战车",extra.sellVehicles,20,210);key(789,extra.sellKey,240,258,true);mouse(pointerinput::MouseSellVehicle);next();
note(L"出售战车不支持兰博玩；两种出售操作共用按住键，鼠标键分别设置。",44);
label(L"直接攻击移动",20,210);label(L"仅鼠标",240,258);mouse(pointerinput::MouseAttack);next();
note(L"直接攻击移动只下达一次命令；高级移动攻击会持续重新选择目标。",44);
toggle(9050,L"队形排列（地面单位）",extra.formationEnabled);next();
label(L"队形拖拽鼠标键",20,260);choices(combo(9056),{L"右键",L"左键（覆盖原生下令）"},extra.formationMouse==1?1:0);next();
label(L"队形按住键",20,306);key(9051,extra.formationHold,345,280,true);modernui::keyboardOnly(GetDlgItem(ui,9051));next();
{const wchar_t* names[]={L"矩形横队",L"分散横队",L"圆形聚集",L"扇形（60°）",L"正方形聚集",L"空心框聚集"};const int ids[]={9052,9053,9054,9055,9057,9058};for(int n=0;n<6;++n){label(names[n],20,306);key(ids[n],extra.formationKeys[n]);modernui::pressKey(GetDlgItem(ui,ids[n]));modernui::keyboardOnly(GetDlgItem(ui,ids[n]));next();}}
note(L"按住队形键或双击 Alt 进入，点按提交当前预览；拖动方向决定朝向。形状可共用键，每敲一次切换，长按不重复。Esc 取消。提交后退出预览；成员不变时后续移动保持队形。",44);
note(L"正方形默认 B，空心框默认 N。框形未拖动时边框面积与中空面积约 3∶1，拖到最大变成单层边框；其余形状拖到最大为一横排。",44);
label(L"飞机路径点",20,260);choices(combo(803),{L"关闭",L"兼容路径点",L"原生优先",L"原生下单（联机实验）",L"完整原生（联机实验）"},airpathpolicy::mode(extra.airWaypoints,extra.nativeAirWaypoints,extra.nativeAirNetwork));next();
label(L"路径点按住键",20,306);{auto h=key(894,extra.airPathKey);modernui::holdKey(h);}next();
note(L"留空跟随游戏路径点键；自定义键使用兼容路径点。原生优先在联机使用兼容模式。原生下单只放宽入口，可能不续飞；完整原生另改本机到达任务，可能不同步。",60);
item(L"STATIC",L"飞机路径点：关闭",20,605,866);next();
toggle(795,L"Shift 五连点",shiftClickEnabled,20,270);choices(combo(796),{L"自动兼容",L"始终追加四次点击",L"不追加点击"},shiftClickMode);next();
number(797,L"连续点击间隔 / 毫秒",shiftClickGap,L"0–250");
label(L"超级武器目标选择",20,306);label(L"启用快捷键",345,280);next();
for(int i:{9,10}){button(100+i,labels[i],20,306);keys[i]=key(200+i,hotkeys[i]);next();}
swList=combo(410,20,315,260);swHotkey=key(411,0,345,280);next();
note(L"启用超级武器后，使用游戏内目标光标选择位置。",26);

heading(2,902,L"自动化与目标设置",L"调整自动任务的频率、范围及参与单位。操作说明会显示在底部状态栏。");
toggle(799,L"生产预定",extra.constructionQueue);next();
note(L"建筑/防御各预定 5 项，步兵/载具各 30 项；可建造单位接连进入原生队列。",44);
number(817,L"建筑预定面板缩放 / %",constructionScalePercent,L"50–250 · 默认 100");
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
toggle(880,L"防空火控",extra.missileDefense);next();
label(L"目标范围",20,260);choices(combo(9040),{L"仅导弹类",L"所有敌方空中单位"},extra.missileScope);next();
label(L"火力分配",20,260);choices(combo(9041),{L"智能分配（血量与伤害）",L"平均分配"},extra.missileAllocation);next();
toggle(9181,L"提前休眠留枪",extra.missileReserve);next();
number(881,L"火控检测频率 / 次每秒",extra.missileScanHz,L"1–60 · 默认 10");
note(L"智能分配以预计 120% 血量为覆盖线。提前休眠留枪：建筑、战车、步兵先 Sleep 等待已分配目标进入原生射程，冷却或多余火力继续等待；无候选时恢复 Guard。关闭留枪则沿用原版攻击接近。手动命令和队形优先。",60);
toggle(9182,L"过滤低伤害自动目标",extra.lowDamageFilter);next();
toggle(9183,L"联机也修改原生索敌（实验性）",extra.lowDamageNetwork);next();
note(L"忽略实际所选武器对目标护甲伤害倍率不超过 2% 的自动索敌候选，保留手动点名。单机过滤己方原生索敌；联机默认仅过滤工具命令。联机原生实验影响全部阵营，所有端需一致启用，仍未实测。",60);
item(L"STATIC",L"低伤害过滤：关闭",20,605,9184);next();

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
 note(L"此开关和清单只作用于上方命名的功能；不勾选时忽略预填 ID，按功能自身能力条件自动识别；勾选后再按清单缩小范围，留空则不处理任何类型。");
}

heading(3,903,L"战场信息",L"调整游戏内信息显示、缩放与位置；有效设置立即生效。");

label(L"黑幕绘制方式",20,260);choices(combo(808),{L"关闭",L"受控模式（半透明）",L"受控模式（透明）",L"原生揭示（实验性）",L"直接模式（全图）",L"素材替换（透明）",L"素材替换（半透明）"},extra.mapStrategy);next();
note(L"素材替换保留原版绘制流程，关闭时恢复已备份的素材；仅改变显示，不改变探索状态。");
item(L"STATIC",L"地图显示：关闭",20,605,809);next();
const wchar_t* displayNames[]={L"载具载员与容量",L"铁幕 / 力场剩余时间",L"生产队列空闲提醒",L"显示未选中的载具",L"敌方索敌显示",L"箱子信息",L"战术文字提示"};
const wchar_t* displayIni[]={L"Capacity",L"Iron",L"Production",L"AllTransports",L"Psychic",L"Crates",L"TextPanel"};
#ifdef RA2_UNIFIED
// Keep the original setting owners and IDs; their visible entries live in the
// corresponding suite panes and send edits to this UI thread.
auto hiddenToggle=[&](int id,const wchar_t* text,bool on){auto h=placedControl({-3,{0,0,1,1}},L"BUTTON",text,id,BS_AUTOCHECKBOX);SendMessageW(h,BM_SETCHECK,on?BST_CHECKED:BST_UNCHECKED,0);ShowWindow(h,SW_HIDE);return h;};
for(int i=0;i<7;++i){bool on=value(L"Display",displayIni[i],0)!=0;if(i==0||i==1||i==3)check[i]=hiddenToggle(300+i,displayNames[i],on);else{check[i]=toggle(300+i,displayNames[i],on);next();}}
extraControls[FrozenEnabled]=hiddenToggle(723,L"冷冻剩余时间",extra.frozen);
extraControls[EnemyMarkersEnabled]=hiddenToggle(785,L"敌方单位提示",extra.enemyMarkers);
hiddenToggle(897,L"敌方载员气泡",extra.passengerBubbles);hiddenToggle(898,L"危险载员常驻提示",extra.dangerPassengers);
hiddenToggle(892,L"蜘蛛寄生标志",extra.parasiteMarker);hiddenToggle(893,L"火力 / 防御 / 速度强化",extra.crateBuffMarkers);
#else
for(int i=0;i<7;++i){check[i]=toggle(300+i,displayNames[i],value(L"Display",displayIni[i],0)!=0);next();}
extraControls[FrozenEnabled]=toggle(723,L"冷冻剩余时间",extra.frozen);next();
extraControls[EnemyMarkersEnabled]=toggle(785,L"显示敌方单位提示",extra.enemyMarkers);next();
toggle(897,L"敌方载员气泡",extra.passengerBubbles);next();toggle(898,L"危险载员常驻提示",extra.dangerPassengers);next();
toggle(892,L"蜘蛛寄生标志",extra.parasiteMarker);next();
toggle(893,L"箱子强化标志",extra.crateBuffMarkers);next();
#endif
const wchar_t* lineNames[]={L"队列提醒：建筑",L"队列提醒：防御",L"队列提醒：步兵",L"队列提醒：载具",L"队列提醒：舰船",L"队列提醒：飞机"};
for(int i=0;i<6;++i){toggle(9070+i,lineNames[i],(extra.productionReminderMask&(1u<<i))!=0);next();}
toggle(9060,L"游戏内小窗跟踪",extra.watchEnabled);next();
toggle(9163,L"第二显示器显示小窗跟踪",extra.watchSecondary);next();
label(L"副屏跟踪组位置 X / Y",20,260);edit(9164,std::to_wstring(extra.watchSecondaryX),290,90,true);edit(9165,std::to_wstring(extra.watchSecondaryY),405,90,true);next();
note(L"位置范围 0–10000：0 为左／上，10000 为右／下；在副屏小窗顶部拖动可整体移动三个窗口。",44);
toggle(9061,L"自动跟随危险载员的敌方载具",extra.watchDanger);next();
for(int n=0;n<3;++n){const wchar_t* names[]={L"跟踪窗口 1 快捷键",L"跟踪窗口 2 快捷键",L"跟踪窗口 3 快捷键"};label(names[n],20,306);key(9062+n,extra.watchKeys[n]);next();}
note(L"同键每按一次添加一个窗口，只使用该键绑定的空位。左击跳转；Shift＋右击关闭。组内目标死亡时，接替原选中组中距它 5 格内的最近单位。",44);
toggle(798,L"空闲飞机指示器",extra.idleAircraft);next();
#ifdef RA2_UNIFIED
rateEdit=placedControl({-3,{0,0,1,1}},L"EDIT",std::to_wstring(fps).c_str(),310,ES_NUMBER);
hiddenToggle(9160,L"自适应屏幕刷新率",infoAdaptive);
placedControl({-3,{0,0,1,1}},L"EDIT",std::to_wstring(labelScalePercent).c_str(),816,ES_NUMBER);
#else
toggle(9160,L"自适应屏幕刷新率",infoAdaptive);next();
rateEdit=number(310,L"手动信息刷新率 / Hz",fps,L"5–500");
item(L"STATIC",(L"当前："+std::to_wstring(informationFPS())+L" Hz").c_str(),20,605,9161);next();
number(816,L"载员与状态文字缩放 / %",labelScalePercent,L"50–250 · 默认 100");
#endif
marginEdit=number(311,L"文字提示上边距 / 像素",topMargin,L"0–4096");
toastEdit=number(471,L"战术提示时长 / 毫秒",toastMs,L"500–60000");
toggle(9325,L"矿车指示器（工作中 / 总数）",harvesterCounterEnabled);next();
toggle(9326,L"电量指示器（发电 − 耗电）",powerDeltaEnabled);next();
toggle(9300,L"渲染升级（全视口 · 实验性）",renderUpgradeEnabled);next();
toggle(9313,L"额外画质处理（关闭：基础快速绘制）",renderProcessing);next();
note(L"仅开启渲染升级时，跳过扩色、连续光照增强、精细 VXL与 xBRZ；仍执行基本素材解码和原生色盘映射。下方已保存的画质选项保留，需独立开启额外画质处理才生效。",64);
toggle(9306,L"4K 物理像素增强（独占全屏保留原分辨率 · 实验性）",renderPhysicalOutput);next();
label(L"渲染层级",20,260);{auto layer=combo(9302);choices(layer,{L"场景固定在信息层下方"},0);EnableWindow(layer,FALSE);}next();
toggle(9301,L"统一覆盖层（世界信息＋主屏统计）",unifiedOverlayEnabled);next();
label(L"256 色素材输出",20,260);choices(combo(9304),{L"原始 DAC（6-bit ×4，对照）",L"完整 24-bit RGB（6-bit→8-bit）"},renderPaletteRange);next();
label(L"颜色计算精度",20,260);choices(combo(9309),{L"兼容色阶（原有光照计算）",L"增强 RGB（连续光照＋匹配原色盘恢复）"},renderColorPrecision);next();
note(L"增强档保留 RGB888／32 位输出，减少光照台阶；仅当原始色盘重新量化后与运行时 RGB565 全表一致，才补回原色盘中的精度。252 是旧式色盘展开后的通道上限，不是颜色数量；不能恢复没有来源的丢失颜色。非 HDR／10-bit 输出。",72);
toggle(9324,L"高刷移动补帧（帧通知采样 · 实验性）",renderHighRefresh);next();
note(L"游戏完成逻辑帧后通知独立采样；显示位置在最新两帧基础上最多推演一帧且不超过 25 ms，不缓存等待下一游戏帧。基础绘制也可使用。",64);
label(L"场景精细度",20,260);choices(combo(9308),{L"原生像素合成（对照）",L"精细：VXL 自适应采样＋256 向转动＋物理像素合成"},renderSceneDetail);next();
toggle(9314,L"启用 xBRZ 边缘重建（独立开启）",renderXbrzEnabled);next();
label(L"素材边缘重建",20,260);choices(combo(9305),{L"面积采样（不使用 xBRZ）",L"最近邻（实际倍率）",L"xBRZ（实际倍率）",L"自适应 1–4× xBRZ"},renderPixelScale);next();
toggle(9310,L"优先硬件 GPU（主视图、副相机与小地图）",renderVulkanEnabled);next();
toggle(9320,L"粒子与动画效果",renderParticleEffects);next();toggle(9321,L"激光与光束效果",renderLaserEffects);next();
toggle(9322,L"VXL 实时光影重算",renderVoxelLighting);next();toggle(9323,L"SHP 实时光影重算",renderSpriteLighting);next();
note(L"基础与精细档均可由 GPU 合成场景、光照、深度、阴影与迷雾；精细档额外使用自适应 VXL，xBRZ 仍需独立开启。实景小地图缩小使用 GPU 分页面积采样，保留原尺寸遮挡。素材缓存复用；采样、姿态与材质准备仍在 CPU。设备或功能不可用时回退 CPU。",88);
note(L"SHP/TMP 使用 xBRZ 1.9 原版算法处理当前光照后的颜色，VXL 按实际输出比例重新采样几何。GPU 为同规则移植并执行设备自检，失败时保留上游 CPU 路径。非整数倍率先用 2–4× xBRZ，再缩至目标像素；实时光照或色盘变化会更新结果。",88);
note(L"4K 增强仅在游戏客户区覆盖整块物理显示器且 DPI/视口校验通过时生效。独占全屏若切换到 2560×1440，仍使用现有覆盖层按 2560×1440 正常绘制，只停用 4K 增强；不会因为独占就隐藏覆盖层。2K 窗口也不会假冒 4K。最终倍率按游戏视口与实际客户区计算，支持 1.5× 等分数比例；不固定放大 2×，不改变镜头与鼠标命中。",88);
note(L"采样、场景准备与呈现独立运行。高刷移动使用最新观测的有界显示推演，逻辑速度与联网事件不变；主视图每次呈现只更新实例位置，不重复解码。帧通知不可用时检测原生帧号变化；游戏速度决定样本频率，目标显示率由屏幕/覆盖层设置决定。",88);
note(L"重绘场景位于信息与交互面板下方，只读呈现。支持框选、血条、光标、路径、卸载绘制顺序，以及生成物、驻军、吸收乘员、心控和范围提示。建筑红绿格沿用原生判定。素材或视口未就绪时保留短期完整帧或回退原画。完整原生 Z/A、位移隐形及部分特殊效果仍有差异；实机与联机验证未完成。",88);
item(L"STATIC",L"渲染升级：实验性，默认关闭",20,605,9303,0,180);next(180);
toggle(830,L"游戏内战术小地图",minimapEnabled);next();
toggle(9162,L"第二显示器显示小地图",minimapSecondary);next();
label(L"副屏小地图位置 X / Y",20,260);edit(9166,std::to_wstring(minimapSecondaryX),290,90,true);edit(9167,std::to_wstring(minimapSecondaryY),405,90,true);next();
number(9168,L"副屏小地图宽度 / 像素",minimapSecondaryWidth,L"160–4096 · 默认 320");
note(L"位置 0 为左／上，10000 为右／下；副屏小地图顶部可拖动，左上角或右下角可等比调整大小。副屏设置与游戏内小地图分开保存。",60);
label(L"小地图画面",20,260);choices(combo(9065),{L"示意",L"实景"},minimapMode);next();
number(831,L"小地图缩放 / %",minimapScale,L"5–200 · 默认 10");
note(L"实景模式 100% 对应地图原生像素大小；显示区域不足时等比适配。",36);
label(L"战术小地图位置",20,260);choices(combo(850),{L"原雷达左侧",L"画面左上",L"画面左下",L"自定义坐标"},minimapPosition);next();
label(L"自定义位置 X / Y",20,260);edit(851,std::to_wstring(minimapX),290,100,true);edit(852,std::to_wstring(minimapY),405,100,true);next();
label(L"自定义水平对齐",20,260);choices(combo(9100),{L"靠左",L"居中",L"靠右"},minimapAlignX);next();
label(L"自定义垂直对齐",20,260);choices(combo(9101),{L"靠上",L"居中",L"靠下"},minimapAlignY);next();
note(L"自定义坐标是布局框左上角，对齐决定地图在框内的位置。左上／左下预设按地图实际尺寸贴边；自动避开原雷达和飞机按钮。左击或拖动可移动主视角。",60);
detailCheck=toggle(481,L"显示运行耗时",showDetails);next();

heading(4,904,L"高级与实验性设置",L"实验性功能尚未完成联机验证，可能导致不同步或掉线。建议单独测试。");
toggle(885,L"自动躲弹（实验性）",extra.dodgeEnabled,20,280);choices(combo(895,310,305,180),{L"顺向移动 2 格",L"原版散开（Scatter）"},extra.dodgeMode);next();
note(L"对己方可移动地面单位预判敌机攻击与逼近碾压，短距离躲避。支持类型清单，不保证躲过追踪弹或瞬时伤害。",60);
button(9094,L"Sticky 驻守（实验性／支持联机进入）",20,306);key(9095,extra.stickyKey);next();note(L"所选地面单位进入 Sticky=6；默认 Ctrl+Alt+S，可改组合或双击。联机仅支持同步进入，重复键不重发，退出尚未实现。单机可再次按键或手动下令解除；实际进入后显示金色标记。");
for(int i=0;i<selectedmode::Count;++i){button(9170+2*i,selectedmode::label(i),20,306);key(9171+2*i,extra.missionModeKeys[i]);next();}
note(L"对所选己方步兵、战车下达一次原版任务。默认未绑定，可设组合键或双击。驻守是 Guard=5，不是 G 的区域警戒。其他命令可覆盖；这三项不能解除 Sticky。",60);
csCheck=toggle(446,L"自动移动攻击（CS）",csMode,20,306);csKeyEdit=key(447,csKey);next();
csHoldCheck=toggle(473,L"按键移动攻击（CS）",csHoldEnabled,20,210);csHoldEdit=key(474,csHoldKey,240,258,true);mouse(pointerinput::MouseCS);next();
note(L"自动模式和按键模式均使用原版移动攻击命令；参与类型、快捷键和鼠标键设置保持不变。");
toggle(899,L"磁电全局自动摔落（实验性）",extra.magnetronDrop);next();
note(L"独立检查全图己方磁电已吸起的载具，包括己方、盟友和敌方，手动强制攻击也生效。检测建筑、地面坦克、树木、矿柱、悬崖及水陆不适配；两栖/悬浮不因普通水陆而摔，排除下方空中单位和桥面。默认关闭，实际摔落效果仍需测试。",76);
EnableWindow(toggle(9090,L"视角缩放（暂不可用）",false),FALSE);next();
label(L"视角倍率",20,260);choices(combo(9091),{L"50%",L"75%",L"100%",L"125%",L"150%"},viewzoom::index(extra.viewZoomPercent));EnableWindow(GetDlgItem(ui,9091),FALSE);next();
EnableWindow(button(9092,L"恢复 100%",20,280),FALSE);next();
note(L"视角缩放暂时停用；旧倍率设置和缩放快捷键均不生效。",56);
item(L"STATIC",L"视角缩放：关闭",20,605,9093,0,48);next(48);
#ifdef RA2_UNIFIED
{auto h=placedControl({-3,{0,0,1,1}},L"COMBOBOX",L"",886,CBS_DROPDOWNLIST);choices(h,{L"仅光标",L"光标＋拖屏",L"全部",L"强制高刷（实验性 · 联机待验证）"},extra.scrollRefreshMode);}
#else
label(L"高刷策略（实验性）",20,260);choices(combo(886),{L"仅光标",L"光标＋拖屏",L"全部",L"强制高刷（实验性 · 联机待验证）"},extra.scrollRefreshMode);next();
number(887,L"画面目标刷新率 / 帧每秒",extra.scrollRefreshHz,L"30–500 · 默认 120");
EnableWindow(GetDlgItem(ui,886),FALSE);EnableWindow(GetDlgItem(ui,887),FALSE);
note(L"当前仅保留光标高刷；拖屏、全部及强制高刷暂不可用。",44);
item(L"STATIC",L"高刷策略：仅光标",20,605,888);next();
#endif

anywhereCheck=toggle(476,L"随处建造（实验性）",buildAnywhereMode);next();
label(L"建造方式",20,180);choices(combo(9096,220,405,180),{L"受控模式（默认）",L"直接模式"},extra.placementImplementation);next();
item(L"STATIC",L"功能状态：等待连接",20,605,807,0,44);next(44);
toggle(810,L"步兵强制进入建筑（实验性）",extra.c4Test,20,280);key(811,extra.c4Key,310,188,true);choices(combo(812,510,115,180),{L"停用",L"中键",L"侧键 1",L"侧键 2",L"右键",L"左键"},extra.c4Mouse==6?5:extra.c4Mouse);next();
label(L"进入实现",20,260);choices(combo(9180),{L"占领任务＋建筑格",L"爆破任务＋建筑格"},extra.c4Mode);next();
note(L"两种实现均向被点击的建筑占地格下令。默认占领任务；普通点击直接提交，按住游戏路径点键时追加。建筑被坦克覆盖时从同格查找。实际能否进入由原版判定。",60);
item(L"STATIC",L"步兵强制进入建筑：关闭",20,605,813);next();
toggle(9097,L"科技加强（实验性）",extra.techEnhance);next();
note(L"开放奶牛、电影英雄等隐藏类型；关闭后恢复本功能修改的科技等级。可能影响联机同步。",44);
item(L"STATIC",L"科技加强：关闭",20,605,9098);next();
toggle(420,L"科技解锁（实验性）",techMode,20,305);techSearch=edit(421,L"",345,280);SendMessageW(techSearch,EM_SETCUEBANNER,TRUE,(LPARAM)L"搜索名称或单位 ID");next();
label(L"实现方式",20,180);choices(combo(896,220,405,180),{L"受控模式（默认）",L"经典兼容",L"直接模式"},techImplementation);next();
note(L"直接模式显示 TechLevel 非 -1 的项目，并保留侧栏图标；此模式不使用下方许可筛选。经典兼容保留早期检查。实际生产与联机效果仍待实测。",44);
techList=item(L"LISTBOX",L"",20,455,422,LBS_NOTIFY|WS_VSCROLL|WS_TABSTOP,152);
int listTop=rowY;button(423,L"生产所选",488,137);next();button(424,L"切换许可",488,137);next();button(425,L"全选",488,65);button(426,L"清空",560,65);next();button(427,L"兼容预设",488,137);rowY=listTop+164;
button(428,L"查看许可清单",20,285);button(814,L"取消所选类型的生产",320,305);next();
note(L"加入原生生产侧栏；仍需匹配阵营的生产设施、电力及可用建造数量。",44);
item(L"STATIC",L"生产进度：等待游戏",20,605,815,0,65);next(65);
toggle(9191,L"原生 MegaMission 统一仲裁",extra.nativeMegaGateway);next();
note(L"默认开启。普通原生命令成功发送后，登记为手动操作并取消冲突的工具自动任务。原生命令不受工具发送预算截断；原生规划和其他事件路径仍按各自流程执行。",65);
item(L"STATIC",L"原生命令仲裁：等待对局",20,605,9192,0,44);next(44);
toggle(860,L"操控其他玩家单位（实验性）",extra.controlAll);next();

item(L"STATIC",L"跨玩家操控：等待连接",20,605,865);next();
toggle(9193,L"命令使用单位所属方（实验性）",extra.controlCommandOwner);next();
note(L"默认关闭，配合上方跨玩家操控使用。联机还需事件所属方保留已启用；条件不满足时拒绝外方命令。他方单位规划命令暂不支持，请退出规划模式下令。实际联机效果待实测。",65);
item(L"STATIC",L"控制命令所属方：关闭",20,605,9194,0,44);next(44);
toggle(9187,L"保留事件所属方（实验性）",extra.networkHousePreserve);next();
note(L"尝试在联机封包时保留每条事件原有的所属方。只有状态为已启用时才允许跨所属方请求；关闭后需完成待发事件处理再恢复。仍受原版调度及平台处理影响，不能保证联机可用。",65);
item(L"STATIC",L"事件所属方保留：关闭",20,605,9188,0,44);next(44);
EnableWindow(toggle(861,L"切换操控方（暂不可用）",false),FALSE);next();
label(L"目标所属方（请明确选择）");next();
EnableWindow(combo(862,20,305,240),FALSE);EnableWindow(button(863,L"切换至所选玩家",340,285),FALSE);next();
EnableWindow(button(864,L"恢复原操控方",20,285),FALSE);next();
EnableWindow(button(9185,L"自毁所选所属方（实验性）"),FALSE);next();
note(L"点击一次提交整方自毁请求，影响该方全部单位和建筑。联机选择其他所属方需要“保留事件所属方”已启用；关闭或恢复中不允许提交。单机可选择其他有效所属方，但AI所属方仍可能受原版调度限制。已入队不代表已自毁。",65);
item(L"STATIC",L"自毁：请明确选择所属方；联机跨方请求需所属方保留已启用。",20,605,9186,0,52);next(52);

toggle(9189,L"原生变卖光标允许对手建筑（实验性）",extra.nativeSellCursor);next();
note(L"启用后请自行点击游戏原版 $ 变卖按钮，再指向对手建筑。只放宽所属方资格，仍检查建筑能否出售；由原版构造Sell事件并入队。联机需所属方保留已启用；与下面的特殊键鼠入口独立。",65);
item(L"STATIC",L"原生变卖光标：关闭",20,605,9190,0,44);next(44);
extraControls[SellEnemyEnabled]=toggle(722,L"变卖对手建筑（实验性）",extra.sellEnemy);next();
note(L"默认关闭；默认 Ctrl＋鼠标侧键 2，沿用“出售被吸取目标”的键鼠配置。仅对手建筑，请求入队不代表已变卖；联机效果待实测。",65);

toggle(788,L"优先执行增强指令",extra.overrideNative);next();
note(L"用于增强操作与普通地面命令冲突的情况；仅对支持的鼠标操作生效。",44);
item(L"STATIC",L"计算模式",20,605,832,0,30);next(30);
note(L"按功能选择工具箱计算或游戏内计算；修改后从下一局开始使用。",44);
for(int i=0;i<compute::Count;++i){if(i==compute::BurstRefresh)continue;label(compute::label(i),20,260);choices(combo(9000+i),{L"工具箱计算",L"游戏内计算"},(computeRequested>>i)&1u);next();}
item(L"STATIC",L"当前计算模式：等待对局",20,605,833,0,192);next(192);
label(L"移动指令兼容模式",20,260);megaQueueCombo=combo(761);choices(megaQueueCombo,{L"标准（推荐）",L"兼容模式（实验性）"},megaQueueMode);next();
label(L"维修与出售兼容模式",20,260);objectQueueCombo=combo(762);choices(objectQueueCombo,{L"标准（推荐）",L"兼容模式（实验性）"},objectQueueMode);next();
note(L"兼容模式需重启游戏和工具箱后生效。通常保持标准模式即可。",44);
// Retain the disabled compatibility control for old configuration handlers;
// it is not presented as an adjustable option.
schedulerEntryCombo=placedControl({-3,{0,0,1,1}},L"COMBOBOX",L"",760,CBS_DROPDOWNLIST);
choices(schedulerEntryCombo,{L"自动"},0);EnableWindow(schedulerEntryCombo,FALSE);ShowWindow(schedulerEntryCombo,SW_HIDE);


