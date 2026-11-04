# 原生 Place 成功後的本地放置 UI 收尾

問題：Place 入隊回執與真正落地是不同時刻。事件成功後，Map 的原生建築／複製放置狀態可能仍指向該產品；工具箱預覽已關閉，不能依靠其 active 狀態繼續觀察，也不能以 Factory 已消耗便猜測落地成功。

新增 `assist/source/native_placement_completion_bridge.h`，只在既有遊戲執行緒 lease 內工作，不新增 Hook、不改程式碼段、規則、工廠或物件狀態，不發點擊、命令或事件。成功的 explicit Request 及互動 down/up 提交各登記一筆本地 receipt；隊列滿／驗證失敗不登記。主 tick 的 frameDue 包含 pending receipt，預覽 reset 後仍逐幀觀察。明確完整 reset／fault 清本地 receipt；一般 modal reset／切屏不清它。

receipt 凍結 Offer 的 match、House pointer/UniqueID、Product pointer/UniqueID、Type pointer/UniqueID，另凍結 `A8B230 Scenario`、`A8B238 Session`、`House+30 index`。最多保留 8 筆，10 秒截止；容量滿僅丟棄最舊的本地觀察。每次確認原生 frame 不得小於前次成功讀到的 frame，epoch／scenario／House／index／session 改變即丟棄。不可讀期間保留至截止但不寫入。

成功證據必須是原 Product：Building vtable `7E3EBC`、UniqueID `+10`、Type `+520` 及其 UniqueID `+10`、Owner `+21C` 都符合；HP `+6C > 0`、Alive `+90`、OnMap `+74`，且 InLimbo `+81 == 0`。只消耗 Factory、產品仍 limbo、不在地圖、被銷毀或無法驗證，都不收尾。五種原生工具旗標 `Map+11B0..11B4` 或 `8809A0` 超武選擇已啟用，則退休該 receipt，保留後來的工具。

兩組 cursor 分別只處理自己完全匹配的三欄：

| 狀態 | Product／Type／House index | Foundation setter |
| --- | --- | --- |
| 目前原生建築 | `87F7E8+11A4/+11A8/+11AC` | `4A8BF0(nullptr)` |
| 複製放置 | `87F7E8+1190/+1194/+1198` | `4A8D50(nullptr)` |

各組先 checked read 及核對 setter 前 12 bytes FNV-1a：`4A8BF0=C44D9D5E`、`4A8D50=DC84E30E`。世界／產品／工具／三欄再次核對後才呼叫。原生 setter 僅退休其 foundation，仍須把匹配的 Product/Type 清為 0、House index 清為 -1；若原生 callback 已換選擇，則不覆寫新欄位。未知簽名、短讀或外來產品都保留其狀態。active 與 copy 獨立，一組屬於外來產品不影響另一組的合法收尾。

支持映像 SHA-256：`268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0`。固定映像的反組譯與受控原始指令執行確認兩個函式都是 thiscall，ECX=Map、單一 nullptr 參數、`ret 4`，保留非揮發暫存器。nullptr 路徑將各自 foundation pointer（`+117C`／`+118C`）清 0，center（`+1178`／`+1186`）改為 `8A03F8` 的 empty cell；舊 foundation 存在時呼叫各自 redraw marker `4A95A0`／`4A9650`。函式本身不改上述 Product/Type/House 三欄。沒有把本地 foundation 清理誤認為 Place 成功、退款或事件接收。

驗證：

- `assist/tests/native_placement_completion.py` 直接編譯真 production header，x86/x64 各 **92 checks PASS**，`-UNDEBUG`、`#error NDEBUG` 與 fail-fast assert 確保執行。覆蓋 queue 不等完成、active/copy/both、外來三欄、產品與 House 指標重用、每個身份與必要短讀、partial read、工具切換、setter hash、callback 替換、10 秒截止、DWORD wrap、frame rewind、去重與容量界限；替身只有讀取、本地 setter 和指定六欄寫入，沒有事件入口。
- 同一 driver 以私有 PE 的兩個原始 setter 各跑「無／有舊 foundation」共 **4 個 x86 nullptr 路徑**，核對 thiscall/ret4/nonvolatile ABI、實際 foundation 寫入範圍，以及 Product/Type/House 三欄未改。redraw marker 是明示 checked seam，沒有執行真畫面 redraw。打包報告：[完整結果、重跑命令與來源 SHA](evidence/native-placement-completion.json)。原始報告在本機工作區（checkout 外）的 `diagnostics/placement-view-hotfix/placement-completion/result.json`；映像前後 SHA 一致，不發布映像或 raw dump。
- `tools/test_native_placement.py` 包含完整 production Place/preview/completion header 與實際抽取的 submit/enqueue，**103 checks PASS**。新增兩條真入隊 route→pending→模擬產品落地→原生 UI 收尾，且 modal reset、失焦後仍有效；Factory 消耗但產品未落地不清、失敗入隊不觀察、不多送任何事件。上述打包 JSON 的 `integration` 同時保留實際 test log 與來源 SHA；本機原件為工作區 `diagnostics/placement-view-hotfix/placement-completion-integrated/test.log` 及 `sources.json`。

上述是私有合成記憶體／原始指令受控執行。它不證明 Windows 現場視覺收尾、完整地圖落地或 RA2OL 雙機同步；真遊戲是否另有平台覆寫，仍需現場回報。持久筆記庫本輪不可用，本筆記保留供後續回寫。
