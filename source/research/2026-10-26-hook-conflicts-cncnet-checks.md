# 控制全部的 Hook 衝突與 CnCNet 額外檢查（2026-10-26）

## 問題與證據範圍

使用者確認 1.0.37 控制全部不掉線，詢問後續掉線是否可能為 Hook 衝突或 CnCNet 額外檢測。延續 [37／38 回歸比較](2026-10-26-control-138-regression-audit.md)，分別查自身補丁區間、外部公開補丁、客戶端檢查與遊戲同步錯誤。

本輪是公開上游源碼及本專案源碼審計，沒有執行原版／平台 EXE、沒有取得使用者實際載入模組或站點記憶體、沒有新的雙機實測。原版已知 EXE SHA-256 仍為先前記錄的 7b8a068535d6af06845edf95ae829b113d00c02909330e16f197426cd7db94b6；本輪不新增支援二進位聲明。私有子模組未讀取，也不從公開部分搜尋不到推定其不存在。

固定來源：
- RA2 Toolbox pre.6 產品 4f7248771754318706b5fd3cbd4ac63bf93ed8d7；研究分支 612e115aa5e97ee889ab56c64b2ad02a4c47fa5c。
- CnCNet/yr-patches：0318b1f1bddcc9c664c50ebee820cde74a662f47。
- CnCNet/yrpp-spawner：234c16f2ec6bb59cb2dc71779084b3f696d1840f。
- CnCNet/xna-cncnet-client：54ff7cb7ab35ec0e2caa140be5cec35e1bb870f9。

## 自身 Hook：沒有找到直接覆寫重疊，但外部相容性不能由統一管理器保證

已核對 50B6F0/6339BA/637E40 三個既有控制站點、646EF5/646FD2/6FFC93/6FFE2B 四個 Owner 適配站點、646F4E 成功後仲裁。這八個覆寫區間互不重疊，不能因同屬 QueueMegaMission 就說互相覆蓋。

舊 CS 646E90 和 NativeEvent 6FFE00 名稱仍在 registry，但目前 auto_cs_bridge/native_input_hooks 只有工具狀態維護；experimental_patch_policy::size 對兩站點返回0，manager不能藉此安裝舊入口補丁。新 Owner/Gateway 的 patch_context 要求相關函式入口16 bytes為已核實原碼，未知外部入口 detour 通常令新 Hook 拒絕安裝。

GameEntryManager 是本 GameRuntime 的唯一 writer，核對 thread/module/token、原碼與上下文；它無法禁止其他 DLL 自行寫遊戲記憶體。Registry 以起始地址索引，沒有通用的兩個 active 區間相交檢查。現有站點無重疊，所以這是防禦缺口，並非已重現的覆寫事故。

### 已確認的衝突處理／診斷缺口

來源：assist/source/hook_registry.h:139–158；placement_bridge.h:394–403、426–451；shared/source/game_entry_manager.cpp:30–71；assist/source/main.cpp:1460–1475、1538。

1. 外部後裝補丁與下一次普通維護檢查之間有時間窗。固定 context 探針不覆蓋整個函式，無法保證中間語義未變。
2. 已安裝站點 bytes 不再等於自己的 after 時，會記 sticky conflict 並拒絕覆寫；恢復也可能因 conflict 被拒絕。整組回復是盡力回復，可能只恢復部分站點。
3. riskControlActive=false 不等於所有遊戲 bytes 已恢復。裸 riskControlEntry 為 mov eax,1;ret，不讀該bool；若殘留跳轉／外部鏈仍抵達它，改bool不會使其失效。此情境尚無使用者機證據。
4. enabled=true 下維護／安裝失敗，沒有走主動關閉才會執行的 unselect，可能留有已選外方物件。是否導致不同步尚未證明。
5. ControlAll復選框關閉時，UI優先顯示「關閉」，可以遮蔽 playerControlStatus=-2 的回復失敗。Owner依賴提示也可能先於自身負狀態。
6. context失配並非一律累加 hookConflicts；計數0不能證明無相容性問題。自動patch日誌觸發未完整涵蓋 PlayerControl/Owner/Gateway 狀態變動，亦缺每站點預期／實際 bytes與回復結果。

上述管理器／控制組既有缺口大部分在1.0.37已存在。新增預設gateway擴大與外部補丁互動的範圍是一項合理推論，不能直接說缺口就是本次回歸原因。

## 與 CnCNet 公開補丁的交叉檢查

[完整掃描摘要](evidence/cncnet-public-hook-scan.json)保留13個控制／發送站點與檢查檔案清單。

- yr-patches：73份source/macros，解析169個hook/jump區間。
- yrpp-spawner：66份cpp/h，解析223個hook/jump區間。
- 兩個固定版本皆未找到13個目標的精確地址引用或已解析區間直接重疊。

相鄰但不重疊的實例：
- 新版 src/Ext/Event/Body.cpp:81 在 [64B6FE,64B704) 改事件長度處理；不是我們 [64B660,64B666) 的打包入口。
- 新版 src/Misc/MPDebug.cpp:83 在 [649C43,649C4A) 補指示器背景，續接649C7D；不是649CA0發送入口。
- 舊版 src/spawner/extended_events_c.c:22 在 [64B708,64B70C) 改事件長度表指針。

此結果只限已讀公開文字與解析的明確補丁區間，不包含私有元件、動態計算位址、使用者平台的其他DLL或不同版本。相同位元組未重疊也不能排除函式其他位置的語義互動。

yr-patches 官方已歸檔並推薦 yrpp-spawner；不能只研究舊repo就代表現有產品。yrpp-spawner仍有Private子模組，故不能宣稱已全面排除平台衝突。

## CnCNet 確有額外檢查：須區分四種機制

### 事件驗證／hardened 元件

官方 yrpp-spawner README 明列 event verification checks，說明 hardened 版需要非公開來源才能編譯，且發布管道提供對應構建；.gitmodules 明列Private子模組。舊 yr-patches README 也明列對 private anti-cheat component 的貢獻。

這證明存在額外驗證／保護，但不證明老師當前運行的版本啟用了哪項檢查，也不證明其檢查了上述Hook位元組。具體檢查條件和失敗處理不可由README推定，尤其不能擅自聲稱「它一定以單位Owner檢查每條命令」或「發現Hook必定製造SYNC」。

來源：
- https://github.com/CnCNet/yrpp-spawner/blob/234c16f2ec6bb59cb2dc71779084b3f696d1840f/README.md
- https://github.com/CnCNet/yrpp-spawner/blob/234c16f2ec6bb59cb2dc71779084b3f696d1840f/.gitmodules
- https://github.com/CnCNet/yr-patches/blob/0318b1f1bddcc9c664c50ebee820cde74a662f47/README.md

### 磁碟檔案摘要

官方 xna-cncnet-client 的 DXMainClient/Online/FileHashCalculator.cs 以FileInfo.OpenRead及SHA1讀取磁碟檔案。CnCNetGameLobby的FileHashNotification、CheaterNotification、StartGame、HandleCheatDetectedMessage處理房間檔案不一致與開局前檔案變更提示。

這條機制不是讀取遊戲執行時Hook bytes，已核對分支也沒有直接呼叫遊戲SYNC錯誤流程。

### 遊戲進行中的進程識別

DXMainClient/Online/CnCNetGameCheck.cs 在IsInGame時定期列舉進程，以進程／視窗資訊識別特定工具，命中後KillGameInstance呼叫Process.Kill終止遊戲。CnCNetLobby的WelcomeMessageReceived handler實際啟動服務，斷線handler取消，並非僅存在但未使用的程式。

這條公開實作的結果是遊戲進程被終止，不是原生模擬摘要比較失敗。診斷無須列出匹配名單或提出規避方法。

客戶端來源（固定54ff7cb）：
- DXMainClient/Online/FileHashCalculator.cs
- DXMainClient/Online/CnCNetGameCheck.cs
- DXMainClient/DXGUI/Multiplayer/GameLobby/CnCNetGameLobby.cs
- DXMainClient/DXGUI/Multiplayer/CnCNet/CnCNetLobby.cs

### 遊戲幀同步摘要

既有原版研究已確認64DAB0產生摘要，FrameInfo的CRC欄位與本機歷史槽比較；SYNC摘要失配說明這條檢查失敗，不能單憑它確定最初變更來自哪個功能。它不是客戶端磁碟SHA1。

此前使用者原版1.001失敗紀錄包含透明／半透明迷霧場景，不能改寫成「控制全部單獨在原版也已證明掉線」，也不能據此全面排除CnCNet對控制功能的影響。

## 對此次回歸的判斷與待取證

兩個方向都有可能；平台額外檢查的存在已有直接官方依據。當前沒有13站點與已讀公開補丁的直接重疊證據，也沒有實際平台檢查攔截此版本的證據。

若同一平台版本、同配置下1.0.37穩定而新版失敗，「平台原本就有檢查」本身不足以解釋版本回歸。還需找出新增Hook、事件差異、模組版本變更或衝突失敗狀態與出錯的因果連結。

最有判別力的資料：
1. 出錯遊戲EXE與已載入CnCNet/其他擴展DLL的版本及SHA-256。
2. 啟用前、啟用後、關閉後各Hook站點與context的預期／實際bytes、跳轉目的模組、安裝與回復狀態；避免只看復選框或總conflicts。
3. 同一場對局兩端的SYNC與client/game日誌，區分摘要失配、程序被終止、房間檔案警告、超時。
4. 保持ControlAll開啟，固定兩個Owner選項關閉，對NativeMegaGateway開／關做新程序、新對局對照。

本輪只記錄兼容性與診斷發現，沒有更改產品行為、沒有繞過／停用平台驗證，也沒有宣稱聯機問題已修復。


## 同日追加：官方 Hardened 二進位已有正面重疊證據

上述「公開來源沒有13站點直接交集」只描述當時的公開源碼掃描。本輪後續下載並靜態分析官方 CnCNetYR-Hardened v0.0.0.17，已確認其.patch有50B6F0/5，CRT初始化器生成跳向DLL自己的ControlledByCurrentPlayer實作；普通版無該項。Hardened另有事件控制者與House ID驗證。這是實際二進位的新證據，不能繼續寫成只知道README宣稱有驗證。

仍未對上使用者當時載入的DLL／站點bytes，且1.0.37同樣使用50B6F0，因此不能僅憑這筆重疊確認版本回歸或掉線原因。詳見 [Hardened二進位研究](2026-10-26-cncnet-hardened-binary-audit.md)。全圖採集則是Skirmish，不能代表通過多人特殊格檢查；詳見 [全圖與原版檢查](2026-10-26-trainer-map-cheatcheck.md)。
