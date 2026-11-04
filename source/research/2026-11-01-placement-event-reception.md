# 原生 Place 不改程式碼：接收控制流的後續核對

問題：任意建造是否有不改遊戲 `.text` 的實際路徑，而不是只能改距離判定分支？

**有。可實作的候選是工具箱獨立選點，對本方已完成的建築發送正常 Place 事件。** 對本次核對的普通建築，它可以越過原生 UI 的基地距離與陰影預覽門檻，接收端仍執行原生落地檢查。不需要改 `Adjacent`、`PlaceAnywhere` 或 Armageddon 全域狀態，也不需要新增遊戲程式碼補丁。這不等於在佔用格、不可通行地形硬放建築，更不會自動解除原生滑鼠游標的紅格限制。

本輪僅研究與隔離指令測試，沒有新增產品模式，也沒有修改 production C++、啟動／操控遊戲、修改私有 EXE 或進行 RA2OL 雙機測試。

## 範圍與來源

先查 [前篇研究](2026-11-01-placement-no-code-feasibility.md)、研究索引及目前事件入口。支援 EXE SHA-256：`268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0`。新結果由該 EXE 的原始 x86 控制流與 Unicorn 隔離執行交叉確認；不發布 EXE、dump 或反組譯全文。持久 SQLite 筆記本未取得，本文及 JSON 留作回寫。

固定 YRpp 參考：[EventClass](https://github.com/CnCNet/YRpp/blob/d9795546950fcc75a94325ffc83b44d16ec34ce2/EventClass.h)、[ObjectClass](https://github.com/CnCNet/YRpp/blob/d9795546950fcc75a94325ffc83b44d16ec34ce2/ObjectClass.h)、[BuildingTypeClass](https://github.com/CnCNet/YRpp/blob/d9795546950fcc75a94325ffc83b44d16ec34ce2/BuildingTypeClass.h)、[CellClass](https://github.com/CnCNet/YRpp/blob/d9795546950fcc75a94325ffc83b44d16ec34ce2/CellClass.h)。地址、欄位與控制流另以支援 SHA 實際指令核對，沒有僅依欄位名稱推論。

## 追到哪裡

原生構造器 `0x4C6AE0`（thiscall，六參數，`ret 24`）建立 `0x0B Place`。事件 dispatch `0x4C6CB0` 的 Place case 呼叫 House `0x4FB0E0`（thiscall，型別／heap index／naval／Cell 指標，`ret 16`）。後者取得已完成工廠的產物與建造工廠，才進產物 `Unlimbo`。

普通建築路徑為 `Building 0x440580 → Techno 0x6F6CA0 → Object 0x5F4EC0`；三者均接座標與朝向，`ret 8`。第一個 `CanEnterCell` 結果在 `0x4405AB` 產生，**不能當作普通建築的最終拒絕點**。在非 ScenarioInit 的正常路徑，Object 於 `0x5F4F3C` 再次呼叫 vtable `+0x1AC`；結果非零便返回失敗，不會進地圖登錄 `Mark`。這次檢查仍落在 Building `0x449440 → Type 0x464AC0 → foundation 0x716150 → Cell 0x47C620`。

Object 同時拒絕已在地圖上或不處於 Limbo 的物件。合法性通過後先設定位置，再呼叫 `Mark`；Mark 失敗會恢復 `InLimbo` 並返回失敗。House 只有在 Unlimbo 成功後，才呼叫工廠完成收尾 `0x4CA1A0` 與生產收尾 `0x4FAA10`，再進放置後處理 `0x4FB6B0`；失敗分支不走這三處。

本地失敗回饋可能重新呼叫預覽函式 `0x4A91B0`，因此不能宣稱「Place 接收後的所有 UI 回饋永遠不會碰距離／可見性」。其用途與接收端是否准許物件落地不同。

## 各限制的結論

| 限制 | 已確認 | 邊界 |
| --- | --- | --- |
| 基地距離 | 本地 `0x4A8EB0` 拒絕的遠處，正常 Place 可進成功路徑；接收落地鏈不重新執行這個 gate。 | 原生游標仍會拒絕，須獨立選點入口；不是直接接管原生左鍵。 |
| 陰影可見性 | 本地 `0x4A9070` 拒絕的陰影位置，同一物理合法地面可經 Place 成功返回。 | 陰影檢查 `0x586360` 與支援 EXE 的既有轉送均實際執行；不是僅用「可見」替身。特殊建築未全覆蓋。 |
| 地面、佔用 | 原生 Cell 的佔用位、Land/Speed 通行表、可用地圖範圍拒絕仍使事件失敗；初次通過、第二次變成佔用也會失敗。 | 不提供重疊建築、硬放水面／悬崖或地圖外的等價性；不是所有特殊地形完整驗收。 |
| 產物與工廠 | 工廠未完成不進 Unlimbo；落地失敗不執行完成收尾。 | 工廠查詢及收尾本體使用替身；沒有真實資金、工廠物件與後續 AI 驗收。 |
| `PlaceAnywhere +0x1703` | 開它會改變原生接收端對佔用地面的結果。 | 這是模擬規則資料寫入，單方改寫仍可能不同步，並非本候選所需。 |

## 新測試

工具：[audit_placement_event_reception.py](../tools/audit_placement_event_reception.py)，結果：[placement-event-reception-audit.json](evidence/placement-event-reception-audit.json)，**12 checks PASS**。

測試使用原生 ctor、dispatch、House、三層 Unlimbo、Type 與 Cell 合法性，跑到正常返回；包含近處、遠處、陰影本地／遠端視角、佔用、不可通行地面、地圖範圍拒絕、Mark 失敗、未完成工廠、兩次檢查間變成佔用、PlaceAnywhere 改變結果、已在地圖上的物件。每例都核對成功才收尾，並核對模擬器內可執行 section 未被改寫。

仍使用合成普通 1×1 建築。工廠服務、地圖查格、佔用物件列舉、可用範圍查詢、Mark 地圖登錄、位置調整、House 帳務、投影、radio、音效及呈現為明示替身。測試證明分支與後續呼叫條件；**不證明真實建築已登錄地圖、真工廠已消耗、事件已入網路佇列或雙機一致**。海軍工廠、牆、覆蓋物、升級附件與特殊地基需另測。

```text
python source/tools/audit_placement_event_reception.py --exe <supported-private-exe> --output source/research/evidence/placement-event-reception-audit.json
```

依賴 pefile、unicorn，以及同目錄的既有 `audit_placement_no_code.py`。不需要遊戲進程或私有 EXE 的寫入權限。

## 最小產品化範圍

目前 `vanilla_constructor_catalog.h` 的 Place 地址已存在，`constructor_adapter.h::fill` 也已有正確 ABI，但仍是 `ProbeOnly`，沒有專門的 Place 產品欄位驗證。不能只把旗標改成可發送。應新增「本方已完成建築＋選定格」語義請求，核對型別／heap index／naval、工廠與產物身份、世界 epoch、座標與地基，並在真正入隊前重驗。

既有 `bridge.cpp::submitConstructed → enqueue` 可承接：遊戲執行緒與世界就緒門檻、原生時間來源、佇列容量、單機 DoList／聯機 OutList、回執皆已有。普通本方 Place 不需要 CommandOwner、NetworkHousePreserve 或任意建造判定補丁。Runtime 的 Windows 訊息 Hook 用於取得遊戲執行緒呼叫時機，本身不等於改遊戲 `.text`；保留這種呼叫入口，仍可做到本功能不改程式碼段。

證據足以開始一個**保留原生地面合法性的獨立超距離／陰影放置模式**；不足以把現有任意建造全部替換並宣稱已驗收。下一步是普通建築的真地圖／工廠收尾、排隊後被佔用或工廠消失、讀檔換局取消與雙機一致性驗證。RA2OL 外層驗證是否另有限制，仍未證實；沒有 `.text` 改寫不能保證平台接受所有事件。
