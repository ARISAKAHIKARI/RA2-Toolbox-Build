# 修改大師全圖、原版特殊格檢查與 CnCNet：實際證據（2026-10-26）

## 問題與結論

使用者問「修改大師的開啟全圖是怎麼繞過地圖合法檢測的」。本輪重新讀取先前提供的全圖四階段採集，對照原版機器碼、官方 YRpp／Phobos 和實際官方 CnCNet v0.0.0.17 DLL。

**已證實全圖改動包含顯示分支、雷達條件、SpySat 類型條件及重罩迴圈；沒有在已查的原版特殊格檢查函式／caller 中觀察到修改。這份採集是 Skirmish，原版那兩個多人特殊格檢查在該模式下不執行，因此不能把採集成功稱為繞過了聯機或 CnCNet 檢查。**

顯示未探索格與寫入探索狀態是不同操作。6D88FC 的分支能改變黑幕幀選擇而不直接寫 Cell 探索旗標；但整組功能另放寬 SpySat 判斷，具備走到原生 Reveal 的條件路徑，不能概括為在所有情況下只改繪圖。

## 樣本與驗證方式

- 採集：`RA2Master-Capture-20261025-002452-24508.zip`，SHA-256 `effd9473f9d6489cbf637d706dcdd1745aedffb43a4ce40f3fa66b2f0e7aff12`。
- 採集報告的原版 EXE SHA-256：`7b8a068535d6af06845edf95ae829b113d00c02909330e16f197426cd7db94b6`；flat 映像基址 400000。
- 四階段映像、valid/stable mask 均重新讀取；下列程式區間與 SessionType 的 valid/stable 全為1。
- 私有 heap 只有 off/on/off，所讀 House 欄位的 valid mask 通過；heap 沒有 stable mask，不能稱為原子或逐幀追蹤。
- 只做只讀快照分析及 x86 靜態反組譯，沒有執行原版／修改大師／CnCNet DLL，沒有新增多人實測。
- 去識別證據：[trainer-map-cheatcheck.json](evidence/trainer-map-cheatcheck.json)。原始 EXE、記憶體及 heap 不提交。

## 1. 實際全圖補丁

| 位址 | clean／off → on → off-again | 可支持的作用 |
| --- | --- | --- |
| 508E60 | 7D25 → EB25 → 7D25 | 雷達電力條件路徑 |
| 508EBA | 744C → 9090 → 744C | 雷達建築類型條件 |
| 508F8F | 7465 → 9090 → 7465 | SpySat 建築類型條件 |
| 577B3A | 7439 → EB39 → **7539** | 跳過 ResetShroud 的逐格重罩；關閉未恢復原始條件 |
| 6D88FC | 7505 → EB05 → 7505 | 黑幕幀選擇，避免未探索格被設定為完整黑幕幀 |
| 6378E0 | 4CAC0085C9 → E8AB38EDFF → 同 on | 跨原始指令邊界的異常寫入，關閉後殘留；本產品不採用 |

與先前 [直接模式研究](2026-10-25-direct-modes-138-pre1.md) 及其 map-capture-analysis.json 相符。本次沒有把舊「未啟動→啟動但關閉」捕獲中的 74→75 誤認為有效全圖 patch；新採集的開啟值確實是 EB，關閉值75屬恢復殘留。

### 顯示分支的具體區別

`6D8700` 先令 EDI=-1。mode0 讀 Cell+12C；原版在該欄位的0x18位均未設定時，於6D88FE把 EDI 改成-2。修改大師改動6D88FC後跳過這項賦值；若0x08位也未設定，6D890A直接走6D8AD4，返回保留的-1。這條路徑讀取探索位以決定顯示幀，沒有把它們改成「已探索」。

因此可以存在「畫面上顯示了未探索格，但該格資料仍未探索」的狀態。這解釋顯示型全圖為何不必先關閉讀取探索資料的檢查；並不單獨證明整個功能的長時間同步。

### SpySat 路徑仍有模擬狀態風險

508F60 掃描 House 建築。放寬508F8F後，在其餘建築條件及 SpySatActive 轉換滿足時，可到509064呼叫 `Map::Reveal(577D90)`，隨後設 House+577A=1。原生 Reveal 會寫 House+241 MapIsClear、CurrentHouse+240 Visionary 及 Cell 探索／重罩欄位。因此不能從單一顯示分支推論整組全圖永遠不改遊戲狀態。

本次捕獲的 CurrentHouse 在 off/on/off 均為：

| 欄位 | off | on | off-again |
| --- | ---: | ---: | ---: |
| Visionary +240 | 0 | 0 | 0 |
| MapIsClear +241 | 0 | 0 | 0 |
| SpySatActive +577A | 0 | 0 | 0 |

這只證明三個快照時刻的這個 House 沒有全揭示旗標；沒有證明所有 House／Cell 不變、期間未有瞬時修改，或這次曾執行 Reveal。

## 2. 原版「特殊格檢查」實際檢查什麼

原版兩處路徑為 main loop `55DDAA..55DE13` 及 House::AI `4F883E..`。前者於55DDC5、後者於4F8861呼叫 MPGameMode vtable+4；已知 vtable7EE184的+4指向5C0E30。原版5C0E30為回傳false的葉函式，官方來源以 SkipCheatCheck 描述其相關語意。

兩路徑均先限定：

1. SessionType 是 LAN=3 或 Internet=4；
2. MPGameMode 非空；
3. 該模式方法回傳false。

檢查輸入是特殊 Cell 的 `+130 != 1` 或 `(+12C & 0x18) != 0`，不是地圖檔案 SHA，也不是畫面是否看得到地形。條件成立後分別在55DE08／4F8908呼叫 `65C7E0`，以 Scenario+218 的同步 RNG 取0..2，返回值未使用。House路徑另有本地House與Global RNG抽查條件。

這意味著各端若對那些格的狀態判斷不同，可能額外消耗不同次數的同步亂數，進而讓後續模擬／摘要不同。該處不是直接跳到同步錯誤視窗；也不能把它與所有CRC檢查或平台私有檢查混成一個開關。

**四階段 SessionType 都是5（Skirmish），且上述caller、5C0E30、已查vtable及原生Reveal區間完全相同。** 列出的模組名稱未見CnCNet／Spawner／Phobos／Ares；模組清單的陰性結果不是排除手動映射的證明，但SessionType gate已足以界定這份捕獲不能驗證該MP檢查。

## 3. 原生 Reveal 與 Phobos 修復

原版577D90在模式3/4、MPGameMode非空且 SkipCheatCheck=false 時，揭示地圖會跳過三個特殊格。這是原版 SpySat／Reveal 既有邏輯。修改大師若走到這條未改的原生路徑，能沿用該保留行為；這只是靜態可達條件，沒有多人執行記錄證明當次如此。

官方 [Phobos PR2111](https://github.com/Phobos-developers/Phobos/pull/2111) 曾提出直接把 SkipCheatCheck 設true，未合併；其文字對極性的首句有歧義，本輪以原版指令確認 true 是跳過。已合併的 [PR2115](https://github.com/Phobos-developers/Phobos/pull/2115)（merge73b10493540c9e4aa9463218753f6b6e09a1785d）改為讓 CreateGap/DeleteGap/ResetShroud/ResetShroudForTMission 同樣避開特殊格，修復與SpySat狀態處理不一致造成的失步。

固定 [Phobos實作](https://github.com/Phobos-developers/Phobos/blob/a1ca6661615335947aa0eed2875148af75e339b2/src/Misc/Hooks.BugFixes.cpp) 的 ShroudFix 區域支持上述關係。這也確認577B3A屬於重罩迴圈，不是原版反作弊檢查入口。

## 4. CnCNet v17 的實際二進位結果

本輪下載的官方普通 CnCNetYR 與 CnCNetYR-Hardened v0.0.0.17 DLL，**兩者的.patch都有5C0E30、長度2、資料B001**，使原版葉函式回傳true。發布版 [Bugfixes.Desyncs.cpp](https://github.com/CnCNet/yrpp-spawner/blob/fbe8d7d2620584d04ba14fa3580943bfa1448d40/src/Misc/Bugfixes.Desyncs.cpp) 將其列為特殊格黑幕檢查的desync修復。

所以在正常套用了該補丁的這兩個固定版本中，這項舊檢查由官方停用；不能把它記成修改大師破解。它不代表原版遊戲CRC停用，更不代表Hardened沒有額外驗證。樣本SHA、正常啟動套用鏈、額外map helper與ControlHouse衝突見 [Hardened二進位研究](2026-10-26-cncnet-hardened-binary-audit.md) 及 [map-shroud證據](evidence/cncnet-v17-map-shroud.json)。

## 結論邊界與後續判定

- 已知的是顯示分支、可達原生Reveal、原版檢查的模式gate與資料依賴、固定官方DLL的共用修復。
- 尚未有「修改大師在使用者那個CnCNet版本成功通過所有檢查」的實機證據。
- 也未證明本次全圖採集所處模式／模組與使用者報告掉線時相同。
- 原版MapIsClear會進入同步摘要；顯示型路徑不直接寫它，不代表完整對局不存在其他狀態／RNG分歧。沿用 [原版同步研究](2026-10-26-network-event-audit.md) 的證據邊界。
- 本輪沒有改產品邏輯、重編譯、發布版本或修改反作弊。

### 型別／欄位來源

YRpp固定 `d9795546950fcc75a94325ffc83b44d16ec34ce2` 的 [GameMode](https://github.com/Phobos-developers/YRpp/blob/d9795546950fcc75a94325ffc83b44d16ec34ce2/GeneralDefinitions.h)、SessionClass.h、HouseClass.h、MapClass.h、CellClass.h、Randomizer.h、ScenarioClass.h；spawner release所用YRpp另固定為b22f1860dd7b100cad57123d4ca5d459b9020d88，兩者來源用途分開記錄。
