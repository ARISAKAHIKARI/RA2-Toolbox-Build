# 控制標記選項替換為修改大師模式

依使用者要求，原「控制標記」選項替換為「高危 · Master 基本指令修改」。原生控制 Hook 選項仍可選；本輪沒有修改實際執行中的遊戲。原生地址、支援 EXE 指紋、指令邊界與 ABI 見[獨立原生核對](2026-11-01-master-control-native-audit.md)。Library 筆記本目前不可存取，先保留倉庫紀錄。

## 配置與行為

設定檔 `ControlImplementation=0` 現在選擇 Master，`=1` 保留原生控制 Hook。關閉控制全部時不安裝這兩種控制組。共享模式 0/1/2 分別為關閉／原生／Master；原來 2 代表 House 資料位元，故 Assist 協議由 85 升為 86，防止舊 Host 與新 Runtime 把同一數值當成不同實作。結構大小與欄位偏移不變，更新時三個 EXE／DLL 必須成套替換。

Master 使用三處完整五 byte 指令：`50B6F0` 跳向遊戲內 `50B710` 的真分支；`6339BA`、`637E40` 的 CALL 指向遊戲內 `50B716` 的嚴格 House 比較。這是修改大師語義的適配，不是聲稱逐 byte 複製其歷史捕獲；已知 forward 入口下 `50B6F7` 不在執行路徑，不能照抄短跳。

新模式不寫 House+1ED、CurrentPlayer 或單位 Owner。舊 House 位元的恢復責任仍保留，但不再產生新寫入。直接改指令仍會影響共用控制判定，因此保留高危標示，不能把「沒有回呼到工具箱」理解為不修改程式碼或已證明聯機安全。

Master 啟用時，有效的「命令所屬方」及「網路保留」請求強制為零；介面會顯示此模式不請求兩組。使用者儲存的偏好不刪除，切回原生模式後按原偏好處理。Runtime 也會拒絕在 Master 中夾帶這兩組請求；不允許外方代理命令。不因選擇模式自動啟用額外站點。

## 所有權、衝突與還原

正式實作位於 `shared/source/master_control_policy.h`、`assist/source/control_sites_bridge.h` 及 `shared/source/game_entry_manager.cpp`。入口只接受精確原生前綴和兩個已知 forward 前綴，並核對上下文與原生葉函式完整尾段。任意 JMP、未知改寫、與本方 after 相同但沒有所有權的 bytes 都不直接接管。

安裝先完成兩個嚴格比較 CALL，最後才放寬共用控制。還原反過來，先恢復共用控制；若其仍被本方占用或發生衝突，保留兩個嚴格比較與待恢復狀態，避免控制仍放寬卻把外方編號折成本方。API 報錯但實際已寫入的情況也保留所有權。模式切換必須完成舊組恢復，不重標尚未恢復的站點。

EntryManager 對遊戲內目標的例外只允許上述精確 site／target／bytes；共用入口還要求兩個嚴格 CALL 已由同一模組持有且 bytes 正確。它沒有開放任意遊戲程式碼目的地。

切屏、暫停、遊戲菜單本身保留當前組；明確關閉或切換模式仍處理恢復，安裝等待原有可操作狀態。沒有加入首次幀同步要求。

## 驗證邊界

`control_master_bridge.cpp` 直接包含正式 bridge，覆蓋原生／Master 互斥、精確原值、部分寫入、衝突和共用入口恢復失敗順序。`control_house_flags.cpp` 與 Windows 版只驗證退役模式的舊責任恢復，不再測試啟用 House 位元。

`control_master_runtime.py` 在受控 Win32／遊戲物件上執行實際 x86 DLL；另以 `feature_lifecycle_runtime.py` 重測冷啟用。最終執行命令、逐例結果與 DLL 雜湊統一保存在[建置證據](evidence/actor-render-performance-build.json)。原生 777 項指令／ABI 對照與產品測試分別記錄，不能互相代替。

尚未進行新版 RA2OL 雙機、完整外方命令或實際遊戲操作驗收；不能據此認定不同步已解決。
