# 科技純資料候選與還原責任

基線：`09455e9e9929ed118689c1d5a82718ddb1b2789c`。支援樣本 SHA-256：`268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0`。Library 中的持久 SQLite 筆記本本次不可存取，保留此頁供回填。

## Hook 和程式碼改寫分開判斷

Windows 訊息 Hook 可在原有遊戲執行緒進入命令流程，不必改寫遊戲指令。IAT／虛表攔截修改指標資料，也不能直接等同 `.text` 改寫。但現有科技經典模式的 `0x6A97D2`、`0x6AA781` 是直接呼叫 `0x4F7870 CanBuild` 的 E8 指令，現實作確實改寫兩個 CALL；只替換 IAT 或虛表不會攔到這兩條直接呼叫。另一種科技模式還涉及 `0x44574A/445795/4457D0/445810/6AA788`，不能說已有純資料等價版本。

## 純資料候選尚未實作

依既有 `construction_catalog.h`、[固定 YRpp TechnoTypeClass](https://github.com/Ares-Developers/YRpp/blob/d9795546950fcc75a94325ffc83b44d16ec34ce2/TechnoTypeClass.h) 與 [HouseClass](https://github.com/Ares-Developers/YRpp/blob/d9795546950fcc75a94325ffc83b44d16ec34ce2/HouseClass.h)，可以調整建造條件資料，並非技術上不可能：

- Type+0x634 TechLevel；Prerequisite 的 Count+0x648；OwnerFlags+0x6CC。
- RequiredHouses+0xDA0、ForbiddenHouses+0xDA4、RequiresStolen 系列+0xD9B..D9D。
- House+0x1FC RecheckTechTree 只是要求重新檢查，不是全部科技解鎖位。

這不是類似 IsInPlayerControl 的單一旗標。Type 是此進程各玩家／AI 共用的規則，直接修改仍可能改變模擬結果；不能以「沒有改程式碼段」推論聯機同步安全。工廠、BuildLimit、資金電力等仍須保留，不應偽造建築計數或 Factory 向量。Unbuildable 的精確寫入偏移及所有 CanBuild／CanExpectToBuild 分支尚未完成驗證，AIBasePlanningSide 也不能憑另一函式的註解擴大解釋。套用前還須完整保存每欄原值／Type 身分、解除現有科技站點所有權，並確認只還原自己仍擁有的值。

另一候選是工具箱自己的清單直接送原生 Produce 事件，不改 Type／遊戲程式碼；但現有 OpenTech／Produce 與 native cameo 耦合需要拆開，建築完工後的放置流程尚無外部等價實作。因此本輪沒有新增並宣稱「純資料全部科技」已完成。原科技一次完整刷新、獨立開關均保留。

## 本輪已修的實際還原錯誤

原 `tech_enhancement_bridge.h::enhancementWorld` 把 command epoch／CurrentPlayer 改變當成舊 Type 已退休。短暫目錄失讀後重新建立命令識別號，可能丟掉同一局仍存活 Type 的原值，並且只用啟用時幀數無法識別較晚的存檔回退。

現在按 Scenario、Session、最近觀測幀水位及原有逐 Type 身分判斷。相同世界中的 command epoch／House 變化，先還原舊記錄再綁定；無法讀取時保留還原責任，禁止重新套用。真正換世界／回退才退休舊身分，不向舊指標寫回。幀水位在每輪更新，不受 500 ms 科技掃描節流影響。

`assist/tests/tech_enhancement_world.py` 執行擷取的正式 world／restore／tick 函式與正式記錄模板：舊版 34 項中 10 項失敗，新版 34 項通過；`tech_enhancement_recovery.cpp` 亦通過。這是受控記憶體 fixture，不是 RA2OL 雙機驗證。本修正隨渲染／生命週期新版一起編譯，未改 Assist85 的共享 ABI。
