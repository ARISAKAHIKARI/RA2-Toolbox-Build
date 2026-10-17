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
 extraControls[EnterCustom]=pageControl(5,HOTKEY_CLASSW,L"",357,175,268,26,704,WS_TABSTOP);
 pageControl(5,L"STATIC",L"增强键进驻：留空直接点击，非空按住配置键；按容量分配同类目标。",20,211,605,24,0);
 extraControls[AirEnabled]=pageControl(5,L"BUTTON",L"飞机均匀攻击",20,247,205,26,705,BS_AUTOCHECKBOX|WS_TABSTOP);
 SendMessageW(extraControls[AirEnabled],BM_SETCHECK,extra.air?BST_CHECKED:BST_UNCHECKED,0);
 extraControls[AirPreset1]=pageControl(5,L"COMBOBOX",L"",240,247,105,160,706,CBS_DROPDOWNLIST|WS_TABSTOP);
 extraControls[AirCustom1]=pageControl(5,HOTKEY_CLASSW,L"",357,247,268,26,707,WS_TABSTOP);
 pageControl(5,L"STATIC",L"飞机均匀攻击 · 键 2",20,283,205,26,0);
 extraControls[AirPreset2]=pageControl(5,L"COMBOBOX",L"",240,283,105,160,708,CBS_DROPDOWNLIST|WS_TABSTOP);
 extraControls[AirCustom2]=pageControl(5,HOTKEY_CLASSW,L"",357,283,268,26,709,WS_TABSTOP);
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
 {const wchar_t*names[]={L"飞碟主武器",L"变卖吸取目标",L"变卖战车",L"变卖敌方建筑（停用）"};bool settings[]={extra.disk,extra.sellDrained,extra.sellVehicles,extra.sellEnemy};const int slots[]={DiskEnabled,SellDrainedEnabled,SellVehiclesEnabled,SellEnemyEnabled};
  for(int i=0;i<4;++i){extraControls[slots[i]]=pageControl(6,L"BUTTON",names[i],20,110+i*36,605,28,719+i,BS_AUTOCHECKBOX|WS_TABSTOP);SendMessageW(extraControls[slots[i]],BM_SETCHECK,settings[i]?BST_CHECKED:BST_UNCHECKED,0);}
 }
 pageControl(6,L"STATIC",L"飞碟：对建筑所在格发送原版强制攻击，使用主武器。",20,263,605,24,0);
 pageControl(6,L"STATIC",L"变卖：配置键+变卖鼠标键；留空直接点击，只支持合法己方目标。",20,293,605,24,0);
 pageControl(6,L"STATIC",L"车辆须与建筑重合；售款归原所属方。详见使用说明。",20,323,605,24,0);
 pageControl(6,L"STATIC",L"自动保存",20,366,605,28,754,0);
 {const uint32_t binds[]={extra.diskKey,extra.sellKey};const int presets[]={DiskPreset,SellPreset},customs[]={DiskCustom,SellCustom};
  const wchar_t* names[]={L"飞碟按住键",L"变卖按住键"};
  for(int i=0;i<2;++i){int y=550+i*36;
   pageControl(6,L"STATIC",names[i],20,y,205,26,0);
   extraControls[presets[i]]=pageControl(6,L"COMBOBOX",L"",240,y,105,160,700+presets[i],CBS_DROPDOWNLIST|WS_TABSTOP);
   extraControls[customs[i]]=pageControl(6,HOTKEY_CLASSW,L"",357,y,268,26,700+customs[i],WS_TABSTOP);
   for(const auto*s:{L"仅鼠标",L"Alt",L"Ctrl",L"Shift",L"自定义 →"})SendMessageW(extraControls[presets[i]],CB_ADDSTRING,0,(LPARAM)s);
   SendMessageW(extraControls[presets[i]],CB_SETCURSEL,holdPreset(binds[i]),0);
   SendMessageW(extraControls[customs[i]],HKM_SETHOTKEY,binds[i],0);
  }
 }
 {const wchar_t*names[]={L"高级移动鼠标键",L"磁电轮换鼠标键",L"按键 CS 鼠标键",L"高级进驻鼠标键",L"飞机模式 1 鼠标键",L"飞机模式 2 鼠标键",L"飞碟主武器鼠标键",L"直接攻击移动鼠标键",L"变卖吸取目标鼠标键",L"变卖战车鼠标键"};
  for(int i=0;i<pointerinput::MouseCount;++i){int y=650+i*36;
   pageControl(6,L"STATIC",names[i],20,y,305,26,0);
   auto h=extraControls[MouseFirst+i]=pageControl(6,L"COMBOBOX",L"",345,y,280,160,729+i,CBS_DROPDOWNLIST|WS_TABSTOP);
   for(const auto*s:{L"停用",L"中键",L"侧键 1",L"侧键 2",L"右键"})SendMessageW(h,CB_ADDSTRING,0,(LPARAM)s);
   SendMessageW(h,CB_SETCURSEL,extra.mouseButtons[i],0);
  }
 }
 pageControl(6,L"STATIC",L"每项独立设置并自动保存；沿用现有冲突规则，右键保留原版点击。",20,1020,605,24,0);
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
