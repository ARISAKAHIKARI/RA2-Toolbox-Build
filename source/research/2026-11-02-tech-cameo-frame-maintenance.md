# 原生科技側欄每幀補缺

問題：原生科技／側欄更新會移除工具箱加過的圖標，原來一秒全表刷新才補回，造成反覆消失。

本輪先重新檢索 GitHub：`ARISAKAHIKARI/RA2-Toolbox-Build` 的 82 個 releases 與 82 個 tag refs 均沒有可確認的 0.3.2；原始碼搜尋命中既有研究／評審記錄，提交搜尋 `0.3.2` 僅命中 `1.0.30.2` 的無關分詞。未找到可驗證 0.3.2 原包或原始碼。本地重新取得並 Git blob 核對的 `2026-10-22-tech-131.md` 為 `fb96b45e249a8fcd7a0813d90ff87f54b364988b`。該記錄只核實三個 **0.3.0** 歸檔，不能稱為 0.3.2。

能核實的早期 0.3.0 做法：兩處原生側欄 CanBuild CALL 包裝，FindFactory(true,false,true)，每1000ms遍歷允許清單並 AddCameo。新版原生面板資料方案改動指定類型的科技／前置資料，保持原生 CanBuild、FindFactory、側欄點擊生產；因此不是把早期程式碼補丁換個名稱。既有模式、允許清單及共用 Type 資料風險本輪維持。

新維護做法：每個模擬幀最多一次讀取四條 Strip 的 count 與完整條目到本機 `CameoSet`，同幀重複喚醒不重做。已存在圖標不跑 CanBuild、不跑 FindFactory、不重插。對缺失且允許的項目，重新執行該模式原有的原生合法性／工廠驗證，呼叫原生 AddCameo，由原生維持排序；**同一幀補全所有通過驗證的缺失項目，沒有八項或其他人為批次上限**。每條 Strip 原生容量仍為75。失敗候選250ms後重試，避免無工廠／上限已滿的項目每幀重試。

四條 Strip 快照短讀、count超過75、前後count不同時，本幀不補，避免把未知狀態當空清單重插。停止、功能關閉、無效會話、House更換、退場／戰敗等情況不補。切換PID／對局、類型清單或實作方式與幀倒退會清掉重試快取。大師模式繼續只觀察原生圖標。

原生位址／ABI／偏移沿用 [tech-131](2026-10-22-tech-131.md)：Sidebar `0x87F7E8`，Tabs `+0x1544`，Strip stride3988，count `+84`，entry `+88`／stride52／容量75；AddCameo `0x6A6300` thiscall(kind,index)，CanBuild `0x4F7870`，FindFactory 虛表 `+0x94`。本輪沒有新增原生函式或程式碼／虛表修改。既有原生資料方案支持映像 SHA-256 `247f72881e1a68c8fc305e3702dc0100a72d17601305999026393c340d5daeb0`；沒有讀寫正在執行的遊戲。

`assist/tests/tech_cameo_maintenance.py` 以實際 production `ensureTechCameo`、inventory、maintenance 程式碼執行有界世界測試，原生合法性、工廠與 AddCameo 為明示替身。0／1／8／75／150／300 個缺失，分別同幀補回相同數量；下一幀不重插、不再查合法性。另驗證250ms失敗退避、無工廠、短讀、容量75／四條共300、關閉／停止／退場／大師模式。

穩定300圖標、1000幀的該隔離fixture：每幀12次本機讀取、15632 bytes，0次CanBuild／FindFactory／AddCameo；CPU平均約2微秒／幀。這只測本機資料遍歷，不能表示原生CanBuild耗時或遊戲FPS。完整結果與來源雜湊在 `diagnostics/mo-render-followup/tech-cameos/result.json`。GitHub舊版檢索記錄在 `technology-legacy-search.json`。尚未作Windows實機或RA2OL雙機驗收。
