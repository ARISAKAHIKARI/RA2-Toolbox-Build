# MO 區域進樓、公平配額與可配置操作政策

本輪從已驗證的 Host／原生縮放整合 checkpoint 繼續，僅補操作側差異；沒有新增遊戲程式碼或虛表 Hook，沒有改寫 House、規則、單位任務或接收端事件。變更前產品源檔已另存工作區 baseline；可離線重現的必要舊源檔收在 `assist/tests/fixtures/mo_smartarea_baseline`，manifest 保存原檔與原完整 bridge SHA。此 checkpoint 以 `5983b943e90694f7b2630608bc464d6f8ee148e3` 為先前基礎，**不是聲稱舊 fixture 整體等於該 commit**。

上游本次唯讀核對至 [MO_ExperienceEnhancements `80850c7acf7d43d112975583b4fee5037c800263`](https://github.com/MagicShiba/MO_ExperienceEnhancements/tree/80850c7acf7d43d112975583b4fee5037c800263)。`ScaleBar`／`FullMap`／`PauseView`／`AirWay`／`PatrolCircle` 與先前固定 `33ed558...` 的內容相同；`SmartArea` 已新增本文針對的自家建築補令及吸收樓公平配額。先查閱研究索引、[MO 合併邊界](2026-11-02-mo-controls-scoped-merge.md)與[預覽連續性](2026-11-02-mo-operation-preview-followup.md)。Library SQLite 在此環境不可取得，新結論保存在 repo，未啟動或操作遊戲程序。

## Select／Move 與建築物件 Enter

參考 [SmartArea.cpp `SA_BldFix`（205–222）](https://github.com/MagicShiba/MO_ExperienceEnhancements/blob/80850c7acf7d43d112975583b4fee5037c800263/src/order/SmartArea.cpp#L205)。原生滑鼠可能對可進入的本家碉堡／吸收型建築回傳 Select／Move；這不表示應該送一個到該格子的 Move。上游以 Enter(7) 並帶**建築物件**作 Destination；其說明對應 `UnitClass::Mission_Enter(0x741970)` 和物件 ID 解碼 `0x40DD70`，本文沒有為這些地址新增呼叫或補丁。

`moBuildingEnterFallback()` 只接受原生 action 1／2、當前本家建築、對應既有型別旗標及 `GameView::entryCompatible()`。車輛只補 TankBunker／UnitAbsorb，非工程師步兵只補 InfantryAbsorb。工程師繼續走原生維修／佔領路徑；盟友、敵方、來源被載入／停用、未知型別或失讀不推測可進。沿用既有已核對的 `+16AB/+16AE/+16AF` 旗標與 GameView 容量讀取，沒有新增未驗證的結構欄位。

## 按真餘位輪轉，而非先各派四人

參考 [SmartArea.cpp `SA_GarrQuota`（827–873）](https://github.com/MagicShiba/MO_ExperienceEnhancements/blob/80850c7acf7d43d112975583b4fee5037c800263/src/order/SmartArea.cpp#L827)。舊分支先對每個目標分配四人，剩餘找最近目標；十名步兵對兩座同樣空的本家吸收樓，較近樓會拿到6、另一座4。現在中立民房預輪之後，對本家 InfantryAbsorb 每輪每樓分配一個兼容步兵，容量相同可得5／5；任何一樓滿了，剩餘才給另一樓。

每次准入按實際容量、已進入乘員使用尺寸、既有 pending reservation、本批已分配尺寸和目前來源 `entrySlots()` 計算。容量失讀或畸形拒絕。普通候選只對每個目標掃一次全體 pending；正在替換自身舊 reservation 的來源才用既有 ignore-ID 精確查詢，避免逐候選對反覆掃整個單位向量。

發送前再驗一次目標／來源 ID、指標、型別、Owner、手動版本及餘位。成功事件進入原生傳輸後，才加入既有 `entryReservations`；尚未執行到 Passengers 的事件也占預算，與原合作進樓通道共用。已有原生 destination 和同一 pending record 不重複計費。沿用該有界 reservation 池的失效／手動接管／180幀清理政策；這是本工具命令的保守准入，不是對其它玩家未執行命令的全域容量保證。

另發現選用多人高階 `0x646E90` backend 不經 `validateMoSubmission`。因此 `nativeMission()` 新增**預設 false** 的可選 `forceConstructed` 參數；只有本次有容量約束的 Enter 設 true，走既有原生 EventClass 構造與單機 DoList／多人 OutList 通道，保留 post-`gameSendTiming` 的最後檢查。沒有把 appendWaypoint 偷設成 true，也沒有改普通攻擊或其他功能的傳輸選項。原生已成功入隊的命令不重新發送。

## 七欄政策與捕獲一致性

參考 [FuzzyAttack.cpp 設定](https://github.com/MagicShiba/MO_ExperienceEnhancements/blob/80850c7acf7d43d112975583b4fee5037c800263/src/order/FuzzyAttack.cpp#L47)、[SmartArea.cpp 設定](https://github.com/MagicShiba/MO_ExperienceEnhancements/blob/80850c7acf7d43d112975583b4fee5037c800263/src/order/SmartArea.cpp#L34)與[PatrolCircle.cpp 設定及全邊長語義](https://github.com/MagicShiba/MO_ExperienceEnhancements/blob/80850c7acf7d43d112975583b4fee5037c800263/src/order/PatrolCircle.cpp#L74)。

| 欄位 | 預設／範圍 | 語義 |
| --- | --- | --- |
| range | 4／2..16 | 掃描方塊全邊長，非半徑；奇數採左側 floor、右側 ceil |
| squad | 4／2..16 | Fuzzy 分組及 Area 普通目標首輪人數；共用設定採 Fuzzy 下限2，上游 Area 個別設定原可為1 |
| patrolSize | 4／2..16 | 方環／長條全邊長，原生式整數 half=Size/2；奇數5得到 half2 |
| nodeStep | 2／1..8 | 巡邏採樣 Chebyshev 距離；末端仍強制保留 |
| loopClose | 3／2..16 | 首尾距離小於此值時採閉曲線 |
| loopStep | 3／2..16 | 閉曲線路點密度，仍受16節點上限 |
| commit | 1／0或1 | 工具箱是否自動發 House-wide PlanCommit |

`moorders::Options` 為七個 uint32、28 bytes，Assist92 在尾部追加並與手勢使用同一 `moGesturePolicySeq` 發布。Runtime 有效偶數快照同時讀按鍵和 Options；範圍非法、全零或序號碰撞均拒絕，不能把 fresh memset 零值當有效政策。捕獲保存整組 Options，中途改動取消未送尾部，不拿新參數解釋舊路徑。現有預設純幾何與先前 production 逐點一致；可配密度沿用本分支已驗證的弧長重採樣，不宣稱與上游離散節點抽取逐像素相同。

Commit=0 在驗證本批閉環後，不生成自動 Commit；使用者真實原生 Planning key-up 仍照原生語義返回。設定沒有權力吞掉使用者的原生提交，也不製造補償 Commit 或直接刪除原生圖。

## 可重跑驗證

從 `source` 目錄執行，Zig 使用0.13.0，輸出目錄需可寫；不需網路、diagnostics 基線或遊戲程序：

```text
python assist/tests/smartarea_capacity.py --zig <zig> --out <out>/capacity
python assist/tests/mo_options.py --zig <zig> --out <out>/options
```

- 容量：Windows x86／x64各3856 checks，588組人數／真餘位矩陣；編譯實際 mo_orders bridge、GameView 容量／相容判斷、entryReserved、nativeMission 路由。包含來源尺寸、失讀、預覽後填滿、時間回調中容量／Owner／UID／手動版本變動、pending不重計、真512槽滿池拒絕／同來源替換，以及普通高階 backend 預設保持。舊源檔各6 checks 確定重現補令缺失、6／4分配和高階 backend 繞過最後容量重驗三個反例。
- 政策：Windows x86／x64各3748 checks，320組舊版預設幾何逐點一致；真捕獲、odd／碰撞／全零拒絕、七欄改動取消、Range／Squad／Size／步長／閉合密度及 Commit0／1×真實原生 key-up 的四種狀態。
- 原有 MO bridge42項與 QuickFormation 回歸通過；操作預覽／Host 衝突保留另跑既有 followup。Host 舊微測已補實際 Transform helper 和零 inactive wire 欄位，沒有用 mock 繞過產品的新投影讀取。

報告 sources 都是相對 `source/` 的路徑至 SHA-256，編前／執行後 guard 必須 `changed_during_run=[]`。容量格式為 `results.current.{x86_64,x86}.{passed,checks}` 與 `results.baseline...`；政策格式為 `results.{x86_64,x86}.{passed,checks}`。證據見 [mo-smartarea-policy.json](evidence/mo-smartarea-policy.json)。

以上使用合成世界和受控 Win32／時間／queue 邊界；它們檢查產品函式與准入時序，不能證明真實引擎進樓、尋路到達、DirectDraw、RA2OL 或雙機同步已通過。未啟動、附加或控制使用者遊戲。
