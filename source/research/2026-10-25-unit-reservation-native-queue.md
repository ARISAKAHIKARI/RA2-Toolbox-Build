# 單位預定與原生 FactoryClass 隊列

## 問題與適用版本

步兵、載具、飛機和艦船的預定在原有工具中等到工廠完全空閒，才每次提交一個；原版工廠能在生產中保留待建單位。目標映像記錄為受支援 `gamemd.exe` SHA-256 `7b8a068535d6af06845edf95ae829b113d00c02909330e16f197426cd7db94b6`。此項尚未進行對該映像的實機逐幀記憶體觀察，測試層級為公開結構定義核對、項目原有事件路徑與合成記憶體執行。

## 欄位與提交路徑

- [YRpp FactoryClass.h](https://github.com/Ares-Developers/YRpp/blob/master/FactoryClass.h) 定義 `QueuedObjects` 在 `Object` 前，`Object` 為 `FactoryClass +0x58`，`Owner` 為 `+0x6C`；[ArrayClasses.h](https://github.com/Ares-Developers/YRpp/blob/master/ArrayClasses.h) 的 `DynamicVectorClass::Count` 在向量起點 `+0x10`。由 32 位布局推出 `QueuedObjects.Count` 為 `FactoryClass +0x50`，未生產佇列；`+0x58 != 0` 是現製品。代碼原有建築忙碌判斷也已讀這兩欄。
- 屋主的工廠指標：步兵 `House +0x53B0`、地面載具 `+0x53B4`、海軍 `+0x53B8`、飛機 `+0x53AC`。這些相同類別偏移既存於 `bridge.cpp` 的 `ProduceDeferred` 路徑，又用於 Host 容量核算。
- 原有 `nativeProduceEvent` 使用單筆原版 `EventClass::Produce (0x0E)` 及統一 `submitConstructed`，Runtime 每幀的 actor 仲裁與原生 OutList/DoList 入隊仍生效。Host 成功 ACK 後前移 FIFO，無需等待現製品完工。每個原生工廠以「`QueuedObjects.Count` + 現製品 + 已 ACK 尚未反映的事件」保守計算 30 項上限；`29+1` 已滿時保留待提交尾項。建築、防禦保留空工廠及放置後再提交。

## 依據與界限

- `source/assist/tests/construction_host_test.py` 用實際 Host service/Book 與合成記憶體測試 30 項、31 項拒絕、29 待建加 1 現製品、未反映 ACK、四種工廠、階段取消及原有建築五項。這是自動測試，不是實機原生事件消費或聯機驗證。
- 預定 30 項是面板政策，並非已反組譯確認受支援 EXE 的 `MaximumQueuedObjects` 設定。原生遊戲設定／模組可更改容額；Runtime 的原版資格檢查與事件消費仍是最終判定。Host 為避免超過使用者需求，採用上限 30，佇列滿時待命。
- 曾考慮直接把 30 項繪成 30 張叠圖，會超過 640×400 遊戲視窗高度；現在僅畫最近五項與總數，FIFO 仍完整保留。也排除一次自建多個 Produce 事件的方案，使用已有一筆一 ACK 的協議，避免重覆提交和繞過每幀仲裁。
- 未決：需要在受支援的原版 EXE 中驗證不同模組／聯機延遲下的隊列消費時序、`MaximumQueuedObjects` 實際值和原生取消交錯後的動態數量。此前不能把合成測試當成實機生產完成證據。
