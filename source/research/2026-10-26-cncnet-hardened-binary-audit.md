# CnCNet v0.0.0.17 Hardened：ControlHouse 重疊與額外 House 驗證

> **後續補充（同日）：** 已追出執行期程式碼 CRC→致命異常、三個 BuildingType 欄位與 CurrentPlayer 檢查、地圖／規則內容衍生的封包驗證；也完成以下事件 continuation 的來源與失敗語義。詳見 [反作弊原理](2026-10-26-cncnet-anticheat-principles.md)。本文保留前輪 PE／同址覆寫證據，以下先前未知項由新研究補足。

本次只讀分析證實兩件事：**官方 Hardened DLL 的 `0x50B6F0` 五位元組 patch 與 Toolbox 的 ControlHouse 入口完全重疊；Hardened 另有普通版沒有的 House／控制者驗證程式。** 這些是固定版本二進位中的事實，尚未對應使用者實際載入的版本，也不構成即時掉線原因的證明。

本次分析使用官方發布包，發布標籤固定為 `fbe8d7d2620584d04ba14fa3580943bfa1448d40`，YRpp gitlink 為 `b22f1860dd7b100cad57123d4ca5d459b9020d88`。Hardened 由公開工程匯入 Private/SpawnerPrivate.props，公開 gitlink 為 `b8821e2f897a72e8872b7fa4e823200e9e825644`；沒有讀取私有原始碼。

官方下載：[v0.0.0.17 release](https://github.com/CnCNet/yrpp-spawner/releases/tag/v0.0.0.17)。普通包 ZIP SHA-256 為 `d0d329c8d4db03f297a44e50c726df1ec97e086ae20079a08ccf0e15351fb9ba`；Hardened 包為 `1100bb556025d80054371fe9c79f1130925fe8e142fdebd7bd63a2472429028d`。

## 樣本與證據級別

| 樣本 | 大小 | SHA-256 |
|---|---:|---|
| 官方 CnCNetYR v0.0.0.17 | 233472 bytes | `58f198973bd0fd3b0e1b2c42fc6c545efe1fd399680abc2e8d6e9e189751614d` |
| 官方 CnCNetYR-Hardened v0.0.0.17 | 417792 bytes | `62466e9e534ceb15c5d71fb05c62ad98a2b2b4903c569ed0c82189ef86ea3b7d` |

下載 ZIP 已核對官方 release asset digest；另以實際 DLL 雜湊固定樣本。所有 RVA/VA/檔案 offset 都由該 Hardened DLL 解析；VA 以首選 ImageBase `0x10000000` 表示，ASLR 後以實際 base 重定位。

| 級別 | 本次涵義 |
|---|---|
| 二進位事實 | 檔案 bytes、section descriptor、export、狹窄的靜態指令與控制流 |
| 公開原始碼對應 | release-pinned 的結構欄位、函式名稱與初始化流程 |
| 有條件推論 | 正常載入成功並到達所註冊遊戲 hook 時的行為 |
| 尚未證明 | 使用者版本、現場 bytes、模組載入順序、實際命中與掉線因果 |

## 1. 13 個 Toolbox 站點中，明確重疊的是 ControlHouse

普通版 `.syhks00` 有 196 個 hook，`.patch` 有 49 筆；Hardened 有 213 與 95 筆，descriptor 差集為新增 17 個 hook、46 筆 patch，沒有刪除。hook 比較鍵為 address/size/name，patch 為 address/size；這不表示同名同站項的函式本體完全相同。

對 Toolbox 的 13 個指定實體覆寫區間求交集，普通版為 0，Hardened 僅 `0x50B6F0..0x50B6F4`。其 descriptor 在 DLL 檔案 offset `0x63408` / RVA `0x76408`，12 bytes 為：

```text
f0 b6 50 00 05 00 00 00 c8 3c 07 10
```

依 [release Patch 結構](https://github.com/CnCNet/yrpp-spawner/blob/fbe8d7d2620584d04ba14fa3580943bfa1448d40/src/Utilities/Patch.h#L25-L35)，這是 `offset=0x50B6F0, size=5, pData=0x10073CC8`。`pData` 位於未由檔案 bytes 覆蓋的資料虛擬區，不能把檔案中的零填充當成最終 patch。

初始化器 RVA `0x1350`、檔案 offset `0x750` 會填入 `E9` 與相對位移，目標為 DLL RVA `0xEC50`。首選 base 下生成 `E9 5B 35 B0 0F`；實際公式是：

```text
E9 + uint32(loaded_DLL_base + 0xEC50 - 0x50B6F5)
```

RVA `0xEC50`（檔案 offset `0xE050`）的函式：Campaign 以外模式只接受 `this == HouseClass::CurrentPlayer`；Campaign 模式接受 `IsHumanPlayer || IsInPlayerControl`。這與 [YRpp HouseClass::IsControlledByCurrentPlayer](https://github.com/Phobos-developers/YRpp/blob/b22f1860dd7b100cad57123d4ca5d459b9020d88/HouseClass.h#L500-L506) 逐句對應。注意 `GameMode==0` 指 Campaign，不能泛稱所有單機模式；`+0x1EC` 欄位名稱是 `IsHumanPlayer`。

Toolbox 的同一入口轉向 `riskControlEntry`，該函式一律返回 `EAX=1`。兩個不同實作不能同時直接佔據同一五位元組入口。pre6 `knownOriginal` 僅接受原版五位元組與兩個固定舊 forwarder，沒有任意外部 DLL detour 相容機制；Hardened 在首選 base 的生成 bytes 不在該集合內。因此先安裝 Hardened 的通常預期是 Toolbox 的相容性檢查拒絕這組控制 hook，並不等於必然掉線。若另一模組後續改寫入口，結果仍須以現場 bytes 與狀態判斷。

## 2. 初始化與 Patch.Apply 的可達條件

CRT 初始化陣列範圍是 VA `[0x100281A4,0x10028258)`，其中 `0x1002823C` 的指標為 `0x10001350`。PE entry RVA `0xFF6A` 經 reason dispatch 與 process-attach 路徑，到 RVA `0xFCF7` 呼叫 initializer walker `0x1A2CE`；該 walker 按四位元組走訪並呼叫非零項。因此成功的正常 DLL attach 會生成這組 patch bytes。

之後，[Main.Hook.cpp](https://github.com/CnCNet/yrpp-spawner/blob/fbe8d7d2620584d04ba14fa3580943bfa1448d40/src/Main.Hook.cpp#L23-L35) 在遊戲 `0x7CD810` 註冊 `ExeRun`，無條件呼叫 `Main::ExeRun`；[Main.cpp](https://github.com/CnCNet/yrpp-spawner/blob/fbe8d7d2620584d04ba14fa3580943bfa1448d40/src/Main.cpp#L30-L35) 首句為 `Patch::ApplyStatic()`。

[Patch.cpp](https://github.com/CnCNet/yrpp-spawner/blob/fbe8d7d2620584d04ba14fa3580943bfa1448d40/src/Utilities/Patch.cpp#L42-L68) 逐筆遍歷 `.patch`，依 `VirtualSize` 或 offset0 終止，直接執行 `VirtualProtect`、`memcpy`、還原保護與 `FlushInstructionCache`。此迴圈沒有逐項功能開關與原 bytes 比較。Hardened export `ExeRun` RVA `0x16A0` 的機器碼也符合這個流程。

以上證明正常初始化與該遊戲入口被執行時會套用；沒有證明特定使用者機器載入成功、該 hook 被執行，或之後未被其他模組覆寫。

## 3. 額外控制者檢查：10 個事件站點共用一個 export

Hardened 新增的 `KD873HWYFB7DN36G67DUY4BGT32GD` export 位於 RVA `0xEEE0`，被 10 個 `0x4C6xxx/0x4C7xxx` hook 共用。它從 hook captured EAX 取 object，captured ESI 取 event；object 為 null 時返回 continuation `0x4C8109`，否則呼叫原生 `0x6339B0`，與 event 的 signed byte `+2` 比較。相等返回 0，不等返回 `0x4C8109`。

原生 `0x6339B0` 是 [TechnoClass::GetControllingHouse](https://github.com/Phobos-developers/YRpp/blob/b22f1860dd7b100cad57123d4ca5d459b9020d88/TechnoClass.h#L497-L499)，event `+2` 是 [EventClass::HouseIndex](https://github.com/Phobos-developers/YRpp/blob/b22f1860dd7b100cad57123d4ca5d459b9020d88/EventClass.h#L154-L157)。這是額外 House 驗證的直接證據。

不能把此檢查說成不可受 Toolbox 影響的原始 Owner 檢查：Toolbox 的 `0x6339BA` hook 正位於 `GetControllingHouse()` 中。後續已確認 `0x4C8109` 是 dispatcher 一般返回，caller 仍消耗事件，直接效果為拒絕當次動作，並不直接走原版同步錯誤。完整證據見 [事件流程](evidence/cncnet-v17-event-flow.json)。另一新增 export RVA `0xEF10` 現已映射為 Detonate 事件驗證：`ObjectClass::AttachedBomb(+0x38)` → `BombClass::OwnerHouse(+0x28)`；LAN／Internet 下比較炸彈施放方 House index 與事件 `+2`，失配同樣回 `0x4C8109`。它的身分來源與目標的 GetControllingHouse 不同，詳見後續事件流程證據。

## 4. 額外 House ID 檢查，與 CRC 分開

Hardened 在 `0x64A0E4` 新增 hook（export RVA `0xEE20`），保存 captured EDX。`0x64BE75` 的新增 hook（export RVA `0xEE30`）比較該值與 signed byte `[captured ESP+0x3E]`；相等返回0，不等記錄包含「Forged house ID detected」的訊息並返回 `0x64C370`。

這足以確認額外 House ID 檢查存在。後續已將來源 EDX 閉環到接收連線建立時綁定的 HouseIndex，stack `+0x3E` 為解壓後 EventClass.HouseIndex；`0x64C370` 返回已接受前綴數量並停止當次解碼，前綴不回滾。完整證據見 [事件流程](evidence/cncnet-v17-event-flow.json)。這段是整數／House 身份比較，不能與地圖合法性、全圖繪製或遊戲 CRC 一致性混為同一項檢測。

## 5. 地圖特殊格修復與 Hardened 繪圖差異

普通版與 Hardened 的.patch **都有5C0E30、size2、資料B001**，使原版 SkipCheatCheck 回傳true。這是官方共用的specific-cell shroud desync修復，不是本次修改大師capture中新做的繞過；也不代表原版同步摘要或全部Hardened驗證關閉。公開來源：[發布版Bugfixes.Desyncs.cpp](https://github.com/CnCNet/yrpp-spawner/blob/fbe8d7d2620584d04ba14fa3580943bfa1448d40/src/Misc/Bugfixes.Desyncs.cpp)。

Hardened新增五個map/shroud helper：

| 遊戲入口 | DLL helper RVA | 窄範圍靜態語義 |
| --- | --- | --- |
| 47EFF1 | ECB0 | 令SHROUD/FOG指標指向DLL內嵌SHP，續行47F018 |
| 47F25E | ECE0 | 更換FOG指標，續行47F274 |
| 58647A | ED00 | 搬移原有Cell+12C的0x08判斷與epilogue，直接返回；無新增判斷 |
| 6D88FE | ED30 | 設EDI=-2、EBX=8，續行6D8908 |
| 6D8903 | ED50 | 設EBX=8，續行6D8908 |

兩份內嵌SHP均為60×30、96幀；SHROUD第15幀為900個值2與900個FE，FOG為900個值66與900個FE。完整窄helper未見CRC比較、網路發送、上報或退出呼叫。原版逐條對照現已確認：前兩段改用內嵌資源，後三段保持原有語義而搬移指令；這五段本身不是「發現全圖即踢人」流程。另已在DLL其他入口確認程式碼／資料完整性比較及致命失敗路徑，詳見 [反作弊原理](2026-10-26-cncnet-anticheat-principles.md)。

五個trainer全圖覆寫區間（508E60/2、508EBA/2、508F8F/2、577B3A/2、6D88FC/2）與兩DLL表中的區間沒有直接相交，但 **6D88FC後的+2及+7已被Hardened改寫**。本產品direct_patch_policy.h明確比對其後12bytes原始上下文，所以在這些外部補丁仍生效的前提下，該固定上下文不再匹配；這是源碼／bytes層面的預期拒絕，不是實測掉線。也不能由兩bytes不相交推論整個功能相容。

修改大師全圖capture的SessionType=5、檢查caller不變、House旗標快照與顯示／Reveal區別，另見 [全圖與特殊格檢查](2026-10-26-trainer-map-cheatcheck.md)。數值證據：[CnCNet map/shroud](evidence/cncnet-v17-map-shroud.json)。

## 結論邊界與可重現材料

這次可以把「Hardened 可能與控制全部衝突」提升為「官方固定版本二進位存在完全相同入口的實體覆寫，且有額外 House 驗證」。但 v1.0.37 與 v1.0.38 的舊三個控制站點相同，因此這個發現本身不能解釋工具版本回歸；仍須把兩次遊戲測試使用的平台 DLL／載入狀態對上。

沒有執行 DLL、安裝 hook、修改反作弊或實作規避。本次也沒有宣稱掃描涵蓋所有動態 patch；`.syhks00/.patch` 的實體區間求交集不能排除其他語義衝突。

持久證據：[ControlHouse／事件驗證](evidence/cncnet-v17-control-event.json)、[map/shroud](evidence/cncnet-v17-map-shroud.json)。其中保留精確RVA／VA／file offset、狹窄機器碼與來源；不提交官方DLL、PDB或使用者原始遊戲映像。完整PE表與objdump材料只用於本輪只讀核對。

前次 [公開來源與Hook管理審計](2026-10-26-hook-conflicts-cncnet-checks.md) 的「公開來源未見13站點交集」仍限定於當時掃描範圍；本輪實際Hardened二進位已提供該公開掃描未涵蓋的50B6F0重疊證據。

