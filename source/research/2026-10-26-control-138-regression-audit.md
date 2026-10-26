# 跨玩家控制：1.0.38 與 pre.6 回歸審計

問題：使用者回報 1.0.38 正式版控制全部敵方單位不斷線，本次版本會斷線；之後補充未開「命令使用單位所屬方」。這是回歸觀察，不能用舊版本來就存在的風險替代新舊比較，也不能把需要額外開關的模型直接當成使用者根因。

基線為 GitHub v1.0.38，commit `9c6509a2a6cf88f95af14834acbd90d0e2d36da1`；受查產品為 1.0.39-pre.6，產品 commit `4f7248771754318706b5fd3cbd4ac63bf93ed8d7`，最終 DLL SHA-256 `24c405af124ec60c88b92c47b0cbaca9ab02ba1ed7530f5631f442520f43a08c`。原版 EXE SHA-256 `7b8a068535d6af06845edf95ae829b113d00c02909330e16f197426cd7db94b6`；私有映像重建 PE fixture SHA-256 `2fcb478cb2e0de00b1af1a6c82e9b55e56e0b555f20f3c0a018e8aaddc58b0c8`。不提交私有程式或映像。本輪未修改產品、未重新編譯，也沒有真實 Windows／雙機聯機測試。

## 已核對的新舊差異

| 項目 | 1.0.38 → pre.6 |
| --- | --- |
| ControlAll 核心 | `50B6F0` 返回1；`6339BA/637E40` 嚴格比較 CurrentHouse。三個 thunk、站點表、整組安裝恢復、原始簽名和上下文檢查均相同。 |
| ControlAll 配置 | 預設0，沿用 Experimental/ControlAll，無隱式開啟其他新項。 |
| ControlCommandOwner | 新增，預設0；四處原生構造／准入適配，按實際來源單位 Owner 處理。 |
| NetworkHousePreserve | 新增，預設0；五處發送／壓縮 hook，保留已填入事件的 House。與 ControlCommandOwner 獨立。 |
| NativeMegaGateway | 新增，預設1；`646F4E` 成功入 OutList 後執行工具仲裁。即使 Owner 選項關閉，這個 hook 仍可安裝。 |

來源：`assist/source/placement_bridge.h:337`、`:426`、`:493`；`additional_settings.h`、`extra_settings_io.h:4`、`:6`、`:7`；`native_mega_gateway_bridge.h`；`control_command_owner_bridge.h`；`network_sender_bridge.h`。

`AssistRiskAction(op=6)` 雖新增 Owner Ready 條件，但三個裸 control thunk 並不呼叫它，不能把這段條件說成每次控制判定均執行。原版共用 ControlledByPlayer 確有非 UI 使用者，但這些覆蓋面並非此次新增。

## 已重現的獨立缺陷：多發送者共用 Event.House

原版 `64C380` 按 House 陣列次序分派該幀事件，再依本端 DoList 插入次序掃描同 House 的事件；沒有用真實發送節點作第二排序鍵。

原版允許各端先插入自己的事件、再插入收到的事件：協議0在 `649E7A/649EAE`、壓縮協議在 `64BA38/64BA6A` 複製並增加本機 DoList，之後才於 `649FA1` 呼叫傳輸 Send；遠端 `64C2A5/64C2EA` 往當前尾端追加。

受控測試使用兩個有效 House、同一幀1234、同一來源單位 ID101、兩條 Move=2，目的地分別100／101。各端以「本機先、遠端後」入列，完整執行原版調度器，只在 Respond 入口記錄事件後返回：

| 事件標頭 House | 端0 Respond 次序 | 端1 Respond 次序 |
| --- | --- | --- |
| 各自發送者0／1（舊發送規則） | Move100 → Move101 | Move100 → Move101 |
| 都保留單位 Owner=1 | Move100 → Move101 | Move101 → Move100 |

4/4 PASS，見 `assist/tests/same_house_dolist_order_capture.py` 與 `research/evidence/same-house-dolist-order-native.json`。這證明順序不一致可以在原版調度層發生；未執行任務效果、後續世界更新或真實網路，不能宣稱已量到真實 CRC 分歧。

此前 pre.6 的逐包編解碼／接收計數測試沒有涵蓋此條件。兩端事件欄位一致不充分；跨發送者事件也必須以一致次序執行。這是確定的設計缺口，但只有實際保留外方 House 時才適用。

## Owner 關閉時的範圍

普通原生 QueueMegaMission `646E90` 在 `646EE3/646EE8` 直接從 CurrentHouse 取索引，F 路徑 `646FC0..646FC8`、ClickedEvent `6FFE1B/6FFE2B` 同理。雖然 `6339B0` 在既有控制補丁下可返回實際 OwnerIndex，不能把所有原生構造者都假定使用它。

最終 DLL 既有用例 `independent_option_off_preserves_original_kind_4/5` 定向重跑2/2：ControlAll=1、NetworkHousePreserve=1、ControlCommandOwner=0，敵方 actor 的普通原生 Mega/F 仍填本方 House。報告本地 `tmp/control-owner-off-audit.json`。

但 Alt 工具 PlayerOrder 在1.0.38就會先填實際 Owner（`pointer_bridge.h`、`nativeMission` 的 PlayerTest 分支）；舊發送器之後會改回本方。若只開新的 NetworkHousePreserve，這些既有外方 House 就不再被改回，因此 **Owner 關閉不代表網路行為完全回到1.0.38**。

若 NetworkHousePreserve 和 ControlCommandOwner 從未開啟，不能用上述多發送者同 House 缺陷解釋斷線。使用者已確認 Owner 關閉，尚未確認 Network 選項及出錯操作的精確狀態。

## 預設仲裁的定向寫入檢查

對最終 DLL 執行 ControlAll=1、Owner=0、Network=0、敵方 actor 的完整原生 `646E90`，分別開／關 NativeMegaGateway。兩例均通過：111位元組事件、全部通用暫存器／旗標／x87／SSE及遊戲寫入序列一致；均只有原版 OutList 的31次寫入與時間戳1次寫入。完整遊戲映像未發現額外變化，合成4MB遊戲 heap不變。開啟仲裁時多出的寫入只在工具 DLL、Shared skipped計數、堆疊及SEH鏈。

證據本地 `tmp/mega_foreign_write_review.py`、`tmp/mega_foreign_write_review.json`。此正常葉路徑中敵方事件會被仲裁跳過，未發現直接改本機模擬的依據。它沒有涵蓋真實 Windows SEH、平台附加檢查、長對局物件生命週期、收包與任務執行，因此不能把兩例通過當成實機穩定證明，也不能宣稱默認仲裁已是根因。

## 其他新增差異及隔離方法

Owner 開啟時，`placementTick` 的 ownerGuardReady 會要求 foreground、非聊天及四處 Owner hook 已就緒。8狀態受控檢查確認：失焦／聊天會恢復三處共用控制補丁，回前台重裝；Owner 關閉時不存在這個新門檻。證據本地 `tmp/control_toggle_review.py/json`。這是時序差異，未證明造成斷線，而且不符合使用者目前確認的 Owner 關閉條件。

隔離新增控制路徑需保持 ControlAll=1，並令 Experimental 下 ControlCommandOwner=0、NetworkHousePreserve=0、NativeMegaGateway=0；不應只關 ControlAll 或只關 Owner 就宣稱全部新增 hook 已撤除。對照應從新的遊戲程序／對局開始，避開先前已發送事件與待恢復補丁的歷史狀態。此組合只隔離控制／發送／仲裁相關新路徑，不代表其他新功能也回到1.0.38。

## 兩項所屬方均關閉的發送對照

收到使用者補充後，對發布 DLL 補做 protocol0／1／2 × 普通原生敵方 Move／工具 Alt 敵方 Move，共6例；每例比較 NativeMegaGateway 關／開。原構造器10個參數、完整111位元組 OutList及時間戳、發送後本機 DoList、wire位元組與遠端原版解碼均一致；入隊前DoList為0、發送站點保留原字節，敵方物件與House記憶體沒有改動。

普通原生構造時House0，Alt工具構造時House1；NetworkHousePreserve=0時，原版發送器把兩者都改為House0，本機／遠端一致。`enqueue` 新增的 PlayerTest 授權複查只拒絕失效請求；Network檢查僅開關為真才參與，沒有找到在N=O=0時改frame／house／payload的新增路徑。

證據：`assist/tests/control_options_off_capture.py`、`research/evidence/control-options-off-native.json`。這是發布DLL內新增仲裁開關對照，沒有執行1.0.38 DLL，也沒有完整世界或實機平台。因此不能說已證整局相容；只能說這些有限用例未發現事件內容回歸。

## 第二個 Network 開啟的缺陷：跨發送者壓縮字典碰撞

三個獨立模擬機、三套獨立字典，原版codec加既有適配模型：真實House1的B先送完整物件組201／202，第三接收端C建立House1.slot0；改版端A（真實House0）以保留的外方House1送完整組101／102，覆蓋C相同slot0；B下一次用原版cached slot0時，本機DoList仍是201／202，C的原版 `64BDD0` 卻解碼成101／102。無A插入的對照仍為201／202。

protocol1／2 × 有／無插入，4/4 PASS。證據：`assist/tests/network_cross_sender_cache_capture.py`、`research/evidence/network-cross-sender-cache-native.json`。本機強制cache miss只能約束本機編碼器，不能禁止另一個原版發送者使用其快取索引。這是多發送者共用House破壞接收字典身份的另一條確定路徑，不依賴Owner選項，可由N=1、O=0的Alt工具外方事件構成。未驗證兩人局必現，亦未執行真實網路／任務效果。

目前結論：已定位兩個需要實際保留外方House的同步設計缺口；Owner 關閉本身不足以判斷是否命中；若兩項所屬方選項均關閉，使用者這次回歸仍未找到根因。後續應以實際出錯版本、選項及操作時間線判定，不能繼續拿已排除的前提作解釋。
