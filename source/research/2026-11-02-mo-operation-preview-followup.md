# MO 操作預覽連續性與快速軌跡

問題：新合入的模糊攻擊／範圍分配／巡邏框移動不順，按住拖動時可能整個消失。此處不是右鍵拖屏。

行為對照：MagicShiba/MO_ExperienceEnhancements 固定提交 `33ed558bd38f36b5012eddcc9c0fb109cc68e38a`。`src/order/FuzzyAttack.cpp` 的 `FA_BoxSample` 每輪更新範圍框參考點，`FA_Preview` 另外以 80ms／400ms 節流分配線；`src/order/SmartArea.cpp` 同樣區分軌跡採樣、範圍框與分配；`src/preview/DeployPreview.cpp` 在每輪持有時採樣軌跡。保留其行為參考註記，沒有導入其工程、下令 Hook 或二進位。

本分支確認的問題與修正：

- 原來把範圍框和巡邏線綁在 80ms 分配計算，框中心還取兩格稀疏採樣的末節點。現在保存最新游標格，範圍框／巡邏輪廓以 16ms 有界更新，昂貴目標分配維持 80ms／400ms。巡邏預覽包含尚未達兩格採樣門檻的末端，鬆開仍以最新座標重新驗證。
- 原來 `moPoint` 對同一滑鼠訊息查兩次原生游標位置。`formationPoint` 現在一次查詢同時傳出格子和懸浮物件，並如原來還原暫存的 pointer 狀態。
- 快速占位原來在 16ms 節流前直接返回，快彎折的軌跡點因此丟失。現在先記每條有效移動訊息的軌跡，再限制昂貴的佈局計算；保留兩格空間採樣和原生目的格限制。
- Host 遇到一次 seqlock 寫入碰撞就回傳 inactive，使完整預覽突然消失。現在只沿用同一 PID／對局／Shared 映射下的上一份完整快照；仍保留 500ms 新鮮度，穩定取消、關閉功能、切換進程／對局、聊天及失焦即清除。
- Host 渲染阻塞超過 150ms 時，Runtime 原來直接取消持有預覽。現在只對已捕獲的唯讀預覽容許不超過 1000ms 的心跳間隔；新捕獲、鬆開提交、每條事件送出仍保持原 150ms 與全部原生上下文驗證。超過寬限或進程／對局／前台失效仍取消，未放寬事件下令門檻。

沒有增加遊戲程式碼／虛表寫入。新修改僅涉及既有 Runtime 訊息邊界、Host 預覽快照及純幾何；原生位置查詢沿用已有 `formationPoint`／`pointerLocation` 路徑。支援映像的既有原生事件證據為 SHA-256 `247f72881e1a68c8fc305e3702dc0100a72d17601305999026393c340d5daeb0`，本輪沒有新增原生函式位址／ABI／欄位偏移，也沒有操作遊戲進程。

驗證：`source/assist/tests/mo_operation_followup.py` 編譯實際 `mo_orders_bridge.h`、`formation_bridge.h` 與 Host `drawing`／`sameDrawing` 原文，以有界世界和 Win32 邊界執行。檢查單次游標查詢、一格移動、快速轉角、200 次／20 秒持有、300ms 心跳抖動、嚴格鬆開門檻、seqlock 碰撞、穩定取消、PID／對局／前台失效及 500ms 過期。沿用既有原生事件入口、巡邏圖驗證及取消回歸。結果在 `diagnostics/mo-render-followup/operations/result.json`；相同輸入執行修改前 production headers 的反例在 `operations-before/result.json`。

這些隔離測試不證明實際 Windows 合成顯示順暢度、原生路徑尋路或 RA2OL 同步。保留鍵盤組合保持到鬆開滑鼠的既有語意，上游相同；沒有以放寬組合鍵來掩蓋預覽消失。

測試工具修正：本機 Zig `-O2` 預設會定義 `NDEBUG=1`。先前部分 C++ 測試雖印出檢查次數，標準 `assert` 並未執行，不能把該舊結果當作斷言驗收。本輪在 MO graph、native planning、quick geometry、quick bridge、MO bridge 及 AirPathMove fixture 明確取消 `NDEBUG`，新 followup fixture 另外使用可輸出失敗條件的 fail-fast 斷言。重新實際執行的 graph 28項、native planning、quick geometry、followup 255項及 AirPathMove 87項均通過；舊 production headers 在同一 followup 輸入下三份均失敗，確認測試能抓出缺陷。
