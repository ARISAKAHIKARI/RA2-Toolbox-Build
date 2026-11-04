# 控制標記資料模式與普通事件功能分類校正

日期：2026-11-01（UTC+8）。基線40f32921035d417cdd0ad37c9cd2b32f4adae051，包含科技全開一次完整刷新。使用者明確要求增加不改遊戲程式碼的單機控制標記選項，並指出高級進駐、智能卸載、載員逃生原本可不改程式碼。

## 實作範圍

新增Experimental.ControlImplementation：缺省／無效值0為控制標記，明確值1才選原生判定Hook。啟用與否仍由既有ControlAll獨立開關控制。共享controlAll值定義0=關、1=原生Hook、2=控制標記；Assist版本升85隔離舊宿主語義，Shared仍6580496 bytes，欄位不增加。

標記模式僅對可用玩家House的+1ED IsInPlayerControl寫一個byte=1。保留+1EC IsHumanPlayer、CurrentPlayer A83D4C、物件Owner+21C，沒有新增事件或代理跨玩家命令。原版非戰役控制判定不依賴此旗標，所以它是按使用者要求提供的資料實驗，不保證在遭遇戰／RA2OL取得完整原生控制；畫面狀態2只表示資料已套用。原有原生判定模式仍可明確選擇。

啟用資料模式時，控制判定3站、命令所屬方3站、物件所屬方1站、網路保留7站必須全部還原且無封包快取待清理，才開始寫旗標。共14站互斥，不自動fallback。反向切回Hook模式也必須先還原所有旗標。命令所屬方與網路保留選項置灰，保存選擇不丟失，實際請求0；純標記模式關閉後可恢復原有獨立選項。選擇資料模式不會因獨立變賣選項暗中裝回網路保留Hook；相關跨玩家操作沿用ready拒絕。

其他單獨開啟的功能仍按各自設定工作，因此『此選項不改程式碼』不代表整套工具在任意已保存設定下全無補丁。

## 原值與生命週期

control_house_flags.h記錄每個House的pointer、ID(+10)、type(+34)、vtable、index(+30)、原byte與所有權。House陣列A80228，items位於A8022C，count位於A80238。原旗標已為1則不接管。寫入前先登記責任，寫後驗證失敗仍必須恢復；暫時讀失敗、不可寫頁、恢復失敗均保留記錄，不冒充完成。外部不明值不覆蓋；已確認身份退休不回寫舊指標。

只允許寫入正常可寫資料頁，不用VirtualProtect把遊戲程式碼頁改可寫。關閉、模式切換、停止／故障與detach納入旗標清理；allOwnedPatchesRestored也檢查資料責任。切屏或短暫未就緒不撤回旗標。失敗狀態保留／清理責任，需關閉再啟用重試，不聲稱具備完整遊戲控制效果。

獨立審查發現命令epoch也可能因CurrentPlayer切換或House陣列暫時失讀後恢復而增加，舊初稿把epoch不同直接當舊House已消失，會丟失原值。最終改為先看Scenario A8B230／Session A8B238／Frame A8ED84與逐House身份：同一存活世界跨epoch仍實際還原，再綁新epoch。真正Scenario/session變更或frame回退不回寫舊世界；存讀檔若保存了已改旗標，無法靠舊記錄可靠判定新載入值來源，這是資料寫入實驗的限制。

## 高級進駐、卸載、逃生等分類更正

此前將『呼叫既有原生合法性查詢，平台副作用未驗證』直接標為高危，容易被理解成『無法不改程式碼』。這項推斷過度，本輪更正UI與說明，不否定使用者實測正常。

高級進駐／裝車使用MouseOverObject(+74)合法性查詢及原生Enter等任務事件；智能卸載／逃生使用Deploy/後續任務事件，查詢+314／+74／+554；順向躲彈使用CanEnterCell(+1AC)及Move事件。這些普通功能本身不安裝code或vtable補丁；native_input_hooks.h已無輸入detour安裝，Windows訊息入口不等於改寫遊戲函式。具體路徑見cooperative_bridge.h、native_orders_bridge.h、unload_compute_bridge.h與reverse_dodge_bridge.h。

相應開關恢復普通名稱；精確原生戰鬥查詢改為『實驗』，仍預設關閉、保留純資料估算。呼叫現有函式與改寫函式不同，目前沒有證據把這些查詢判為已確認OOS根因。明確程式碼／虛表補丁及直接模擬資料寫入仍標高危。移除誤標也不冒充已完成所有平台聯機驗證。

## 驗證

- control_house_flags.cpp：模式准入、僅1ED寫入、原值、寫前／寫後失敗、部分恢復、讀失敗、外部值衝突、身份重用與同世界epoch遷移。
- control_house_flags_windows.cpp：實際x64 Win32、production資料bridge（固定32bit模擬位址；x86測試CRT佔用低地址，實際遊戲Runtime另完整編譯x86）、真實可寫／唯讀頁、模式互斥、切屏暫停、讀失敗重試與epoch恢復；無遊戲程序。
- host_control_modes.cpp：22項模式映射、保存偏好不變、灰色依賴、獨立變賣不能例外啟用網路Hook及UI分類。
- 沿用科技一次刷新30項回歸；普通功能路由回歸；Assist85跨位元686項欄位比對，對照84只版本改變，其他685項一致；完整產品／診斷器建置、PE及真實診斷UI/broker載入。

證據見 [本輪配套](evidence/control-house-flag-build.json)。支援EXE SHA-256沿用268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0。沒有RA2OL雙機、遊戲畫面或標記實際控制效果的實測。持久Library筆記無可用存取工具，依AGENTS記於repo供回填。
