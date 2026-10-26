# 普通 CnCNet、無額外反作弊前提下的掉線排查

日期：2026-10-27（UTC+8）。

## 問題、前提與證據邊界

使用者未找到 CnCNet-Spawner.dll，提出現場可能是普通 game-spawn，並指出 Hook 不一定改程式碼段。本輪依「沒有額外反作弊」的條件重新排序原因；這不是對現場 build 的辨識結論。其他工具能注入或運作，也不足以辨識現場版本。

產品基線仍是 pre.6（4f7248771754318706b5fd3cbd4ac63bf93ed8d7），未更改產品。原版機制沿用已分析 EXE SHA-256 `7b8a068535d6af06845edf95ae829b113d00c02909330e16f197426cd7db94b6`；本輪不新增可執行檔支援聲明，沒有實際遊戲／Windows／雙機執行。

證據分兩部分：前輪源碼、原版受控案例、使用者 SYNC 的重新整理；新增的固定上游來源與普通 v17 PE 描述表交叉核對。後者沒有追出新的完整遊戲因果鏈。

## 1. 不需要額外反作弊的同步失敗

原版 64DAB0 摘要涵蓋位置／朝向、House+241 MapIsClear、顯示層／Logic 列表與 Scenario RNG 輸出。它不是額外的主程式碼區 CRC；「有 Hook」「改了 .text」本身並非原版不同步的完整因果。

使用者先前兩份透明／半透明 SYNC 確認 FrameInfo 摘要失配，無法以單純卡頓解釋。但兩份來自不同對局，不能做雙端欄位差分。報錯時某個 House 的 Visionary/MapIsClear=1，也不等於兩端同一 House 不一致；來源、修改時刻、對端值均待核。

參考：[原版事件與摘要](2026-10-26-network-event-audit.md)、[兩份原版 SYNC](2026-10-26-shroud-sync001-logs.md)。

## 2. 控制全部：既有風險與新版差異分開

- ControlAll 的 50B6F0 共用 predicate 恒回 1，6339BA/637E40 使用本端 CurrentPlayer 判斷。它們是常駐控制補丁，並非僅在點擊期間有效。若同步模擬路徑經過被改變的共用判斷，便可能讓同一事件在兩端產生不同結果；尚未定位故障對局的具體分歧 caller。這個設計在 1.0.37 已有，不是新回歸證據。
- 1.0.37→1.0.38 的 24 個控制相關檔案相同；共用 runtime／entry manager 至 pre.6 亦相同。沒有證據說新版更換了 Hook 執行緒。WH_GETMESSAGE 不等於邏輯幀邊界，但這個既有事實不足以證明回歸。
- pre.6 新增 NativeMegaGateway，預設 1，在 646F4E 成功入 OutList 後仲裁；兩個所屬方選項關閉不會自動關閉它。有限 ABI／事件／寫入測試未見正常路徑額外改模擬狀態，應作新增變數隔離，不能直接判定是根因。
- NetworkHousePreserve=1 且實際發送外方 House 時，已受控重現同 House 多發送者的 DoList 順序差異及壓縮字典碰撞。若 NetworkHousePreserve=0、ControlCommandOwner=0，不能用這兩個缺陷解釋該場景。

參考：[控制回歸審計](2026-10-26-control-138-regression-audit.md)。

## 3. 地圖模式不能混為同一條路徑

| 模式 | 已確認行為 | 無額外反作弊時的意義 |
| --- | --- | --- |
| 原生揭示／策略3 | 呼叫 577D90，寫 House+240/+241 及 Cell 揭示狀態 | 原生函式不等於網路事件；若同一 House 的狀態只在本端改變，足以改變原版摘要 |
| 直接全圖／策略4 | 五處可見性／雷達補丁；放寬 SpySat 判斷有條件通往 Reveal | 不能把整組功能當作僅改顯示；實際是否走到該路徑仍待執行證據 |
| 透明／半透明／策略2、1 | 47F038/47F274 後段繪圖 Hook，跳過遮罩或寫 alpha | 現有源碼與有限葉測試未見直接寫 MapIsClear、探索或 RNG，根因仍未定位 |

同局先揭示再切換繪圖模式，不會還原探索狀態；這是需要排除的歷史影響，並非已確認使用者如此操作。半透明逐像素管理器開銷有量測，但沒有實機耗時，不能解釋透明亦出現的明確摘要失配。

參考：[迷霧審計](2026-10-26-shroud-network-audit.md)、[修改大師與揭示路徑](2026-10-26-trainer-map-cheatcheck.md)。

## 4. 新增核對：普通 CnCNet 本身也處理可見性與浮點狀態的不同步

固定官方來源：

- 倉庫 CnCNet/yrpp-spawner，commit `fbe8d7d2620584d04ba14fa3580943bfa1448d40`。
- 路徑 `src/Misc/Bugfixes.Desyncs.cpp`，Git blob `8ee8797aa94a5f2f5d16a65c0d49e2fb5ce7b7f3`。
- 普通 v17 DLL SHA-256 `58f198973bd0fd3b0e1b2c42fc6c545efe1fd399680abc2e8d6e9e189751614d`。
- Hardened v17 DLL SHA-256 `62466e9e534ceb15c5d71fb05c62ad98a2b2b4903c569ed0c82189ef86ea3b7d`。

### 4.1 可見性相關副作用

上游來源註解描述：飛機陰影繪製暫時 Set_Height，經 Place_Down/Pick_Up 觸發地塊 RecalcAttributes；原有 Overlay 清除路徑可能留下過時屬性，只有能看見飛機的一端重新計算，造成 LandType／後續尋路差異。來源用三個 CALL6 補丁將陰影內 Set_Height 呼叫改接已知空函式。

來源另在 TechnoClass::DrawBehind 對 Invisible 類型跳過一條受本端可見性影響的動畫建立路徑。

這是官方來源對引擎缺陷的解釋，不是本輪重新執行原版陰影函式後取得的結果。透明／半透明修改 alpha 是否改變這些函式的到達條件，本輪沒有證據；不能從「迷霧相關」直接推出因果。

### 4.2 浮點環境

上游另描述特定顯示驅動在遊戲執行緒建立裝置時改壞 MXCSR，導致使用 SSE 的擴展解析／計算不同，並在顯示模式設定後及邏輯幀入口修復浮點狀態。來源特別區分原版 x87 與擴展 SSE 的影響；本輪未重現顯卡問題，也沒有檢查使用者機器的 MXCSR。

### 4.3 實際普通 DLL 描述表核對

根代理與子代理分別讀既有 static-metadata.json。下列項目在普通及 Hardened v17 都存在：

| 類型 | 遊戲地址／長度 | 描述符或 file-backed patch bytes |
| --- | --- | --- |
| Patch | 4147D5 / 6 | E8 26 FB 1D 00 90 |
| Patch | 4147F3 / 6 | E8 08 FB 1D 00 90 |
| Patch | 4148AB / 6 | E8 50 FA 1D 00 90 |
| Hook | 70F1E3 / 8 | TechnoClass_DrawBehind_InvisibleDesyncFix |
| Hook | 4A4420 / 9 | SetVideoMode_RepairFPState |
| Hook | 55B4E1 / 5 | LogicClass_Update_RepairFPState |

三組 patch 均為 CALL 5F4300 + NOP。三個 Hook 本輪只核對描述符，沒有另追初始化器、helper 實碼或真實啟用狀態。壓縮證據見 [普通版不同步修復描述表](evidence/cncnet-v17-ordinary-desync-fixes.json)。

已分析的普通 v17 因此帶有上述修復項；不能把這些「原版已知缺陷」直接當成該固定版本的未修問題。使用者現場檔案仍未知，不能斷言有或沒有這些修復。其用途是證明一般可見性／浮點副作用是獨立於額外反作弊的機制。

同份來源的 5C0E30/2 = B0 01（特殊格黑幕檢查修復）早已在兩個 v17 樣本確認，因此也不能將舊特殊格檢查當成這兩個樣本的一般根因。

## 5. 普通補丁相容性與下一個可區分的對照

普通平台依然可能修改遊戲函式。既有自身八個控制／Owner／Gateway 區間互不重疊；未知入口常先拒絕安裝。沒有現場 bytes，不能說已發生覆寫衝突。既有 registry 仍有部分恢復與 UI 狀態掩蓋失敗的診斷缺口，所以重啟程序的對照比只取消勾選可靠。

控制回歸最小對照：從新程序、新對局開始，保持 ControlAll=1，ControlCommandOwner=0、NetworkHousePreserve=0，單獨比較 NativeMegaGateway=0/1；另保留舊／新 DLL 同配置對照。這只是隔離新增路徑，不代表整個產品還原至1.0.37，也不是已驗證修復。

地圖對照：新對局只開透明，另一新對局只開半透明；同一次失敗取雙端 SYNC。先區分 MapIsClear、RNG、物件列表／位置、事件內容／次序，再追第一個分歧的寫入。直接閃退另查 ABI／指標／補丁，單純卡頓斷線另查主執行緒與網路耗時。

參考：[Hook 衝突與恢復審計](2026-10-26-hook-conflicts-cncnet-checks.md)。

## 官方來源

[Bugfixes.Desyncs.cpp 固定版本](https://github.com/CnCNet/yrpp-spawner/blob/fbe8d7d2620584d04ba14fa3580943bfa1448d40/src/Misc/Bugfixes.Desyncs.cpp)
