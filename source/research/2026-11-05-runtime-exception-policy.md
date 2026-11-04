# pre.8：保留 Hook 的異常傳遞、SEH 清理與重連

基線 `main@98ff5db7872407b86aa0be7849755e9728116fa2`，承接 [Runtime(8) 分析](2026-11-04-runtime-single-step-fault-reconnect.md)。使用者明確要求 Hook 不要隨便失效，即使真正異常造成崩潰也優於靜默停用。本輪實作此策略，並修正先前確認的清理／重連鎖存；沒有宣稱已找出現場 SINGLE_STEP 被哪個 lease/Dr6 條件拒絕。

先查研究 INDEX 及同一持久 SQLite 的故障、SEH、Hook 生命週期記錄。現場遊戲 EXE 完整 SHA 未提供；本輪沒有讀取／執行私人遊戲或捕獲、沒有新增引擎地址或入口 ABI。證據為產品來源、來源抽取的受控回歸及產品 Windows 目標編譯；實際 Windows SEH 另由本輪 CI 的自有程序 fixture 執行，不當作遊戲／多人驗收。

## 異常策略

`exception_policy.h` 由真正 MSVC x86 `AssistExceptionFilter()` 使用。SINGLE_STEP、BREAKPOINT、debug/control、unwind/invalid-stack/nested/未知控制 flags 和未知語言／軟體例外交回原處理鏈。僅 NONCONTINUABLE 屬性本身不禁止合法的 handler unwind。

全局 boundary1（tick）、10（輸入）、11（風險動作）對明確原生 fault 記錄診斷並 CONTINUE_SEARCH，不執行全局 AssistFault。沒有用 CanBuild 地址假定 debug 歸屬，也沒有吞掉外部／混合斷點；未被其他合法 handler 處理的異常保留系統原本的崩潰流程。

具明確 fallback 的局部 boundary9/12/13/14/17/18/19/21/22 只處理允許的 AV／in-page 錯誤，不升級成整個命令橋停用。科技參數與純視口輸出的 no-report guard，以及 Cursor 資源 guard 使用同一 access-only 分類，避免在處理硬體例外時遞迴記錄。

## 為何需要顯式 SEH 清理

Runtime、bridge 和 Cursor 以 GNU x86 `-fno-exceptions` 編譯。只把全局 filter 改為 Search，外部平台若決定 unwind，原本 C++ RAII 解構不保證執行，仍可能遺留 schedulerBusy、dispatching、執行租約或 FP 狀態。

新增 `RuntimeScopedCall()`：僅以 MSVC `__try/__finally` 執行同步 callback 及 cleanup，沒有新增 catch。`runtimeunwind::run()` 的 callback 資料存於受保護呼叫之外，normal return 或真正 unwind 均清理；第一時間繼續執行的例外不提早清理。

`gameentry::scoped()` 只釋放自己成功取得的完整 Request；Scope.release 與 FP.restore、WakeTrace.release 均可重複呼叫。Hook／watchdog／post 路徑只釋放本次成功 CAS 的 dispatching，dispatcher 同理處理 schedulerBusy。Cursor、原生 Tooltip、frame observer、輸入和 watchdog 的 entry／FP 範圍全部覆蓋；沒有清空外部 activeToken 或其他客戶的深度。

同時處理產品原有的短期狀態：原生 Mega/Object 呼叫深度、sender packing 的前值、MO 提交種類／actor，以及 computeFrameSeq。中斷的計算樣本只失效並關閉自己持有的序列，不當作完成幀發布，不改事件 request/ack 或強制重試已提交命令。正常提早 return 仍保留原先發布順序。此範圍不是任意遊戲原生副作用的 rollback。

## 舊故障清理及重新連接

故障鎖存期間繼續進入 cleanup-only 路徑，輪詢非同步資源退休，不處理普通命令；live Host 的故障連接保留至明確停止或控制者退出後才解除。`disconnect()` 只有在 `featureRestorationComplete()` 為真且完成原有傳輸釋放後才清除舊 fault。`AssistRuntimeDetached()` 亦要求 fault 已清除，避免過早授權新 controller 接手。

## 聚焦驗證

- `exception_policy.py`：portable 706 項產品決策檢查；Windows 連結真正完整 guard 與 Cursor resource guard，驗證48項例外路由、48次正常再進入，以及3種 normal／外層 AV unwind／可恢復 debug 的 finally。
- `fault_reconnect.py`：抽取真正舊／新 bridge 路由與新 helper。舊基線重現4種鎖死，新版10項案例通過；傳輸／資源完成為替身，portable C++ throw 僅用來驗證呼叫者 cleanup 路由，不冒充 Windows SEH。
- `runtime_scope_windows.py`：真正 MSVC trampoline、真實產品 execution manager 與 Scope／FP helper，自有程序測 normal／外層真 AV／可恢復 debug、巢狀精確 Capture、拒絕 entry/CAS、FP及再次進入。
- 繼承的 `hook_recovery_policy.py` 在新 dispatcher／故障路徑下重新執行27項通過，保留共享 Cursor 提示框兩槽及未知站點的所有權門檻。
- 產品 guard、bridge、Runtime、Cursor 實際 Windows 目標物件編譯通過。完整配套編譯及 Windows fixture 的最終結果綁定至交付證據，不把本機僅連結的 exe 稱為已執行。

本版為 `1.0.41-pre.8`；保留 pre.6 小地圖及 pre.7 共享提示框雙槽清理修正。Cursor 專用布局槽 `7F74C8` 與已釋放 Assist claim 的共享文字槽，依 pre.7 的同一所有權規則排除，不阻止 Assist 退休；未知活動站點及仍有責任的資源仍阻止 handoff。現場硬體例外漏接的詳細原因、實際遊戲畫面和雙機聯機仍未驗收。
