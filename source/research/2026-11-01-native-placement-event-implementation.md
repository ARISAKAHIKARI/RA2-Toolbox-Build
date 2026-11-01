# 原生事件放置：產品接入與驗證界線

本輪把 [原生 Place 接收研究](2026-11-01-placement-event-reception.md) 接成明確選項：`placementImplementation=2`。此模式先撤回既有任意建造補丁，完成後 `placementStatus=2`；仍有補丁責任、簽名不一致或衝突時拒絕提交，不回退至 Hook／直接 patch 模式。

協議 87 新增 `PlaceCompletedBuilding`，保留舊 command 數值；Snapshot 列出普通建築／防禦兩個工廠的已完成產品。`nativeplacement::Offer` 為 72 bytes、`Request` 為 104 bytes，均為 8-byte alignment 的 standard-layout trivial 結構。Host 選擇特定產品，按配置快捷鍵時凍結產品身份與遊戲客戶區座標；不使用泛用命令排隊或自動重試。

## Runtime 契約

- `native_placement_capture.h` 雙次 checked read，核對 House、Factory、Product、Type 各 pointer／UniqueID、Factory Owner、Product Owner、血量、Alive、InLimbo、未 OnMap、建造分類、native type array index、naval，以及完整 type list hash。
- 工廠 `+0x24 Production.Value` 必須正好 54，且 `+0x58 Object` 是完成建築。原生 `4CA130 IsDone` 的 SpecialItem-only 分支不提供建築，故不接納。普通／防禦工廠仍依 House `+0x53BC/+0x53CC` 與 Type `BuildCat +0xE08` 對應。
- Factory ID 偏移已以支援 SHA 原始指令核實：Factory constructor `4C98B0` 先進 Abstract constructor；`4C9934` 呼叫 `410230` 並傳 `this+4`，後者在 `41024F` 將 Scenario 新 ID 寫入該參數 `+0xC`，即 Factory `+0x10`。不是把任意 factory 欄位當身份。
- 已確認同世界的 Ready／Paused／DisplayHidden 可重新讀取完成清單，供切到工具箱選取；Unknown、換局、退出或不可驗證身份清空清單。這不授予背景下單能力。
- 真正提交要求前台、可操作玩家、command Ready、同 epoch／frame／House、3500 ms 內 offer、目前 command request 序號與 type hash，以及舊 placement patch 已完全撤回。
- 原生 `692300 ProcessClickCoords` 只解析本次凍結客戶區點。前後驗證 tactical pointer、camera、解析度、表面原點與尺寸；不取執行時滑鼠，不讀舊 CurrentFoundationCenter，不接受 sidebar 點。
- 地圖原點由既有 `map_bounds` 重驗 live playable rect、cell identity、level、slope；完整地基的佔用／地面合法性留給原生 Place 接收路徑，不改 Type 或全域規則。
- ctor 前、ctor 後進 enqueue、原生 clock callback 後都重驗產品與環境，最後再核對 packet 的 House／Type／index／naval／cell。其他 Domain 不得帶 Place submission，普通 Place 也不能繞過這份凍結請求。

原生 Place 封包沒有 Product UniqueID。身份保證的截止點是入隊；接收時仍由原生工廠邏輯解釋事件。成功回執僅表示「已提交放置事件」，提交後取消選擇，使用者可明確重新選擇再試；沒有假設固定延遲即執行完成，也沒有自動重播、清除工廠、退款或刪佇列。

## 驗證

`tools/test_native_placement.py` 編譯 `assist/tests/native_placement_bridge.cpp`，直接包含 production Place helper，並從當前 `bridge.cpp` 抽取真實 `submitConstructed/enqueue`，**48 checks PASS**。涵蓋 SP／MP 正常入隊、只讀清單與下單門檻分離、舊 patch 殘留、factory／product／type pointer reuse、讀失敗、過期／換局、sidebar、原生入口不符，以及 query／ctor／timer 邊界的鏡頭、frame、epoch、產品、patch ownership、request、stop 改變；所有拒絕都沒有入隊或清空工廠。非法 Place 形狀與其他 ProbeOnly Domain 仍拒絕。

測試是 x86 Windows 私有 buffer 模型；讀取與 queue 目的位址翻譯至測試資料，ctor／座標查詢／clock 由明示替身提供邊界變化。不包含遊戲 EXE、真地圖、網路或平台驗收。前篇 12 例原始指令測試另證明 native 接收分支，不能與本 fixture 合併宣稱真遊戲已建造成功。

production `bridge.cpp` x86 單檔編譯通過；整包與 ABI 驗證由主整合執行。可重跑：

```text
python source/tools/test_native_placement.py --zig <zig-0.13.0> --build-dir <isolated-output>
```

參考固定 YRpp：[FactoryClass](https://github.com/CnCNet/YRpp/blob/d9795546950fcc75a94325ffc83b44d16ec34ce2/FactoryClass.h)、[AbstractClass](https://github.com/CnCNet/YRpp/blob/d9795546950fcc75a94325ffc83b44d16ec34ce2/AbstractClass.h)、[DisplayClass](https://github.com/CnCNet/YRpp/blob/d9795546950fcc75a94325ffc83b44d16ec34ce2/DisplayClass.h)。本地核對 SHA 與前篇相同；未修改或操作遊戲進程，未發布私有 dump。
