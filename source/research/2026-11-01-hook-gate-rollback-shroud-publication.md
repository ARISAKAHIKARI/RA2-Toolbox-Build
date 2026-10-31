# 撤回首次同步門檻／科技聯機封鎖，修正不完整陰影發布

## 範圍與依據

接續 `156f19dee2cc3fb2b3122e79cc14fc7234d9b199` 的逐組測試器。使用者明確要求：現在因首次幀同步門檻裝不上 Hook，要撤回；科技功能已有獨立開關，不要額外禁止聯機開啟；工具箱實景小地圖陰影閃爍要修。

本次沿用 [先前 RA2OL 審查](2026-11-01-ra2ol-hook-isolation-diagnostics.md)、[首次同步審查](2026-10-31-peer-sync-hook-activation-audit.md)、[恢復所有權審查](2026-11-01-latent-desync-mutation-audit.md) 及原生迷霧筆記。Library 原 `RA2-Reverse-Engineering-Notes.sqlite` 仍未能存取，新增內容保存在本筆記供原檔後續寫回，未建替代資料庫。

原生位址沿用既有樣本 SHA-256 `268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0`。沒有執行私人樣本、沒有新反組譯結論，沒有發布遊戲檔案或轉儲。以下是來源行為、記憶體 fixture 與本機 Windows 編譯證據，不是 RA2OL 雙機實測。

## 首次遠端同步條件完整撤回

* `assist/source/main.cpp`：刪除載入 Runtime 前的 `hostInitialSyncReady` 觀察門檻與 Gate 實例。
* `assist/source/bridge.cpp`：刪除阻塞功能維護的 `initialSyncTick`／approval 流程；獨立的 `hook_readiness.h` 僅管理精確簽名重試。原有本地對局／House／戰術視圖就緒檢查仍在。
* `shared/source/multiplayer_patch_gate.h`：補丁管理器與可選幀通知不再要求 approval token、遠端 FrameInfo 或 CRC 歷史；以模式 0/3/4/5、有效 House、本地 frame、running、未設 OOS 決定本地可安裝狀態。還原不依賴此檢查。
* 逐組測試器也刪除載入前的遠端同步觀察，不會透過另一份 Gate 卡住。

留下的 `initial_sync_gate.h` 僅供歷史觀察器測試與研究引用，沒有生產路徑 include／呼叫。`initial_sync_bridge.h` 已刪除。這次撤回不會改封包、CRC、平台檢查或 Hook 精確簽名；仍不覆蓋外部補丁。

本地判定沿用 `A8B238` Session、`A83D4C` House、`A8ED84` Frame、`A8E9A0` running、`A8B8C2` OOS。它不檢查 DoList 或 `B04474` CRC 歷史，也不要求幀數先增加。合成 fixture 在沒有任何 FrameInfo／CRC 記憶體的模式 3/4、frame=0 即允許；停止／無 House／非法 frame／OOS 則拒絕新安裝。是否每個 RA2OL 補丁簽名相容，仍由現場結果決定。

## 科技恢復原開關控制

`tech_enhancement_bridge.h` 撤回 `mayApply(mode)` 的單機限定與 SingleplayerOnly 狀態；`TechEnhance` 關閉仍不進入寫入迴圈，開啟時由原有獨立開關控制。原「科技全開」的 `UnsafeTech` 控制保持原樣，沒有改成自動啟用。

保留 `TechnoType + 0x634` 的原值記錄、類型／向量／世界身分驗證、寫入失敗仍持有所有權、關閉後恢復及失敗重試。這是撤回額外模式封鎖，不是撤回恢復修正；也不是宣稱單端改規則必然不影響同步。

## 小地圖陰影發布修正

來源確認的故障機制：`radar_map.h::Sampler::step` 在已存在 Cell 讀失敗或座標不符時 continue，鄰格讀取失敗可被 selector 當成不透明；到批次終點仍發布 `pending_`。因此不完整候選可能覆蓋上一完整陰影。`radar_live_shroud.h::Live::capture` 的鄰格 selector 也存在同類「讀失敗等同不透明」路徑。

現在：

1. 地形候選記錄完整性。非空格指標無法讀取、座標不符、selector 鄰格讀失敗或非法陰影值，整個候選作廢，保留上一完整 bank 及 revision；下輪重讀。真正的空指標／地圖外鄰格仍視為合法缺格。
2. 格子陣列、查表或必要幾何讀取失敗時只丟棄 pending，不把完整 front 清成空白。`offsetX/offsetY` 的讀失敗不再用合法值 0 代替，避免誤判幾何變更。已驗證的世界／幾何／顯示模式變更仍重置舊 bank。
3. 即時遮罩遇同類讀取失敗時回報 capture 失敗，不發布局部黑塊；呼叫端沿用已有的完成畫面保留路徑。整頁讀失敗但欄位讀取成功仍有效。

沿用的欄位：Cell 座標 `+0x24/+0x26`、探索 bits `+0x12C`、fog bits `+0x140`、selector table `7F4194`；Map `87F7E8` 的陣列 `+0x13C/+0x140`、投影位移 `+0x1490/+0x1498`。沒有新增遊戲寫入、沒有修改同步狀態、沒有改原生迷霧演算法或像素材質。

## 驗證與限制

新增 `assist/tests/radar_visibility_publication.cpp` 直接執行生產 Sampler／Live，注入主格、鄰格、陣列、查表、幾何及分批讀取失敗，驗證完整 front 不變、恢復後能發布、初次失敗無半成品、真正空格可用、整頁 fallback 可用、換 House 會清舊資料。另跑既有 `radar_boundary.cpp`、`radar_shroud_mask.cpp`，保持地圖邊界及原生 alpha 語義；科技原值／失敗恢復 fixture 仍通過。

完整編譯 x64 工具箱、x86 Runtime/broker；測試版重新編譯配套診斷 DLL 和 x64 觀察器，並檢查 PE／匯出／協議、執行 UI／broker 載入測試。[本輪建置及回歸證據](evidence/hook-gate-rollback-shroud-build.json) 和輸出包的 BUILD／VERIFIED 記錄提供實際檔案雜湊和執行結果。

沒有現場視覺或 RA2OL 雙機實測，因此只能確認修復了上述可重現的不完整陰影發布路徑，不能確認它涵蓋使用者所有閃爍來源，也不能宣稱 OOS 已解決。逐組測試仍需新遊戲進程及使用者逐輪判定。
