# Runtime(9)：共享提示框清理死鎖與故障連接退休

基線 `main@7285b5e66003ec91d1414f060bc5915c51040e4a`；修正版標記 `1.0.41-pre.7`，Assist96／Cursor155／Runtime4 的 ABI 不變。先檢索研究 INDEX 與持久 SQLite v128，讀取 Runtime(8) 單步／重連鎖存及雙槽提示框筆記。本次有使用者實機日誌、來源審查和隔離 C++ 資源測試，沒有實機遊戲、Windows 例外派發或聯機驗收。

輸入 `Runtime(9).log`：530243 bytes，SHA-256 `2de28f14cf168f18359d5842026161d6dbe4362b6f36237a331de1b3107d555b`。現場為平台 `gamemd-spawn.exe`，完整 EXE SHA-256 未提供；PE metadata 不是映像身分證明。下述地址來自日誌和既有支援 ABI，本輪沒有重新反組譯遊戲映像。

## 日誌可以確認的兩類失效

- 第587行，00:10:13.849：PID45620/TID14784，boundary1，`faultSerial=1`、`0x80000004`、`0x004F7951`。下一筆 `techError=-45`、`status=-2`；這是科技精確參數的比較站點，不能單靠地址認領所有 debug 原因。此前 Runtime(8) 的單步地址為 `4F7870`，本次是同服務的另一站點。
- 第897行，Host40188/PID44996，00:12:16.752 仍為正常命令狀態、frame126、installed2。
- 第909行，00:12:28.570：Host26932 重新安裝同一 PID44996/TID33568 的 `WH_GETMESSAGE` 成功。第924至1017行 callbackCount141→8651，`runtimeError=0`、`faultSerial=0`，但 `commandStatus=6`、frame331 固定。第916至1020行持續為 `owned-feature-restoration-pending`。

因此最後這段是 Hook 回調存活、命令恢復卡住，不能稱為 SetWindowsHookEx 安裝失敗。日誌只有 installed 總數，**沒有列出當時兩個地址**；不能僅由數字2證明它們必然是文字／佈局槽。以下是來源上可獨立重現、與該現象一致的清理死鎖。

## 確定的共享槽漏項

`game_runtime/source/native_tooltip_runtime.h` 由同一 Runtime 同時服務 Cursor 與 Assist：文字 vtable 槽 `7F74D8`（原方法 `479050`）和 Cursor 專用佈局槽 `7F74C8`（原方法 `478BA0`）。兩槽都寫入 common game-entry registry；`ProcessHookRegistry::installed()` 是整個 DLL 的活動數，不是單一 Assist 的活動數。`NativeTooltipBuildTime(false)` 只釋放 Assist 的使用需求，Cursor 仍需顯示時兩槽可繼續存活。

舊 `featureRestorationComplete()` 從 installed 中排除幀通知 `55DE81`、已釋放 Assist 需求的文字槽，卻漏掉 Cursor 佈局槽。故 Cursor 持有兩槽且 Assist 自身資源已清完時，仍永遠返回 false：worldCleanupPending 無法解除，或 disconnect 無法完成。

修正把 Cursor 專用 `7F74C8` 納入獨立呈現資源計數。不撤掉 Cursor、不略過任意未知／遊戲功能站點；Assist 建造時間 claim 未退休、CRC／網絡／資料／硬體租約等還有責任時仍阻止完成。沒有新增 Hook、改寫遊戲程式碼或規則。

## 故障退休與下一個 Host

Runtime(8) 已記錄的 static fault 問題這次實作修正：

1. dispatcher 仍向 GuardedAssistTick 提供已獲得的訊息執行機會。tick 在停止／控制者退出分支之後、正常世界讀取之前設故障專用路徑：重試 reset、發布失效狀態、返回。`maintainTechParameters()` 可繼續輪詢異步退休；不能執行新的正常指令或恢復故障連接的功能。
2. AssistFault 一開始就設 fault 並停用所有硬體參數需求，再進行各功能還原，避免逐組還原期間仍把兄弟需求當成正常工作。
3. 只有 featureRestorationComplete 成功且 disconnect 真正解除舊 mapping 後才清 fault。未退休資源時保留舊 mapping／stop 未確認；正常命令不自動重播。DLL pin 不再令下一個 Host 永遠繼承舊 fault。

首個 SINGLE_STEP 的 lease／VEH 交錯來源**尚未重現和修復**；本次不吞掉外部／混合單步例外、不按 CanBuild 地址強制 CONTINUE_EXECUTION。這個修正處理確定的清理與重連缺陷，不能宣稱消除了所有首發單步或平台兼容問題。

## 驗證與未決項

`assist/tests/hook_recovery_policy.py` 抽取實際 production 清理判斷、disconnect、dispatcher 及 fault 分支，編譯執行。修正前在「Cursor 兩槽存在，Assist 自身已清完」第2項失敗；修正後27項通過，包括雙槽+Frame、仍持有建造時間、未知活動站點、各種未退休資源、stop 確認、fault 清除時機、故障期間仍接收清理機會、沒有 grant／重入時不消耗錯誤工作，以及 fault 分支不繼續正常路徑。

結果：[hook-recovery-policy.json](../verification/hook-recovery-policy.json)。日誌轉換證據：[runtime9-hook-recovery.json](evidence/runtime9-hook-recovery.json)。測試只使用合成資源和 Win32 替身；不包含真實硬體斷點、例外派發、遊戲畫面、同步或聯機結果。Windows 配對編譯由專用 workflow 執行，編譯結果須另行核對後交付。

下一步：實機確認已不再卡 gate6，並補充 SINGLE_STEP 拒絕時的完整 debug-register／lease 快照以識別首發漏接。不能把本次 C++ 判斷測試當成已重現該競態。
