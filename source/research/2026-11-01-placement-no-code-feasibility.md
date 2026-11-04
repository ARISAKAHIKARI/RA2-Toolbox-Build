# 任意建造不改程式碼段：距離、地面與原生事件

問題：現有「任意建造」能否改用資料或原生事件，不修改遊戲 `.text`？本輪只研究，保留既有產品行為，沒有新增模式或放寬命令門檻。

結論：**原生 Place 事件是可繼續實作驗證的超距離建造候選；尚未接入或完成實際建造驗收。** `BuildingType::PlaceAnywhere` 確實存在，但它繞過的是物件落地合法性，不是本地建造距離；單方修改它會改變接收端模擬判斷，不能作為聯機安全替代。

後續：[完整接收控制流與第二次落地檢查](2026-11-01-placement-event-reception.md) 已將普通建築的受控執行延伸至正常返回。注意本篇 12 例的 `0x4405AB` 初始結果不是最終拒絕門檻：普通建築仍會進入基類 `Object::Unlimbo`，在 `0x5F4F3C` 再次檢查。新測試補足此區分，但地圖登錄／工廠副作用仍有明示替身，沒有真遊戲或雙機驗收。

## 證據範圍

支援的私有 EXE SHA-256：`268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0`。只讀取既有 EXE，沒有啟動遊戲、注入或修改檔案。新證據分為原始 x86 靜態控制流與 Unicorn 受控執行；未進行 Windows 遊戲、RA2OL 或雙機測試，不發布 EXE、原始 dump 或反組譯全文。

已先查閱研究索引與既有 [直接模式研究](2026-10-25-direct-modes-138-pre1.md)。持久 SQLite 筆記本目前未能取得，本紀錄及 JSON 保留供後續回寫。上游欄位與已命名 ABI 參考固定 [YRpp commit d979554](https://github.com/CnCNet/YRpp/tree/d9795546950fcc75a94325ffc83b44d16ec34ce2)，不是僅憑欄位名稱推斷行為。

## 現有與舊版實作

目前 `assist/source/placement_bridge.h` 的受控模式在 `0x4A8EB0` 安裝跳轉，對本方 BuildingType 返回通過；直接模式在 `0x4A8EC3` 與 `0x4A918E` 改分支，分別涉及距離與地基可見性。兩者都寫遊戲程式碼，不能因為叫 Hook 就算作只改資料。

本地已取得的 v0.7.20 `source/assist/source/placement_bridge.h` 明確只回報 `BuildAnywhere ? -20 : 0`，沒有安裝合法性繞過。其他功能在該版有效，不能據此證明任意建造已有無 patch 的等價實作。

## 三個不同的門檻

| 層次 | 位址／ABI／欄位 | 已確認行為 |
| --- | --- | --- |
| 本地距離 | `0x4A8EB0`，Display thiscall，四參數，`ret 16` | 對本方依 `BuildingType +0xEB4` 的 `Adjacent` 搜尋附近可延伸基地的建築；不是 `PlaceAnywhere`。 |
| 本地地基可見性 | `0x4A9070`，Display thiscall，四參數，`ret 16` | 普通建築逐地基格檢查可見性。預覽在 `0x4A94D5` 呼叫，結果寫 Display `+0x1181`。 |
| 物件落地合法性 | `0x464AC0`，BuildingType thiscall，`(CellStruct*, HouseClass*)`，`ret 8` | `+0x1703 PlaceAnywhere` 為真便直接通過；否則進 `0x716150`，依地基呼叫 `0x47C620 Cell::CanThisExistHere`。 |

距離搜尋還依既有建築 Owner `+0x21C`、House index `+0x30`、既有類型 `+0x154F BaseNormal`；盟友分支涉及 `+0x1550 EligibileForAllyBuilding` 與 `0x4F9A50` 的同盟判斷。提高 `Adjacent` 的確可擴大距離，但仍需合格基地，並不解除地面或可見性限制。它是共享類型規則寫入，極大範圍也會擴大游標檢查的搜尋成本，沒有聯機安全驗收。

全域 `0xA8ED6B` 對距離及可見性有直接放行作用，但 [YRpp Unsorted.h](https://github.com/CnCNet/YRpp/blob/d9795546950fcc75a94325ffc83b44d16ec34ce2/Unsorted.h) 將它命名為 `ArmageddonMode`。它還參與其他引擎分支，不能當成單一「任意建造」資料開關。

欄位及 ABI 參考：[BuildingTypeClass.h](https://github.com/CnCNet/YRpp/blob/d9795546950fcc75a94325ffc83b44d16ec34ce2/BuildingTypeClass.h)、[DisplayClass.h](https://github.com/CnCNet/YRpp/blob/d9795546950fcc75a94325ffc83b44d16ec34ce2/DisplayClass.h)、[CellClass.h](https://github.com/CnCNet/YRpp/blob/d9795546950fcc75a94325ffc83b44d16ec34ce2/CellClass.h)。具體欄位偏移及控制流另以此 SHA 的原始指令核對。

## 原生 Place 事件候選

原生 UI 在預覽及滑鼠放開時呼叫距離檢查（直接呼叫候選 `0x4A9480`、`0x4ABA59`），把結果記入 Display `+0x1180`；放開時又檢查 `+0x1180/+0x1181`，之後才在 `0x4ABB55` 呼叫 `0x4C6AE0` 構造 Place 事件並入 OutList。事件類型是 `0x0B`，構造 ABI 可參考 [EventClass.h](https://github.com/CnCNet/YRpp/blob/d9795546950fcc75a94325ffc83b44d16ec34ce2/EventClass.h)。

接收執行路徑不同：`0x4C6CB0` 事件 dispatch 的 Place case `0x4C70E1`，在 `0x4C710B` 呼叫 House 的 `0x4FB0E0`，thiscall 四參數 `(AbstractType, heapId, naval, CellStruct*)`。它檢查工廠完成狀態（`0x4CA130`）、取得產物（`0x4CA160`），再經產物 vtable `+0xD8` 進 Building `Unlimbo 0x440580`。這不是直接生成任意物件或免付費建造。

`0x440580` 的最初落地檢查經 Building vtable `+0x1AC` 到 `CanEnterCell 0x449440`（五參數，`ret 20`），普通建築再經 Type vtable `+0xA8` 到 `CanPlaceHere 0x464AC0`。受控執行的這段接收路徑沒有重入上述 Display 距離／可見性 gate，卻仍走原本地面合法性。故工具箱自行選點、送出正常 Place 事件，是不用改 `.text`、`Adjacent` 或 `PlaceAnywhere` 的超距離候選。

這並不會自動改變遊戲原生游標的紅綠判斷或滑鼠放開門檻。需要獨立放置入口，而且不能承諾水面、佔用格或其他非法地形也能建造。也不能僅由初始落地檢查推論後續完整建造一定成功。

目前 `vanilla_constructor_catalog.h` 的 `Domain::Place` 仍為 `ProbeOnly`；診斷構造器不是產品發送通道。最小後續範圍應包括：已完成工廠／產物身份確認、合法型別與座標、正常本方事件入隊、世界／佇列生命週期及失敗回饋，然後驗證完整放置與雙機結果。RA2OL 的外層驗證也仍未知。本輪沒有啟用這條候選路徑。

## 受控測試與未解邊界

工具：[audit_placement_no_code.py](../tools/audit_placement_no_code.py)。結果：[placement-no-code-audit.json](evidence/placement-no-code-audit.json)，12 checks PASS。它只接受上述 SHA，映射原始指令；在資料區建立合成物件，依賴函式使用明示替身，不改引擎程式碼。

- 四例距離：近處通過、遠處拒絕、提高 `Adjacent` 後遠處通過、只開 `PlaceAnywhere` 的遠處仍拒絕。
- 四例 Type 落地判斷：未開 `PlaceAnywhere` 時依 Cell 合法性替身結果；開啟時跳過該判斷。
- 四例 Place 接收路徑：未完成工廠不進 Unlimbo；已完成工廠遇合法／非法地面得到原生移動結果 `0/7`；開 `PlaceAnywhere` 可使非法地面的初始判斷變成 `0`。這四例沒有呼叫 Display 距離／可見性 gate。

最後四例在 `0x4405AB`、即 Unlimbo 初始 CanEnterCell 結果產生後停止；工廠服務、物件取得、幾何及 Cell 合法性是替身。**測試未完成建築生成、網路入隊、資金／工廠收尾或雙機同步。** 單方 `PlaceAnywhere` 改寫可使不同客戶端對同一事件的合法性分歧；即使事後恢復該 byte，也不會撤銷已發生的不同模擬結果。

重跑（需 Python 的 pefile 與 unicorn，不包含私有 EXE）：

```text
python source/tools/audit_placement_no_code.py --exe <supported-private-exe> --output source/research/evidence/placement-no-code-audit.json
```

排除的誤解：`PlaceAnywhere` 不等於建造距離、原生事件不是免除所有落地條件、構造成功不等於已入隊或已完成建造、受控指令測試不等於 RA2OL 安全。下一步優先完整驗證事件候選，不把單方共享 Type 規則改寫包裝成安全替代。
