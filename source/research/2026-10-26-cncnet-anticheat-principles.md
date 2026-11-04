# CnCNet Hardened v17 反作弊原理：程式碼、資料、封包與事件分層驗證

## 問題、樣本與證據範圍

使用者要求先繼續分析 CnCNet 反作弊程式原理。全圖開關／快捷鍵恢復停在本地 checkpoint，未隨本次研究提交產品修改。

本輪從官方發布的實際 DLL 繼續追蹤，而非由 README 的 anti-cheat 名稱推測。已確認 **Hardened 存在執行期間的遊戲程式碼 CRC 掃描；失配進入刻意破壞控制流的致命異常路徑**。另有特定資料快照檢查、地圖／規則內容衍生的封包驗證、事件身分和控制權驗證、資源與雷達狀態接管。這些機制的失敗處理不同，不能一律稱為原版同步錯誤。

| 對象 | 固定版本／SHA-256 |
| --- | --- |
| 官方 Spawner release | `v0.0.0.17`，commit `fbe8d7d2620584d04ba14fa3580943bfa1448d40` |
| Hardened DLL | `62466e9e534ceb15c5d71fb05c62ad98a2b2b4903c569ed0c82189ef86ea3b7d`，417792 bytes |
| 普通 DLL | `58f198973bd0fd3b0e1b2c42fc6c545efe1fd399680abc2e8d6e9e189751614d`，233472 bytes |
| release 的 YRpp | `b22f1860dd7b100cad57123d4ca5d459b9020d88` |
| 公開 C# client | `CnCNet/xna-cncnet-client` commit `54ff7cb7ab35ec0e2caa140be5cec35e1bb870f9`；獨立來源，不能假設與使用者安裝包完全相同 |
| 原版指令參照 | 已支援 EXE SHA-256 `7b8a068535d6af06845edf95ae829b113d00c02909330e16f197426cd7db94b6`；私有乾淨 loaded-image 的相關區間 valid/stable 均通過 |
| Toolbox 比較版本 | `1.0.39-pre.6`，commit `4f7248771754318706b5fd3cbd4ac63bf93ed8d7` |

下載來源為 [官方 release](https://github.com/CnCNet/yrpp-spawner/releases/tag/v0.0.0.17)，ZIP 已按官方 digest 核對；DLL 雜湊另行固定。沒有讀取 Private 子模組原始碼，也沒有執行 DLL 或實機聯機。以下遊戲地址為固定遊戲 VA；DLL 位置以 RVA 表示，不能把首選 ImageBase `0x10000000` 當作每次實際載入基址。

本輪分級：公開源碼、PE／機器碼與靜態控制流為已證事實；從已證條件推導可能故障為有條件推論；使用者現場載入版本、實際命中、掉線因果尚未確認。

## 1. 執行期間的程式碼完整性檢查

### 1.1 建立基準

DLL RVA `0xEA00` 依序讀取遊戲 `[0x401000, 0x7E1000)`：248 個區塊，每塊 16384 bytes，總共 4063232 bytes。這涵蓋原版 `.text` 到 `0x7E038D` 以及其後至 `.rdata` 前的對齊區；不是全程序記憶體掃描。

每塊用遊戲 `0x81F7B4` 的 CRC 表及當前種子計算，結果存入 DLL RVA `0x73880` 的基準陣列。已獨立核對表的 256 項，等於反射式 CRC-32 多項式 `0xEDB88320`，來源 bytes 的 valid/stable 均為真。這與原版 `0x64DAB0` 的 ROL1 加法遊戲狀態摘要是不同演算法、不同資料。

兩條已確認的基準建立路徑：

- `ScenarioClass::StartScenario(0x683AB0)` 成功返回前的 `0x683E94` → DLL `0xEB40` → `0xEA00`。
- `ScenarioClass::LoadGame(0x67E440)` 完成處的 `0x67E6E0` → DLL `0xEAF0` → `0xEA00`；保留原完成日誌後建立基準。

這是對當時記憶體內容建立本次執行的基準，並非每次掃描都重新接受當前內容。基準取得時間與實際修改時間影響是否產生差異；這是診斷時序，不能據此宣稱任何載入順序已驗證相容。

### 1.2 輪流重算與失配

遊戲主迴圈站點 `0x55D8B4` 轉入 DLL RVA `0xEB60`。在 `Session.GameMode=3/4`（LAN／Internet）時，每到達一次此站點便驗證一塊，索引到 248 後回到 0。

原主迴圈外側有遊戲狀態分支，可以跳過此站點。因此準確說法是「248 次符合條件的站點到達完成一輪」，不能由靜態碼換算為固定秒數或 248 幀。

任一區塊重算值與基準不同，就呼叫 DLL RVA `0xE530`。該函式改寫 ESP 等暫存器，清零 EDI，再令 EBP=EDI 並 `JMP EBP` 到零地址。這是刻意的非法控制轉移，並非只記日誌或一般返回。尚未分析實機的例外攔截／Windows UI，不能寫成已證實的特定視窗、退出碼、伺服器封禁或原版同步錯誤。

### 1.3 與 Toolbox 的關係

已核對的 13 個控制／事件站點、2 個繪製站點 `47F038/47F274`、5 個修改大師式全圖 patch 站點，全部落在上述完整性驗證範圍內。完整列表見 [覆蓋證據](evidence/cncnet-v17-integrity-coverage.json)。

因此「兩個模組沒有覆寫相同地址」只排除局部實體覆蓋，不能排除程式碼完整性不相容。即使某個 Hook 只改變繪製行為，安裝 Hook 時改寫的遊戲指令仍可能與基準不同。

有條件因果鏈為：現場使用該 Hardened → 已建立基準 → 受檢位元組在其後改變 → 該區塊再次被掃描且 CRC 不符 → 進入 E530。使用者實際 DLL、基準 bytes、修改時序與命中尚未取得，不能省略這些條件而宣稱已鎖定根因。

## 2. 特定資料與本機玩家指標檢查

DLL RVA `0xE5E0` 保存最多 4096 種 BuildingType 的三個欄位，`0xE690` 在 LAN／Internet 比較：

| BuildingType offset | 公開名稱 | 本次核對 |
| --- | --- | --- |
| `+0xEB4` | `Adjacent` | YRpp 與原版 INI 欄位讀取、字串互證 |
| `+0x170C` | `PsychicDetectionRadius` | 同上 |
| `+0x157A` | `ClickRepairable` | 同上 |

快照在 BuildingType 載入 INI 後及讀檔完成時建立；快照函式另外容許部分設定條件，不能說它只在多人模式執行。比較則明確限制 LAN／Internet，任一已檢欄位改變走 E530。這只證明三個欄位，沒有證明金錢、所有單位屬性或全部物件資料都被掃描。

同一個 `0xE690` 還做兩項不同檢查：

1. 第一次有效執行時保存 `HouseClass::CurrentPlayer @ 0xA83D4C`；後續指標改變走 E530。
2. 另外，必須同時滿足 `CurrentFrame > 100`、CurrentPlayer 非空、CurrentPlayer 等於 `HouseClass::Observer @ 0xAC1198`，且該 House 的 `Defeated(+0x1F5)` 為 false，才走 E530。

第二條檢查不是泛稱「觀察者旗標改變就崩潰」；欄位是 Defeated，而且有幀數與指標身分條件。

原始欄位來源：[BuildingTypeClass.h](https://github.com/Phobos-developers/YRpp/blob/b22f1860dd7b100cad57123d4ca5d459b9020d88/BuildingTypeClass.h)、[HouseClass.h](https://github.com/Phobos-developers/YRpp/blob/b22f1860dd7b100cad57123d4ca5d459b9020d88/HouseClass.h)。

## 3. 地圖與規則內容衍生的封包驗證

### 3.1 載入內容，再把衍生值用於通訊

遊戲 `Read_Scenario_INI` 的 `0x6867AA` 呼叫被轉入 DLL RVA `0xEA90`。在 LAN／Internet 時，經 `0xE970` 讀取目前地圖檔內容，再讀 `RULESMD.INI`，累積內容 CRC，並加入 `Game::Network::WOLGameID @ 0xB779D4` 衍生的資料。結果存於 DLL 的一個 DWORD。

WOLGameID 是網路對局 ID，不是 `Game::Seed @ 0xA8ED94` 或模擬 RNG 狀態。讀取失敗、讀取長度不完整等情況會保留先前累積值，這段沒有已證的官方簽章或固定合法地圖白名單比較。LoadGame 的另一路更新既有衍生值，沒有重新呼叫檔案 helper 讀取這兩個檔案。

### 3.2 送出與接收的直接結果

Hardened 的 `WinMain_SpawnerInit` 將遊戲 UDP 呼叫站點 `0x7B3D75/0x7B3EEC` 分別接到 DLL `0xE2E0/0xE3F0`。公開設定在 Hardened 與 CnCNetYR 構建中令 Spawner Enabled 為常數 true；實際 Hardened 初始化指令也確認了這組動態 patch，可達性不只來自函式存在。

送出端用內容衍生的值轉換 payload，並計算原 payload CRC，附 8-byte 前綴後送出。接收端依路由／session 條件檢查前綴，以本機衍生值還原 payload，再核對 CRC。CRC 不同時在 `0xE4C8` 跳 `0xE523` 返回 -1，拒收該封包；這條失敗路徑不呼叫 E530，也不直接呼叫原版同步錯誤對話框。

普通 DLL 在相同初始化位置選用 `0x8090/0x8190`，已逐段核對為公開 NetHack 的直接 send/recv 或 4-byte tunnel routing header，沒有這項內容衍生轉換與新增 payload CRC。這也說明僅統計新增 17 個 hook／46 筆 static patch，會漏掉共同 hook 所選函式本體的差異。

若對端的檔案／session 衍生值不同，可能在還原及 CRC 比對時被拒收。這是內容一致性與封包完整性機制，不宜稱為密碼學認證，也不能由此推論已證明強加密。

**這裡讀的是地圖／規則檔的內容，不是全圖啟用後的 `MapIsClear` 或 Cell 記憶體旗標。** 因此不能說按下全圖必然改變這個值，或這項機制已解釋使用者的全圖掉線。

公開安裝／普通封包處理來源：[Spawner.Hook.cpp](https://github.com/CnCNet/yrpp-spawner/blob/fbe8d7d2620584d04ba14fa3580943bfa1448d40/src/Spawner/Spawner.Hook.cpp)、[Spawner.h](https://github.com/CnCNet/yrpp-spawner/blob/fbe8d7d2620584d04ba14fa3580943bfa1448d40/src/Spawner/Spawner.h)、[NetHack.cpp](https://github.com/CnCNet/yrpp-spawner/blob/fbe8d7d2620584d04ba14fa3580943bfa1448d40/src/Spawner/NetHack.cpp)；Game ID 名稱來源：[YRpp Unsorted.h](https://github.com/Phobos-developers/YRpp/blob/b22f1860dd7b100cad57123d4ca5d459b9020d88/Unsorted.h)。Hardened 新增行為的證據是本次固定 SHA 的實際 DLL，不能把普通 NetHack 公開源碼當成新增機制的原始碼。

## 4. 事件身分與目標控制權：兩個不同問題

### 4.1 指令是否來自宣告的玩家

`0x64A0E4` 的 hook 保存接收連線建立時綁定的 HouseIndex，`0x64BE75` 的 hook 與解壓後 `EventClass.HouseIndex` 比較。expected 的來源鏈已閉環：

`NodeNameType.HouseIndex(+0x6F)` → CreateConnections → connection constructor 存 `+0x64` → 收到資料的 connection 將該值輸出 → caller 傳給 `0x64A0C0` → hook 捕獲 EDX。

actual 為 decoder 區域 EventClass 起點 `[ESP+0x3C]` 加欄位 `+2`，即 `[ESP+0x3E]`。壓縮事件沿用 FrameInfo 所提供的 header。連線綁定身分、連線陣列下標、事件自己宣告的身分，三者不可混稱。

失配會記錄 forged-house-ID 診斷，返回 `0x64C370`，其語義是「停止這次解碼，返回已入隊事件數」。已接受的前綴不回滾，當前及後續事件不再入隊。它不是佇列滿時的 -1 返回，也沒有直接跳進原版 FrameInfo CRC 錯誤路徑。

本項註冊站點屬壓縮 decoder，已追 caller 的 `CommProtocol!=0` 分支會進入；不能由此宣稱其他協定分支完全沒有驗證。

### 4.2 該玩家是否有權控制目標

10 個事件站點共用 DLL RVA `0xEEE0`，檢查 `TechnoClass::GetControllingHouse()` 是否等於 `EventClass.HouseIndex`。涉及 PowerOn、PowerOff、Archive、Repair、Sell、Primary、Idle、Deploy、Scatter，及共用入口的 MegaMission／MegaMissionF。

失配或目標為空會轉到 `0x4C8109`，即原版 dispatcher 的一般尾端。呼叫者仍把 DoList 事件標為已處理，或將 MegaMission 佇列頭移除。因此被拒事件會被消耗，沒有自動延後重試。

這是對 GetControllingHouse 的呼叫，不能稱為讀取不可受影響的原始 Owner。Toolbox 的 `0x6339BA` 就在該函式內。更改事件宣告身分也不等於通過另一項連線綁定身分驗證。

以上兩項拒絕可能間接影響後續等待或狀態，但是否造成某場逾時／不同步，必須另有執行證據。詳細閉環與有限原版有效穩定區間見 [事件流程證據](evidence/cncnet-v17-event-flow.json)。

### 4.3 Detonate 的身分來源是附著炸彈

另一個新增站點 `0x4C782B` → DLL RVA `0xEF10` 屬於 Detonate 事件。原版先以 `TargetClass::As_Techno(0x6E6F20)` 取得目標；helper 再讀 `target+0x38 = ObjectClass::AttachedBomb`，以及 `BombClass+0x28 = OwnerHouse`。

LAN／Internet 下，已存在有效 House 指標時，判斷等價於附著炸彈的 OwnerHouse.ArrayIndex 是否等於事件 HouseIndex；其身分來源不是被附著物件自己的控制方。Campaign 另有 CurrentPlayer／人類或受控 House 的選擇邏輯，精確條件保留於 JSON。目標或 AttachedBomb 為空，或身分不符，都走相同 `0x4C8109` 返回並消耗事件。

已用公開 `TargetClass.h`、`ObjectClass.h`、`BombClass.h` 與原版 Detonate case 隨後呼叫 `BombClass::Detonate(0x438720)` 交叉確認；這補足前輪未知的 `+0x38/+0x28`，不能泛稱與 `GetControllingHouse()` 相同實作。

## 5. 生產、放置與退出事件

| 入口 | 已確認語義 | 直接結果 |
| --- | --- | --- |
| `4FA35D` → DLL `EE60` | 排除負 type index，取得類型後呼叫 `HouseClass::CanBuild(type,false,true)` | 不可建造回 `ProdFailType::Illegal(2)`，一般函式退出；未證完整上界檢查 |
| `4FB203` → DLL `EFE0` | 對適用玩家的 Building 呼叫原生 `PassesProximityCheck`；相關 `4A8EC3` patch 移除原本非本機 House 的提前成功分支 | 不通過則走原版放置失敗分支 `4FB35F` |
| `64C850` → DLL `EF90` | 遠端 EXIT 對人類 House 呼叫 `FlagToDie`；非人類保留剩餘人類判斷 | 遊戲中的退出／判敗語義 |

這三段都不能概括成「查到作弊即把程序殺掉」。尤其 Remote EXIT 處理本身不是作弊偵測器。

## 6. 黑幕資源與雷達狀態接管

五個地圖 helper 已逐條對照原版：

- `47EFF1 → ECB0`、`47F25E → ECE0`：直接選用 DLL 內嵌的 SHROUD／FOG SHP，替代外部資源載入。
- `58647A → ED00`：搬移原有 `Cell+12C & 8` 判斷與返回。
- `6D88FE → ED30`、`6D8903 → ED50`：搬移原有 `EDI=-2`／`EBX=8` 指令，並未新增條件。

所以前兩項可觀察的效果是固定該繪製路徑所用資源，後三項保留原語義並搬移指令。這五個 helper 本身沒有 CRC 失配處罰分支；不能把它們稱為五種新作弊檢查，也不能反過來否定第 1 節已證的完整性掃描。

Toolbox 的兩個晚段繪製 Hook 在資源準備與原版 Cell cache 更新之後，控制流確實可以前後接續，表列覆寫區間也不相交。但這兩個入口仍在程式碼 CRC 掃描範圍內。因此「局部繪製流程保留」與「整體 Hardened 相容」是兩項不同結論。

另外有 21 個 radar patch 把原本 `RadarClass+0x14B0`（全域絕對地址 `0x880C98`）的讀、寫、取址及比較，改用 DLL 自己的一個 DWORD；原值操作與續行逐筆保持。公開欄位仍命名 `unknown_14B0`，不能猜成所有雷達權限。其效果是改舊地址不再改變這份狀態；防止固定地址修改的意圖屬合理推論，這些 helper 本身沒有 CRC 或上報。

圖形細節、C5/C6 旗標不對稱及不能以此判根因的理由見 [map/shroud 流程證據](evidence/cncnet-v17-map-controlflow.json)。

## 7. 外部 C# 客戶端：磁碟摘要與程序識別

此節是固定公開 client 原始碼分析，不等同於前述 DLL 的發布版本。

`FileHashCalculator` 對指定磁碟檔案計算 SHA-1，部分文字檔會正規化換行並 Trim，再把各項合成總摘要。房間加入時交換 FHSH，與房主不一致時顯示紅色警告；StartGame 會再計算，與加入房間時不同便記錄／通知檔案變更。已讀的失配分支沒有因該原因提早 return，之後仍進一般啟動流程。不能將這條源碼寫成自動封禁或遊戲執行期記憶體掃描。

`CnCNetGameCheck` 則在遊戲進行中定期列舉程序，根據程序／視窗資訊識別特定工具，命中後 `KillGameInstance` 透過 `Process.Kill` 終止遊戲。服務有實際啟動點：大廳收到 Welcome 後啟動，斷線時取消。這與 DLL 的 CRC→E530，以及原版的 FrameInfo 摘要錯誤，屬三種不同的失敗或中止路徑。

公開來源：

- [CnCNetGameCheck.cs](https://github.com/CnCNet/xna-cncnet-client/blob/54ff7cb7ab35ec0e2caa140be5cec35e1bb870f9/DXMainClient/Online/CnCNetGameCheck.cs)
- [FileHashCalculator.cs](https://github.com/CnCNet/xna-cncnet-client/blob/54ff7cb7ab35ec0e2caa140be5cec35e1bb870f9/DXMainClient/Online/FileHashCalculator.cs)
- [CnCNetGameLobby.cs](https://github.com/CnCNet/xna-cncnet-client/blob/54ff7cb7ab35ec0e2caa140be5cec35e1bb870f9/DXMainClient/DXGUI/Multiplayer/GameLobby/CnCNetGameLobby.cs)
- [CnCNetLobby.cs](https://github.com/CnCNet/xna-cncnet-client/blob/54ff7cb7ab35ec0e2caa140be5cec35e1bb870f9/DXMainClient/DXGUI/Multiplayer/CnCNet/CnCNetLobby.cs)

## 8. 原版地圖特殊格與遊戲同步摘要仍須分開

普通及 Hardened v17 都有 `5C0E30/2 = B0 01`，令原版 `SkipCheatCheck()` 返回 true。公開發布源碼將其列為 specific-cell shroud desync 修復。這只停用那個舊特殊格檢查，不是關閉第 1–7 節的機制，更不是修改大師完成的繞過。

原版真正的 FrameInfo CRC 比較在 `0x64C6F0`；失配才跳 `0x64CC61`，經同步診斷、訊息對話框並失敗返回。輸入是遊戲歷史幀的狀態摘要，與 DLL 的程式碼 CRC、封包 payload CRC、client 的磁碟 SHA-1 均不同。

使用者已更正「1.0.37 與修改大師的地圖全開也會掉線」。因此不能繼續把二者當作穩定全圖基線，也不能推論修改大師已通過全部檢測。這個更正針對全圖，不自動證明 ControlAll 單獨使用的版本表現。

現有修改大師捕獲是在 `SessionType=5` 的 Skirmish，不能證明其跨過多人檢查。已有兩份原版 SYNC 來自不同對局，也不能拿來當同一場的雙端差分，或全歸到 CnCNet 的 E530 路徑。

來源：[發布版 Bugfixes.Desyncs.cpp](https://github.com/CnCNet/yrpp-spawner/blob/fbe8d7d2620584d04ba14fa3580943bfa1448d40/src/Misc/Bugfixes.Desyncs.cpp)、[修改大師全圖分析](2026-10-26-trainer-map-cheatcheck.md)、[既有原版同步研究](2026-10-26-network-event-audit.md)。

## 9. 對先前假說的修正與下一個實證缺口

**已提升為具體機制：** `50B6F0` 與 Hardened 的實體入口衝突；程式碼 CRC 可觀察其他無直接區間重疊的 Hook；命令接收端存在連線身分／目標控制權驗證；地圖／規則內容參與封包驗證。

**不能由現有證據推出：** 新版 Hook 時機一定錯誤；所有掉線都是反作弊；失配一律彈同步錯誤；晚段繪製 Hook 只畫畫所以一定不受檢；1.0.37／修改大師全圖是可靠穩定對照；三項資料快照等於全遊戲資料掃描。

下一步要建立現場因果，最有區分力的是同一對局的模組版本／DLL SHA-256、異常種類及時間、可用 crash call stack／同步日誌，和功能啟用前後的只讀站點狀態。先將現象歸為致命異常、外部 Kill、封包拒收／逾時、命令被拒或原版 FrameInfo CRC 失配，再沿對應路徑分析；不能只用「掉線」一詞合併。

## 持久證據與驗證

- [執行期與其餘檢查](evidence/cncnet-v17-runtime-checks.json)：精確入口、基準／掃描時序、欄位、原版對照、來源與限制。
- [CRC 表與 Hook 覆蓋](evidence/cncnet-v17-integrity-coverage.json)：256 項表比對、精確掃描範圍、20 個站點。
- [事件來源與失敗續行](evidence/cncnet-v17-event-flow.json)：接收連線 HouseIndex 閉環、dispatcher／decoder 返回、原版 CRC 分支。
- [地圖 helper 與繪製接續](evidence/cncnet-v17-map-controlflow.json)：原版／DLL 小片段、區間交集、暫存器與資源語義。
- [前輪 Hardened 二進位研究](2026-10-26-cncnet-hardened-binary-audit.md)：發布包、PE 表、50B6F0 初始化與 ApplyStatic 可達鏈。

只讀重現腳本與有限 objdump 片段保留在工作資料；本次持久化只含研究文字與精簡衍生證據，不提交官方 DLL、PDB、使用者 EXE 或 raw memory dump。沒有實機 Windows／多人驗證，沒有改寫反作弊、產品邏輯或發布新版本。

其餘少量字串、資料與跳轉 patch 尚未逐項完成語義分類；本報告記錄已閉環的主要機制，不宣稱是 Hardened 所有反作弊行為的完整清單。

## 10. 故意異常的可恢復性：破壞前與破壞後須分開

使用者後續詢問是否能旁路掉 Hardened 製造的異常。本次只讀追蹤 E530 的直接呼叫者與 Windows 例外上下文語義，沒有修改 DLL、寫入遊戲或實作／執行旁路。

### 10.1 四個已找到的直接呼叫者都有正常後續流程

| DLL call RVA | 原因 | call 之後已存在的流程 |
| --- | --- | --- |
| E70B | BuildingType 三欄位之一不符 | 續行 E710，增加迴圈索引與快照指標，繼續下一個類型 |
| E764 | CurrentPlayer 與保存指標不符 | 續行 E769，進行後續幀數／Observer 條件檢查 |
| E7AC | 已列明的 Observer／Defeated 條件 | 續行 E7B1，呼叫原版 55E160 並恢復保存的暫存器、返回 |
| EC2F | 程式碼區塊 CRC 不符 | 續行 EC34，更新掃描索引，恢復保存狀態，尾跳原有主迴圈函式 |

因此不能把此版本描述成「呼叫 E530 後，編譯器已移除所有正常返回路徑」。這四處均有實際可反組譯的後續；呼叫前的故障標記透過暫存器與 DLL 全域值傳遞，未見需要額外清理的 stack 參數。

**在 E530 尚未破壞上下文，且原有堆疊、非揮發暫存器與呼叫約定均保持的假設下，靜態控制流支持回到這些後續的可能性。** 這項結論不等於已在 Windows 上驗證，也不能證明其餘檢查、對端行為或遊戲狀態一致性都會正常。

### 10.2 事後「吞掉例外」無法自動還原

E530 的原有指令會先清空／重組 ESP，清零 EDI，改寫 EBP、ECX、EAX，最後以已清零的 EBP 跳到地址零。到例外真正發生時，原有上下文已有資訊丟失。

Microsoft 將 EXCEPTION_POINTERS.ContextRecord 定義為「例外發生時」的處理器狀態；它不會自動提供較早、尚未執行破壞指令時的快照。VEH 可以觀察或處理例外，但其註冊與呼叫機制本身不保證能重建先前的暫存器與有效 stack。

因此僅要求繼續執行，會嘗試使用原有故障上下文；僅變更指令位置，也沒有修復堆疊與活躍暫存器。具體例子是第一個 caller 的後續依賴 EDI 迴圈索引，而 E530 已把它清零；資料檢查尾端與程式碼掃描尾端均依賴有效 stack 來恢復保存狀態。

四個 caller 所設定的值經 OR 後，分別使 ESP 變為：E70B→`0x00A80220`、E764→`0x008A0DA0`、E7AC→`0x0084313D`、EC2F→`0x00A8B3B9`。這些是實際指令可推導的候選崩潰辨識值；只有在現場上下文確實保留它們時才有辨識意義，不能保證例外派送成功。

本次沒有取得實機例外記錄，也未驗證這個錯誤 ESP 下 Windows／既有 handler 的實際派送順序。不能宣稱我們的 handler 一定能被呼叫，更不能宣稱可以用通用的忽略 access violation 規則安全恢復。

### 10.3 能解決的範圍

只處理 E530 所代表的本機故障動作，不會改變已存在的 CRC／資料差異。未變更的檢查還可能再次觀察同一差異；資料檢查在後續有效呼叫再次比較，程式碼掃描在輪巡回該區塊時再次比較。

它也不會改變另一條 payload CRC 拒收路徑、HouseIndex／目標控制權事件拒絕、客戶端 Process.Kill，或原版 FrameInfo 狀態摘要。即使單獨處理了本機故意異常，仍不能承諾全圖／控制全部的聯機一致性。

所以本輪結論是：**有條件地處理破壞前的故障動作具有靜態控制流可行性；對破壞後例外做一般性吞錯，沒有安全恢復的證據；完整聯機相容性仍未實測。**

證據：[故障呼叫與返回流程](evidence/cncnet-v17-fatal-return.json)。Windows 語義來源：[EXCEPTION_POINTERS](https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-exception_pointers)、[Exception Dispatching](https://learn.microsoft.com/en-us/windows/win32/debug/exception-dispatching)、[Vectored Exception Handling](https://learn.microsoft.com/en-us/windows/win32/debug/vectored-exception-handling)。以上 Windows 文件只支持例外處理機制與上下文定義，不作為本 DLL 可安全恢復的實測證據。
