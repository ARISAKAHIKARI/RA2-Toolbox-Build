// Included inside WM_CREATE; kept executable by the shipped geometry harness.
 pageControl(5,L"STATIC",L"自动装车分配",20,104,255,24,0);
 extraControls[LoadLogic]=pageControl(5,L"COMBOBOX",L"",20,134,255,160,700,CBS_DROPDOWNLIST|WS_TABSTOP);
 for(const auto*s:{L"就近优先",L"兵种均衡"})SendMessageW(extraControls[LoadLogic],CB_ADDSTRING,0,(LPARAM)s);
 SendMessageW(extraControls[LoadLogic],CB_SETCURSEL,extra.loadMode,0);
 pageControl(5,L"STATIC",L"范围 / 格",300,136,95,24,0);
 extraControls[LoadRadius]=pageControl(5,L"EDIT",std::to_wstring(extra.loadRadius).c_str(),403,134,80,26,701,ES_NUMBER|WS_TABSTOP);
 pageControl(5,L"STATIC",L"1–512",500,136,125,24,0);
 extraControls[EnterEnabled]=pageControl(5,L"BUTTON",L"高级进驻",20,175,205,26,702,BS_AUTOCHECKBOX|WS_TABSTOP);
 SendMessageW(extraControls[EnterEnabled],BM_SETCHECK,extra.enter?BST_CHECKED:BST_UNCHECKED,0);
 extraControls[EnterPreset]=pageControl(5,L"COMBOBOX",L"",240,175,105,160,703,CBS_DROPDOWNLIST|WS_TABSTOP);
 extraControls[EnterCustom]=pageControl(5,HOTKEY_CLASSW,L"",357,175,142,26,704,WS_TABSTOP);
 pageControl(5,L"STATIC",L"增强键进驻：留空直接点击，非空按住配置键；按容量分配同类目标。",20,211,605,24,0);
 extraControls[AirEnabled]=pageControl(5,L"BUTTON",L"飞机均匀攻击",20,247,205,26,705,BS_AUTOCHECKBOX|WS_TABSTOP);
 SendMessageW(extraControls[AirEnabled],BM_SETCHECK,extra.air?BST_CHECKED:BST_UNCHECKED,0);
 extraControls[AirPreset1]=pageControl(5,L"COMBOBOX",L"",240,247,105,160,706,CBS_DROPDOWNLIST|WS_TABSTOP);
 extraControls[AirCustom1]=pageControl(5,HOTKEY_CLASSW,L"",357,247,142,26,707,WS_TABSTOP);
 pageControl(5,L"STATIC",L"飞机均匀攻击 · 键 2",20,283,205,26,0);
 extraControls[AirPreset2]=pageControl(5,L"COMBOBOX",L"",240,283,105,160,708,CBS_DROPDOWNLIST|WS_TABSTOP);
 extraControls[AirCustom2]=pageControl(5,HOTKEY_CLASSW,L"",357,283,142,26,709,WS_TABSTOP);
 {const uint32_t binds[]={extra.enterKey,extra.airKey1,extra.airKey2};const int presets[]={EnterPreset,AirPreset1,AirPreset2},customs[]={EnterCustom,AirCustom1,AirCustom2};for(int i=0;i<3;++i){
  for(const auto*s:{L"仅鼠标",L"Alt",L"Ctrl",L"Shift",L"自定义 →"})SendMessageW(extraControls[presets[i]],CB_ADDSTRING,0,(LPARAM)s);
  SendMessageW(extraControls[presets[i]],CB_SETCURSEL,holdPreset(binds[i]),0);SendMessageW(extraControls[customs[i]],HKM_SETHOTKEY,binds[i],0);
 }}
 pageControl(5,L"STATIC",L"默认优先 1",20,323,105,24,0);extraControls[DefaultFirst]=pageControl(5,L"EDIT",std::to_wstring(extra.defaults.first).c_str(),130,321,65,26,710,ES_NUMBER|WS_TABSTOP);
 pageControl(5,L"STATIC",L"默认优先 2",227,323,105,24,0);extraControls[DefaultSecond]=pageControl(5,L"EDIT",std::to_wstring(extra.defaults.second).c_str(),338,321,65,26,711,ES_NUMBER|WS_TABSTOP);
 pageControl(5,L"STATIC",L"最大数",440,323,105,24,0);extraControls[DefaultMaximum]=pageControl(5,L"EDIT",std::to_wstring(extra.defaults.maximum).c_str(),550,321,75,26,712,ES_NUMBER|WS_TABSTOP);
 extraControls[QuotaType]=pageControl(5,L"COMBOBOX",L"",20,359,270,240,713,CBS_DROPDOWNLIST|WS_TABSTOP);
 extraControls[QuotaID]=pageControl(5,L"EDIT",L"",305,359,185,26,714,ES_AUTOHSCROLL|WS_TABSTOP);
 pageControl(5,L"BUTTON",L"读取 ID",505,359,120,26,751,BS_PUSHBUTTON|WS_TABSTOP);
 extraControls[QuotaOverride]=pageControl(5,L"BUTTON",L"此类型单独设置",20,397,175,26,715,BS_AUTOCHECKBOX|WS_TABSTOP);
 pageControl(5,L"STATIC",L"优先 1",208,399,57,24,0);extraControls[QuotaFirst]=pageControl(5,L"EDIT",L"1",270,397,50,26,716,ES_NUMBER|WS_TABSTOP);
 pageControl(5,L"STATIC",L"优先 2",340,399,57,24,0);extraControls[QuotaSecond]=pageControl(5,L"EDIT",L"2",402,397,50,26,717,ES_NUMBER|WS_TABSTOP);
 pageControl(5,L"STATIC",L"最大",480,399,57,24,0);extraControls[QuotaMaximum]=pageControl(5,L"EDIT",L"0",542,397,83,26,718,ES_NUMBER|WS_TABSTOP);
 pageControl(5,L"STATIC",L"自动保存",20,436,270,27,752,0);
 pageControl(5,L"STATIC",L"自动保存",305,436,320,27,750,0);
 pageControl(5,L"STATIC",L"最大数 0 表示不限；优先数 0 表示不优先。只重分配敌方目标。",20,478,605,24,0);
 pageControl(5,L"STATIC",L"空键直接按增强鼠标键；飞机两键均空时用模式 1。明确按住的键优先。",20,511,605,24,0);
 {const wchar_t*names[]={L"飞碟主武器",L"变卖吸取目标",L"变卖战车（兰博玩不支持）",L"变卖敌方建筑（停用）"};bool settings[]={extra.disk,extra.sellDrained,extra.sellVehicles,extra.sellEnemy};const int slots[]={DiskEnabled,SellDrainedEnabled,SellVehiclesEnabled,SellEnemyEnabled};
  for(int i=0;i<4;++i){extraControls[slots[i]]=pageControl(6,L"BUTTON",names[i],20,110+i*36,205,28,719+i,BS_AUTOCHECKBOX|WS_TABSTOP);SendMessageW(extraControls[slots[i]],BM_SETCHECK,settings[i]?BST_CHECKED:BST_UNCHECKED,0);}
 }
 pageControl(6,L"STATIC",L"飞碟：对建筑所在格发送原版强制攻击，使用主武器。",20,263,605,24,0);
 pageControl(6,L"STATIC",L"变卖：配置键+变卖鼠标键；留空直接点击，只支持合法己方目标。",20,293,605,24,0);
 pageControl(6,L"STATIC",L"自动保存",20,366,605,28,754,0);
 {const uint32_t binds[]={extra.diskKey,extra.sellKey};const int presets[]={DiskPreset,SellPreset},customs[]={DiskCustom,SellCustom};
  const wchar_t* names[]={L"飞碟按住键",L"变卖按住键"};
  for(int i=0;i<2;++i){int y=110+i*36;
   extraControls[presets[i]]=pageControl(6,L"COMBOBOX",L"",240,y,105,160,700+presets[i],CBS_DROPDOWNLIST|WS_TABSTOP);
   extraControls[customs[i]]=pageControl(6,HOTKEY_CLASSW,L"",357,y,142,26,700+customs[i],WS_TABSTOP);
   for(const auto*s:{L"仅鼠标",L"Alt",L"Ctrl",L"Shift",L"自定义 →"})SendMessageW(extraControls[presets[i]],CB_ADDSTRING,0,(LPARAM)s);
   SendMessageW(extraControls[presets[i]],CB_SETCURSEL,holdPreset(binds[i]),0);
   SendMessageW(extraControls[customs[i]],HKM_SETHOTKEY,binds[i],0);
  }
 }
 auto mouseChoice=[&](int slot,int page,int y,int x=507,int width=118){
  auto h=extraControls[MouseFirst+slot]=pageControl(page,L"COMBOBOX",L"",x,y,width,160,729+slot,CBS_DROPDOWNLIST|WS_TABSTOP);
  for(auto name:{L"停用",L"中键",L"侧键 1",L"侧键 2",L"右键",L"左键"})SendMessageW(h,CB_ADDSTRING,0,(LPARAM)name);
  SendMessageW(h,CB_SETCURSEL,extra.mouseButtons[slot]==6?5:extra.mouseButtons[slot],0);
 };
 mouseChoice(pointerinput::MouseAdvanced,2,108,497,128);
 mouseChoice(pointerinput::MouseMagnetron,2,342,497,128);
 mouseChoice(pointerinput::MouseCS,1,326,497,128);
 mouseChoice(pointerinput::MouseEnter,5,175);
 mouseChoice(pointerinput::MouseAirFirst,5,247);
 mouseChoice(pointerinput::MouseAirSecond,5,283);
 mouseChoice(pointerinput::MouseDisk,6,110);
 mouseChoice(pointerinput::MouseSellDrained,6,146);
 auto saleKey=pageControl(6,HOTKEY_CLASSW,L"",357,182,142,26,789,WS_TABSTOP);
 SendMessageW(saleKey,HKM_SETHOTKEY,extra.sellKey,0);modernui::mouseOnly(saleKey);
 mouseChoice(pointerinput::MouseSellVehicle,6,182);
 pageControl(2,L"STATIC",L"直接攻击移动（不持续索敌）",20,448,470,26,0);mouseChoice(pointerinput::MouseAttack,2,448,497,128);
 pageControl(2,L"STATIC",L"高级移动索敌 / 秒",20,484,270,26,0);
 pageControl(2,L"EDIT",std::to_wstring(advancedScanHz).c_str(),305,484,180,26,787,ES_NUMBER|WS_TABSTOP);
 pageControl(2,L"STATIC",L"1–60 次",497,484,128,26,0);
 {auto h=pageControl(2,L"BUTTON",L"覆盖原生指令",20,560,605,26,788,BS_AUTOCHECKBOX|WS_TABSTOP);SendMessageW(h,BM_SETCHECK,extra.overrideNative?BST_CHECKED:BST_UNCHECKED,0);}
 pageControl(2,L"STATIC",L"默认先原版后追加；覆盖仅尝试屏蔽兼容的地面命令点击。",20,596,605,24,0);
 for(int slot:{EnterCustom,AirCustom1,AirCustom2,DiskCustom,SellCustom})modernui::mouseOnly(extraControls[slot]);
 extraControls[FrozenEnabled]=pageControl(3,L"BUTTON",L"冻结倒计时",20,366,605,26,723,BS_AUTOCHECKBOX|WS_TABSTOP);SendMessageW(extraControls[FrozenEnabled],BM_SETCHECK,extra.frozen?BST_CHECKED:BST_UNCHECKED,0);
 extraControls[EnemyMarkersEnabled]=pageControl(3,L"BUTTON",L"敌方位置标记（包含未揭示区域）",20,526,605,26,785,BS_AUTOCHECKBOX|WS_TABSTOP);
 SendMessageW(extraControls[EnemyMarkersEnabled],BM_SETCHECK,extra.enemyMarkers?BST_CHECKED:BST_UNCHECKED,0);
 pageControl(3,L"STATIC",L"显示画面内敌方单位 / 建筑，按玩家着色；共用上方刷新率。",20,564,605,24,0);
 pageControl(1,L"STATIC",L"按当前武器选同类变体",20,404,340,26,0);
 extraControls[VariantKey]=pageControl(1,HOTKEY_CLASSW,L"",374,404,153,26,724,WS_TABSTOP);SendMessageW(extraControls[VariantKey],HKM_SETHOTKEY,extra.variantKey,0);
 pageControl(1,L"STATIC",L"自动保存",541,404,84,27,755,0);
 pageControl(1,L"STATIC",L"单按选屏幕，双按选全图；只看普通武器，忽略精英武器。",20,444,605,24,0);

 EnableWindow(extraControls[SellEnemyEnabled],FALSE);

 extraControls[RevealExperimental]=pageControl(6,L"BUTTON",L"启用地图全开（实验性）",20,1060,605,26,739,BS_AUTOCHECKBOX|WS_TABSTOP);
 SendMessageW(extraControls[RevealExperimental],BM_SETCHECK,extra.experimentalReveal?BST_CHECKED:BST_UNCHECKED,0);

 {auto all=pageControl(6,L"BUTTON",L"热键冲突时全部执行",20,1100,605,26,786,BS_AUTOCHECKBOX|WS_TABSTOP);
 SendMessageW(all,BM_SETCHECK,extra.allHotkeys?BST_CHECKED:BST_UNCHECKED,0);}
 pageControl(6,L"STATIC",L"键盘及增强鼠标匹配项依次提交；同一单位的后续命令可能覆盖前项。",20,1138,605,24,0);


 {auto h=pageControl(0,L"BUTTON",L"Shift 五连点",20,1000,270,26,795,BS_AUTOCHECKBOX|WS_TABSTOP);SendMessageW(h,BM_SETCHECK,shiftClickEnabled?BST_CHECKED:BST_UNCHECKED,0);}
 auto clickMode=pageControl(0,L"COMBOBOX",L"",305,1000,320,160,796,CBS_DROPDOWNLIST|WS_TABSTOP);
 for(auto text:{L"自动（Ares 使用原生五连点）",L"强制补四次点击",L"关闭补点"})SendMessageW(clickMode,CB_ADDSTRING,0,(LPARAM)text);
 SendMessageW(clickMode,CB_SETCURSEL,shiftClickMode,0);
 pageControl(0,L"STATIC",L"补点间隔 / ms",20,1036,270,26,0);pageControl(0,L"EDIT",std::to_wstring(shiftClickGap).c_str(),305,1036,180,26,797,ES_NUMBER|WS_TABSTOP);
 pageControl(0,L"STATIC",L"0–250",497,1036,128,26,0);

 {auto h=pageControl(3,L"BUTTON",L"空闲飞机指示器（小地图左侧）",20,604,605,26,798,BS_AUTOCHECKBOX|WS_TABSTOP);SendMessageW(h,BM_SETCHECK,extra.idleAircraft?BST_CHECKED:BST_UNCHECKED,0);}

 {auto h=pageControl(0,L"BUTTON",L"建筑 / 防御各预定 5 个（点图标，无需热键）",20,1076,605,26,799,BS_AUTOCHECKBOX|WS_TABSTOP);SendMessageW(h,BM_SETCHECK,extra.constructionQueue?BST_CHECKED:BST_UNCHECKED,0);}

 pageControl(2,L"STATIC",L"高级 CS 索敌额外格数",20,556,270,26,0);
 pageControl(2,L"EDIT",std::to_wstring(advancedRangeCells).c_str(),305,556,180,26,818,ES_NUMBER|WS_TABSTOP);
 pageControl(2,L"STATIC",L"0–30 格 · 默认 2",497,556,128,26,0);
 pageControl(2,L"STATIC",L"高级移动索敌优先",20,520,270,26,0);
 {auto h=pageControl(2,L"COMBOBOX",L"",305,520,320,150,802,CBS_DROPDOWNLIST|WS_TABSTOP);for(auto text:{L"单位价值优先",L"弹头伤害比例优先"})SendMessageW(h,CB_ADDSTRING,0,(LPARAM)text);SendMessageW(h,CB_SETCURSEL,advancedTargetPriority,0);}

 {auto h=pageControl(2,L"BUTTON",L"飞机 Z 路径点（外部队列；原生模式下暂停）",20,636,605,26,803,BS_AUTOCHECKBOX|WS_TABSTOP);SendMessageW(h,BM_SETCHECK,extra.airWaypoints?BST_CHECKED:BST_UNCHECKED,0);}

 {auto h=pageControl(0,L"BUTTON",L"自动防偷点杀（防御建筑名单可配置）",20,1112,605,26,804,BS_AUTOCHECKBOX|WS_TABSTOP);SendMessageW(h,BM_SETCHECK,extra.antiTheft?BST_CHECKED:BST_UNCHECKED,0);}
