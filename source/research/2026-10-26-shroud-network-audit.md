# 透明／半透明迷霧與聯機斷線：實作及證據邊界

日期：2026-10-26。問題：目前兩種迷霧模式改了什麼，是否會改變聯機摘要，以及戰網斷線的可能路徑。

結論：現版兩種模式攔截黑幕／迷霧繪製後半段，沒有發送地圖揭示事件，也沒有在該路徑呼叫原生 RevealMap。受控執行未發現遮罩像素或兩處程式碼補丁直接改變原版同步摘要。半透明存在已量化的逐像素管理器驗證開銷；但沒有 Windows 耗時、平台附加檢查或真實聯機斷線證據，不能宣稱它就是根因。

## 樣本及證據

- 支援原版 EXE SHA-256：`7b8a068535d6af06845edf95ae829b113d00c02909330e16f197426cd7db94b6`。
- 私有載入映像 SHA-256：`94ac595e7a6fef82839b7959f3f926a52708fc857fbc65aa9ff88428dff3f4bd`。不提交 EXE 或映像。
- 實測 pre.6 GameRuntime.dll SHA-256：`24c405af124ec60c88b92c47b0cbaca9ab02ba1ed7530f5631f442520f43a08c`。
- 原版 CRC 隔離：`assist/tests/shroud_crc_capture.py`、`research/evidence/shroud-crc-isolation-native.json`，6/6 PASS。
- 實際 DLL 繪圖 hook：`assist/tests/shroud_draw_capture.py`、`research/evidence/shroud-draw-native.json`，6/6 PASS。
- 靜態反組譯：私有 `tmp/native_events/inspect_native.py` 對 `47EFE0/47F250/4801F0/4114B0/69E740/69E7E0/6D2790/577D90/64DAB0`。
- 本輪沒有改產品、重編譯或執行 Windows／實際聯機測試。

## 兩種模式的實際改動

| UI 模式 | Host mapStrategy | Runtime renderReveal | 實作 |
| --- | ---: | ---: | --- |
| 半透明 | 1 | 2 | 在兩處繪製 hook 呼叫 halfShroud，重寫本地 alpha 遮罩 |
| 透明 | 2 | 1 | 在兩處繪製 hook 跳過遮罩繪製主體 |
| 原生揭示（不同模式） | 3 | 0 | ExperimentalReveal 開啟，直接呼叫 577D90 |
| 直接全圖（不同模式） | 4 | 3 | 安裝五處可見性／判定補丁 |

映射見 `assist/source/auto_settings_ui.h:63`、`extra_settings_io.h:46`。兩種繪製模式均關閉 ExperimentalReveal；`bridge.cpp:821` 又獨立拒絕 renderReveal 非零時的 RevealMap。`placement_bridge.h:500` 將五處直接模式與兩處繪製補丁互斥處理。現版沒有這兩種模式專屬的單機限制。

hook 位於 `47F038`（DrawShroud）及 `47F274`（DrawFog），各覆蓋五位元組。保留原版資源初始化前綴，以及呼叫端 `4801F0` 的 GetOcclusion 和 Cell `+120/+121` 邊緣快取更新；並非在整個函式入口立即返回。兩個原版葉函式均以 `ret 12` 返回；Fog 另有 `0x2C` 區域變數，hook 依各自堆疊形狀恢復。

`shroud_render_bridge.h` 讀 SHP 原版幀範圍、幀像素及 `87E8A4` 指向的 16-bit alpha 緩衝。alpha 中 0 代表不透光、127 代表清晰；有效 mask 以 `(mask+128)/2` 轉換，故 0→64、127→127。Fog 再取 destination 與轉換值的較小者；mask>127 跳過。這不是 Windows 視窗透明度，也不是直接改 Cell 探索旗標。

半透明沿用原版 `69E7E0`、`69E740`、`4114B0`；其中 SHPRef 可能懶載入資源。`4114B0` 的 surface vfunc `+5C/+60` 分別指向 `4115F0/411570`，成對增減 BSurface `+0C` 鎖計數，並計算像素地址。已查路徑沒有隨機數呼叫。

切換／停用會呼叫 `6D2790(Rect,false)` 請求重繪。該原版函式管理 `B0CE78` 脏矩形向量，記錄矩形、交集／聯集及容量擴展；完整靜態函式未見 House、Cell 探索、RNG 或物件顯示層的直接寫入。

工具小地圖及觀察窗會把所有非關閉模式當成 reveal=true，因而半透明主畫面時它們仍可能完全顯示地圖；它們是外部讀取與工具繪圖，不會因此寫遊戲探索狀態。

## 原版同步摘要隔離

沿用 [原版聯機摘要研究](2026-10-26-network-event-audit.md)。`64DAB0` 是循環左移一位再相加的摘要，而非通用 CRC32。它混入單位位置／朝向、House `+241 MapIsClear`、五個顯示層與 Logic 列表的物件位置／RTTI，以及一次 Scenario RNG 輸出。

每例從相同合成世界及 RNG 初態開始，完整執行私有映像中的 `64DAB0` 與 `65C780`，只替代原版日誌入口：

| 單一變動 | 摘要 |
| --- | ---: |
| alpha 全 0（基準） | 863614368 |
| alpha 全 64 | 863614368 |
| alpha 全 127 | 863614368 |
| 僅改 47F038 的 JMP 位元組 | 863614368 |
| 同時改 47F038／47F274 的 JMP 位元組 | 863614368 |
| House+241 改為 1（陽性對照） | 863614496 |

這排除「這些像素或兩處程式碼位元組被該原版摘要直接讀入」。該隔離測試沒有執行 JMP 目標、繪圖或後續模擬 tick，不能單獨證明 hook 沒有間接副作用。顯示層和 Logic 本身會被摘要涵蓋，所以也不能推論「所有渲染相關資料都不受校驗」。

## 實際 DLL 葉繪圖量測

合成已載入、未壓縮的 32×32 SHP（1024 個有效 mask）、原版 BSurface alpha 佈局；執行上述最終 DLL 的真實 hook 及原版 SHP／surface helper。Win32 與切換時重繪用 fixture 代替。六例為關閉／透明／半透明各測 Shroud 與 Fog。

| 每次 32×32 葉繪製 | 原版 | 透明 | 半透明 |
| --- | ---: | ---: | ---: |
| source mask 讀取 | 1024 | 0 | 1024 |
| alpha 寫入 | 1024 | 0 | 1024 |
| ManageGameEntry Validate | 0 | 0 | 1027 |
| Capture／Leave | 0／0 | 1／1 | 1／1 |

原因：`bridge.cpp:127` 的每次 put 都呼叫 `gameentry::active`；`game_entry_client.h:42` 隨即走管理器 Validate。1027 次等於 1024 次像素寫入加三次原版 helper 包裝呼叫。半透明另有 4135 次 Runtime DLL 資料寫入，主要為管理器狀態／原子操作；不能把此數量解讀成改寫遊戲模擬狀態。

六例所有非 stack／SEH／Runtime DLL 寫入僅落於 alpha 像素及 surface 鎖計數；鎖計數最終均回到 0。x87 八個暫存器及控制狀態、MXCSR、XMM 種子檢查均恢復。這支持目前葉路徑的狀態保存，但不涵蓋其他 SHP 格式、畫面邊界、平台 hook、例外路徑或後續世界更新。呼叫次數是確定證據，沒有量到 Windows 實際毫秒數，不能換算幀率或宣稱必然逾時。

## 斷線的候選原因與排除條件

1. **半透明開銷導致卡頓／連線逾時**：有明確開銷證據；需實機耗時及連線日誌確認。透明沒有逐像素 Validate，不能用此單一原因解釋兩種模式都斷線。
2. **先前原生揭示的狀態殘留**：`bridge.cpp:828` 直接呼叫 `577D90`，其中 `577DB0` 寫 House+241=1、`577E29` 寫 CurrentHouse+240=1，並寫 Cell+130/+134、+12C、+140。House+241 有確定摘要因果鏈。之後切換透明或關閉只恢復補丁及重繪，沒有回滾探索欄位。因此須用從未開原生揭示的新對局隔離。
3. **平台附加檢查或平台補丁互動**：尚未取得使用者實際戰網客戶端、遊戲模組版本及斷線日誌，不能直接套用原版函式結論，也不能宣稱已確認反作弊踢出。CnCNet yrpp-spawner 官方 README 說明另有 hardened 版本及事件驗證，但不證明它檢查這兩個位址，更不證明使用者就是該版本。
4. **未覆蓋的 hook／後續模擬副作用**：現有靜態路徑及六例葉繪圖未找到直接改摘要輸入的寫入；仍需真實失敗時比較 MapIsClear、RNG、顯示層／Logic 列表及錯誤種類。

官方參考（2026-10-26 查閱）：https://github.com/CnCNet/yrpp-spawner/blob/main/README.md 。僅用於界定原版研究不能代替平台完整性分析。

目前缺少的關鍵證據是斷線提示原文（同步錯誤、重連／逾時、程式崩潰或平台退出）、戰網版本，以及同局是否先開過原生揭示／直接模式。本輪結果是縮小範圍，沒有確認真實斷線根因。
