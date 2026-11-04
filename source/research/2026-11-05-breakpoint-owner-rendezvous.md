# 硬體斷點交易的 owner 交會與偵錯例外分流

## 問題與證據範圍

基線 `codex/hook-recovery-20261105@98ff5db7872407b86aa0be7849755e9728116fa2`；本輪版本 `1.0.41-pre.9`，不改 Assist96／Cursor155／Runtime4。
使用者要求持續處理 Hook 動輒失效。先讀研究 INDEX、Runtime(8)／共享提示框清理筆記與持久 SQLite v129；沿用 Runtime(9) 的 `0x80000004 @0x004F7951`、Runtime(8) 的 `0x004F7870` 現場紀錄。
未取得現場完整 EXE SHA-256 或當次例外 DR／lease 快照；不新增引擎入口、不重新反組譯私人遊戲映像。既有科技站點及 ABI 不變。

## 確定的程式行為

舊 `techbp::Service::start()` 啟動背景 worker 後立即返回。worker 可於稍後才 SuspendThread owner、讀寫 DR、驗證並發布 lease。呼叫端返回後已可重新進入原生遊戲／硬體斷點例外；原 lease 與舊 Exception CONTEXT 可能仍在派發，而 worker 又改了當前 lease。原本 worker 在 ResumeThread 前發布 lease，不能單獨排除「派發已開始後才被掛起」的交錯。

修正讓 live owner 只能在自己的 service 呼叫內啟動交易，並留在 Runtime 邊界等待 worker 完成；worker 仍以另一執行緒 Suspend/Get/Set/readback/Resume，owner 不嘗試修改自身正在執行的 CONTEXT。等待失敗也必須等到 worker 發布完成，避免逾時／失敗又放走 owner。已退出的 owner 允許其他執行緒清理；錯誤執行緒提出 live-owner 交易直接拒絕，既有健康 lease 不被標為故障。

此交會只作用於安裝／重配／稽核／退休／資源探測，不作用於每次硬體命中；代價是交易期間 owner 短暫等待，實際遊戲停頓成本仍未測量。受支援呼叫路徑是 Runtime 正常維護／清理，不可從任意外部 VEH 回呼啟動交易。

另有獨立且確定的問題：`AssistExceptionFilter()` 對所有例外回傳 EXECUTE_HANDLER，故外部 SINGLE_STEP／BREAKPOINT 在外層 SEH 才被處理時也會觸發 AssistFault。現在這兩種例外回傳 CONTINUE_SEARCH、不記為 Assist 故障、不改 CONTEXT；由原 debugger／VEH／外層 SEH 負責。這不是按 CanBuild 地址吞掉未知例外，也不保證沒有接收者的外部例外可繼續執行。一併整合 `4331dbd04f50994ab786883889a42ce3f2669b73` 的完整例外策略：真正的全域故障記錄後仍往外傳遞，只有明確定義回傳值的本地 AV／in-page 邊界可恢復。入口鎖、FP、序列、嵌套計數以真實 SEH finally 釋放；見 [例外策略筆記](2026-11-05-runtime-exception-policy.md)。這避免外部處理者展開堆疊後，只依賴 GNU C++ 析構而遺留 Runtime 鎖。

Microsoft 參考：
- [SetThreadContext](https://learn.microsoft.com/windows/win32/api/processthreadsapi/nf-processthreadsapi-setthreadcontext)：目標需先掛起。
- [Vectored Exception Handling](https://learn.microsoft.com/windows/win32/debug/vectored-exception-handling)：VEH 於 debugger first-chance 之後、堆疊展開前派發。
這些文件不證明現場首發原因；交錯風險為本輪來源分析。

## 驗證與未決事項

`assist/tests/breakpoint_transaction_gate.py` 抽取 production start／poll／AssistExceptionFilter，用延遲 worker 和合成 Win32 handle 執行。舊 start 在第2項（交易尚未完成已返回）失敗；新版本20項通過，涵蓋重配、audit、wait失敗、CreateThread失敗、非owner拒絕、owner退出清理、外部debug分流及原有真故障報告。這不模擬 Windows 核心 CONTEXT／例外派發。

整合後 production 清理／排程27項、延遲交易／filter20項、例外決策706項、故障重連及展開10個場景通過；原 hot helper 2101項亦通過。Windows 真實 x86 DR／VEH 測試已列入專用 workflow：延遲 worker 的32次重配與32次audit、既有兩槽與四站點交易、混合／外部例外、未知槽保留及精確退休。Windows 執行結果、成品編譯與配對身分在成功後另行補記。

來源交錯風險與舊 start 提早返回已受控確認；**尚未以真實 Windows 重現 Runtime(9) 當次 lease／VEH 交錯，不能認定已證實或消除首個 `0x004F7951` 單步的所有來源**。尚無使用者遊戲、畫面、平台或聯機驗收。化學兵腳下攻擊與蜘蛛自動功能維持暫停。


整合保留新版例外策略的明確故障邊界：已有 IPC 的 legacy fault 維持清理／報告，等待 Host stop 或死亡再完成退休；無 IPC 的殘留 fault 可清完後再接新 Host。全域 debug／native 例外正常走外部處理鏈，不進 legacy fault。所有產品標記統一為 pre.9，避免與其他分支 pre.8 混淆。
