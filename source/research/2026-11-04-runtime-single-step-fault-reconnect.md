# Runtime(8)：單步異常後命令鎖存與重連殘留

基線 `main@9eb8feef1306ddb5428f094b9dcf7e8fc9e92ae8`。使用者反映 Hook 會錯誤卸載、重啟工具箱仍無法安裝，重開一局才恢復。本輪只分析日誌和現行來源，沒有修改 Runtime 程式碼、執行遊戲或新增平台入口授權。

先檢索研究 INDEX、同一持久 SQLite v127 的 Hook/宿主/啟動記錄，並讀 Hotfix33 與 latent-desync 清理筆記。輸入 `Runtime(8).log` 為1,486,257 bytes，SHA-256 `39e15b2960c67d472047bc3f7d67aa8ed0cc8d846253f32b9cad4edda9381d12`。日誌為 pre.4/pre.5 累積；本輪故障發生於 pre.5。現場遊戲 EXE 完整 SHA-256 未提供，PE metadata hint 不當作身分確認；下列地址是日誌值及既有工具定義，不冒認本輪重新反組譯支援映像。

## 可確認的時間線

| 行號／本地時間 | 觀察 |
| --- | --- |
| 967，23:19:43.544 | pre.5 Host34724 啟動 |
| 972/980，23:19:57 | PID39736/TID3884 讀取連接與 WH_GETMESSAGE 安裝成功 |
| 995，23:20:01.135 | match1/session5/status1/commandStatus1，原生 hook 狀態正常 |
| 1182，23:22:25.315 | faultSerial0→1；code `0x80000004`；address `0x004F7870`；boundary1；thread3884；phase/command/source/target均0 |
| 1183–1184，23:22:25.318–.341 | blocked1、status−2、commandStatus0，controlFrame停在8764 |
| 2339，23:38:01.258 | callbackCount12231→79809、dispatches80138，subscription1、stageconnected仍在；命令畫面fresh0、frame8764 |
| 2338–2369，23:38:01–04 | detach時舊PID仍活著；同PID重連多次被「舊命令橋尚未完成卸載」拒絕 |
| 2400/2415，23:38:16.558／21.082 | 舊PID退出，新PID40268/TID43444安裝成功 |
| 2844，23:42:03.847 | 新遊戲進程仍健康：fresh1、frame13439、faultSerial0 |

第一個失效時，實體 Windows Hook 沒有消失；它仍在回調而命令 Runtime 被停用。此故障是 SINGLE_STEP，並非此前 `0xC00000FD` 栈溢出。記錄的恢復同時更換了遊戲PID，不能單憑此檔證明「同進程只換scenario就會清除」。故障後 Host 始終是34724，使用者所述重啟工具箱是另外的現場觀察，這份記錄沒有獨立呈現。

## 來源上確定的鎖死

`assist/source/guard.cpp::AssistExceptionFilter()` 對所有例外均回傳1。故未被 VEH 消費的單步例外到達 boundary1 時，`GuardedAssistTick()` 會執行 `AssistFault()`。`0x4F7870` 是本工具科技參數服務使用的 House::CanBuild 站點；單靠該地址不能證明所有 debug 原因都歸本工具所有。

`bridge.cpp` 的 static `fault` 在 `AssistFault()` 設為true後沒有清除路徑。`dispatchFromTrigger()` 只在 `!fault || (sh&&sh->stop)` 進tick，故正常故障期間連 `maintainTechParameters()` 的非同步退休完成輪詢也不再執行。Host停止／死亡的 watchdog 可以進入停止分支；即使後來 `disconnect()` 完成全部還原並把sh清空，fault仍保留在被pin的遊戲DLL內。下一個Host的mapping要由tick打開，卻因 `fault=true && sh=nullptr` 無法進tick；watchdog亦在sh為null時直接返回，形成無法重連／無法stop確認的殘留。

可修正的邊界：故障期間保留僅用於清理的排程；確認 `featureRestorationComplete()` 且成功disconnect之後，才清除舊連接的故障狀態。不可在資源還未退休時強制stop=2或無條件重啟正常工作。

## 單步漏接仍待確認

`techbp::Service::vectored()` 在明確自有命中時，即使參數callback拒絕修改，也會清自身Dr6位並設RF後繼續。它沒有gameentry busy／重入拒絕條件。會轉交的情況包括錯執行緒、沒有本方pending位、混合foreign/TF/BD原因、lease/Dr7/EIP不符、handler/instance不存在。不能以「callback返回false」解釋本次漏接。

worker在ResumeThread之前已發布lease，排除直接的先resume後發布寫法；但來源上仍存在另一個交錯候選：worker可在VEH派發中途掛起owner、更新lease，恢復後舊Exception CONTEXT與新lease可能不符。現有Windows fixture多在worker交易完成後才呼叫被監控函式，沒有覆蓋這個交錯；尚未受控重現，更未由現場日誌證明。

下一輪需記錄一次性拒絕快照：Dr0–3、Dr6、Dr7、EIP/EFlags、owner/currentTid、lease mask/sites/tag/uncertain、job/op/state，才能區分真正外部例外與本方租約交錯。不可單按CanBuild地址吞掉所有SINGLE_STEP；外部／混合debug原因應保留原處理鏈。恢復故障連接與處理首個單步來源是兩個獨立問題。

本輪證據為使用者實機日誌與來源審查，沒有宣稱已修改、編譯或驗收修復。
