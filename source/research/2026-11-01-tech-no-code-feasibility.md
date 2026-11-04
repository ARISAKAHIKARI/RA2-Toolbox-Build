# 科技全開：不改程式碼的兩條候選路線

日期：2026-11-01。此頁補充 [純資料候選與還原責任](2026-11-01-tech-data-candidates-and-recovery.md)；沒有新增產品模式，沒有改 production source、正在執行的遊戲或私有 EXE。支援樣本 SHA-256：`268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0`。

結論：可以做不改 `.text` 的科技實現，不應籠統說「科技全開必須改程式碼」。具體有「改建造條件資料」及「工具箱獨立清單送原生生產事件」兩條；前者不是單一全部解鎖旗標，後者不等同完整原生側欄全科技。本輪只研究並隔離驗證必要條件，兩者尚未成為新模式。

## 原生條件與資料模式

以支援 PE 的 `HouseClass::CanBuild 0x4F7870`、`ObjectTypeClass::FindFactory 0x5F7900` 實際指令核對 [固定 YRpp TechnoTypeClass](https://github.com/Ares-Developers/YRpp/blob/d9795546950fcc75a94325ffc83b44d16ec34ce2/TechnoTypeClass.h) 與 [HouseClass](https://github.com/Ares-Developers/YRpp/blob/d9795546950fcc75a94325ffc83b44d16ec34ce2/HouseClass.h)：

| 條件 | 資料候選及邊界 |
|---|---|
| 科技等級 | Type+`0x634` 改成不高於 House+`0x1D4` 的非負值；`-1` 禁建。只改這個，不能解除下面其他條件。現有 `TechEnhance` 已對 21 個指定 Type 以資料寫入改成 1。 |
| 先決建築 | Type 的 Prerequisite 向量 Count+`0x648` 暫置 0，可略過此項；須保留原指標、容量、計數與還原責任，不偽造 House 的實有建築計數。 |
| 國別限制 | RequiredHouses+`0xDA0` 的 `-1` 表示無要求；ForbiddenHouses+`0xDA4` 可用 0 表示不禁止。原生還存在被偷／取得科技的替代國別位，不能把國別與 Owner 混為同一條件。 |
| 偷科技 | RequiresStolen 系列+`0xD9B..0xD9D` 可清除該需求；對應 House+`0x2BC..0x2BE`。不需偽造玩家已完成間諜行為。 |
| 禁建 | 已補實 PE 證據：Unbuildable 是 Type+`0xC98`，非零在最前面拒絕。只能對已驗證可建造的白名單類型考慮清除；不應把裝飾、特殊用途和缺素材的 Type 一律放進建造表。 |
| 工廠陣營相容 | `GetOwners 0x711EC0` 通常讀 OwnerFlags+`0x6CC`；FindFactory 要求產品與實有工廠的 owner mask 相交。可把相容工廠的位元加入產品 OwnerFlags。另 DoubleOwned+`0xC99` 在非戰役時令 GetOwners 回傳 `0x7FFFFFFF`，但它只解這項，戰役忽略此捷徑。 |
| 超武與特殊建築 | 完整 CanBuild 對 BuildingType 另有超武設定分支（讀 Type+`0x16F0` 及 House/Super/Rules 狀態）。本輪合成動態測例是 UnitType，沒有把此分支或所有 CanExpectToBuild/原生目錄篩選宣稱已覆蓋；不應為了顯示建築直接抹去其超武關聯或改全局對局規則。 |
| 建造限額 | BuildLimit+`0x3B8` 正數比現有數，負數比累計產量，**0 不是無限制，而是零個也不准造**。最小科技資料模式應保留限額；若另做解除限額須明確分項，不偽造統計。 |
| 刷新 | House+`0x1FC RecheckTechTree` 只要求重新評估，不是全科技開關。實際工廠類型、Naval、電力與生產流程仍須成立。 |

原生還有 `PrerequisiteOverride` 捷徑：Type+`0x654` 向量若列出本 House **真正擁有**的建築類型，CanBuild 會直接進入 BuildLimit 檢查，跳過普通 TechLevel、國別、偷科技及先決條件。但 Unbuildable 檢查在它之前，BuildLimit 在它之後，工廠條件仍獨立。隔離測試已驗證這個邊界。這可作另一種較少標量欄位的資料模式候選，但需要可靠保存／管理向量記憶體，不能把臨時緩衝指標留在原生 Type，或假造建築所有權。相比之下，標量模式與既有 TechEnhance 還原記錄較接近。

以上資料屬於同進程各玩家／AI 共用的 Type 規則。**不改 `.text` 不代表聯機同步安全**；本機改規則仍可能改變後續模擬。不能為了本機選單開放而悄悄修改 House 的人類玩家標誌、ActiveBuildingTypes 計數、Factory 向量或 Owner 身分。模式切換還須先完整撤回目前科技補丁，逐 Type 記錄身分／原值，只還原仍屬自己寫入的值；短失讀要保留還原責任。

## 原生事件模式：可做，但需拆開側欄依賴

原生 `Produce` 事件是 `0x0E`，由正常 OutList 派送；`0x10 Abandon` 可取消。它們不是「設定所有科技」事件。[YRpp EventClass](https://github.com/Ares-Developers/YRpp/blob/d9795546950fcc75a94325ffc83b44d16ec34ce2/EventClass.h) 和 [FactoryClass](https://github.com/Ares-Developers/YRpp/blob/d9795546950fcc75a94325ffc83b44d16ec34ce2/FactoryClass.h) 有對應宣告。

支援 PE 的 `House::Produce 0x4FA350` 在 `0x4FA438` 呼叫 FindFactory(false,true,true,house)，而 FindFactory 的 requireCanBuild 路徑在 `0x5F79A3` 實際呼叫 **CanBuild(type,true,true)**：第二參數是 buildLimitOnly，故不重新檢完整科技條件。本輪完整原生謂詞測試證實：即使 Type 同時為 TechLevel=-1、缺先決、國別不合、要求偷科技，仍可找到相容實有工廠；沒有工廠、類別錯、停電、海陸不合、Owner mask 不合或限額達到，仍會拒絕。後續 `0x4FA5D1` 進入 `FactoryClass::DemandProduction 0x4C9C70`。這支持「自己的清單 + 原生事件」候選，不需把本地 Type 條件改成全開。

目前 `bridge.cpp` 的 Produce 指令仍要求 UnsafeTech、techOpen、techAllowed，並呼叫 `ensureTechCameo`；`sidebar.h::refreshTech` 必須先安裝科技站點。因此不能只把現有補丁關掉就宣稱事件模式已完成。實作需拆出獨立清單／許可／生產觀察，保留真正工廠和限額檢查，再走原有事件提交；不能反覆灌事件。原生側欄還會以完整 CanBuild 裁剪不合法 cameo，部分路徑會發取消事件，故不能假設「只呼叫 AddCameo」足以保留全科技側欄。完整生產、排隊、取消、建築完成後放置、原生側欄互動及雙機同步，尚未做端到端驗證。本輪不擴成產品實作。

## 舊版與驗證範圍

現有經典／受控科技模式改兩個 CALL（`0x6A97D2/0x6AA781`）；Master 科技模式改 `0x44574A/445795/4457D0/445810/6AA788`，這些確實寫遊戲指令。不能用「Hook」一詞推論任何 Hook 都寫 `.text`，但也不能把這幾處實際 CALL 改寫說成純資料。

[固定 v0.7.20 sidebar.h](https://github.com/ARISAKAHIKARI/RA2-Toolbox-Build/blob/50f59d03fda6c37ce2c35927565cabb372d8e94e/source/assist/source/sidebar.h) 的 refreshTech 是停用 stub，回傳 `techError=-20`；它不是一份可直接取回的「完整科技純資料模式」。這項結論只針對該功能實現，不能拿來否定使用者觀察到其他舊版功能有效。

[公開結果](evidence/tech-no-code-native-predicates.json)：**31 項 PASS**，實際執行支援 PE 的 CanBuild、FindFactory、向量複製、計數讀取及 UnitType 的原生 WhatAmI/GetArrayIndex/GetOwners。只合成 House/Type/Building 資料、替代測試用 malloc/free 和建築當前任務 getter；沒有改遊戲程式碼。另有直接查閱 Produce／DemandProduction 指令鏈的靜態證據，但沒有執行完整生產收件與生成。結果不涵蓋 MOD 改寫、RA2OL 雙機同步或 UI。公開 JSON 只含合成測例、結果、位址與支援 EXE hash，不含私有原始位元組／反組譯 dump。
