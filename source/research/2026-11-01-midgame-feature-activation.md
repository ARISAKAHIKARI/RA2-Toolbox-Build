# 遊戲中途首次開啟功能的連接與診斷

使用者確認的觸發是「關著進遊戲後首次打開」。受控測試未重現 Runtime 的一般冷啟用失敗；Host 則存在一個有條件的連接需求漏項，已補齊。這兩項結論不能代替 RA2OL 實機驗證，也不代表所有功能都有相同失效原因。

## 連接需求漏項

原本 `main.cpp::tick` 依賴 `externalData.status == 1` 或部分功能來呼叫 `ensureBridge()`。控制全部、地圖繪製、部分原生適配、低傷害過濾和隊形等選項，以及智能卸載、原生飛機路徑等 flag，未完整列入需求。正常外部快照會掩蓋漏項；外部快照暫不可用、尚未建立操作連接時，首次單開這些功能可能沒有建立 Runtime 連接的機會。

[runtime_feature_demand.h](../assist/source/runtime_feature_demand.h) 集中判斷實際啟用的功能需求；單純顯示設定及未啟用功能的附屬偏好不要求連接。`ensureBridge()` 的原生 HWND、PID、執行緒、舊連接清理與 ABI 檢查，以及 Runtime 的世界／玩家／簽名判定均保留。這是確定的條件缺陷，尚未證明就是使用者 RA2OL 個案的全部原因。

## 編譯元件的冷啟用測試

[feature_lifecycle_runtime.py](../assist/tests/feature_lifecycle_runtime.py) 在 Unicorn 中執行實際編譯的 x86 Runtime；Win32 和場景物件是替代資料，遊戲 EXE 只在本機讀取。輸入 EXE SHA256 為 `268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0`。基線與新版各通過 9 例：

- 首次世界綁定時 flags 為零，再於同一對局開啟、關閉、重新開啟；另對照開局已開啟。
- 遊戲已至 frame 5000，Tactical 訂閱為零時先送消息，確認 Assist 尚未執行；首次訂閱與功能同時啟用。`externalDisplay` 為 0、1 各一例。
- 暫停後開啟再恢復、背景狀態開啟再返回、遊戲菜單中開啟再關閉菜單。
- 計算位置仍按既有規則在下一局切換；暫時簽名失配在恢復原字節後重試成功。

最終 Assist86 的首次冷啟用執行 9 例已通過，但輸出目錄不存在導致證據寫入失敗；保留失敗紀錄、建立目錄後，以同一腳本和同一 DLL 完整重跑通過。

較早首次執行曾因缺少 Python 依賴而未進入 Runtime；安裝私有依賴後，9 例又因舊 fixture 缺少 `0xA8B230` 的 ScenarioClass 身分全部停在世界判定。Map Cell 與 House 資料已有設定，Scenario 指標仍為零。只補測試資料後，基線及新版均通過；沒有放寬正式程式的世界檢查。此失敗記錄保留，不能當成產品故障或修復成效。

被測基線 Runtime SHA256 為 `28d5b9032bacbab246aa3fff7856e3d2be2923f622845febc82682c373ee8bb7`；新版為 `a30ea864008b291a49f7847c6fc7304fe2d69e37165763d919c191551ebeb5e3`。最終交付 Assist86 Runtime 已重新執行 9 例，測試前後 SHA256 相同。新版使用 `--detailed-gates`，另核對暫停與菜單的具體原因碼。命令、逐例結果、依賴固定版本與雜湊見[建置及測試證據](evidence/actor-render-performance-build.json)。

## 未啟用原因的顯示

[activation_diagnostic.h](../assist/source/activation_diagnostic.h) 沿用既有 `commandStatus`，不改共享記憶體布局。只有 1 授權操作；3 仍代表簽名不符。新增 5–11 區分世界資料暫不可讀、舊功能還原未完成、玩家列表不可讀、觀戰或無操作權、暫停、戰場／菜單不可見和操控方改變。

Host 介面顯示具體原因；`Runtime.log` 同時記錄要求值、已發布 flags、觀察到的功能狀態、補丁數與衝突數。狀態變化最多每 250 ms 合併一次，未變化的阻塞狀態最多每 5 秒一次。這些診斷不授權命令、不重送已確認的請求，也不因切屏撤回 Hook。SEH 與無法確認所有權的情況仍保留原有保護。

本次沒有新增遊戲地址或原生呼叫 ABI 的逆向結論。持久 Library 筆記本沒有可用工具；結果與初次失敗記錄先保留於倉庫證據，待可用時回填。
