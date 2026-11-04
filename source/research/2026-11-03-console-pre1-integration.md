# 1.0.41-pre.1 三方整合與 Web 控制臺收納

本輪以 `5983b943e90694f7b2630608bc464d6f8ee148e3` 為 merge base，合入 `codex/console-architecture-20261103` 的 `4b6bb9cc59658876eb1e3a10835c85298036371b`（tree `5cb0bef165473cd6e42d1cc8e0731929fe96d8c3`）。`1.0.41-pre.1` 是原始碼內的版本標籤；查詢時沒有同名 GitHub Release/tag。它比 base 前進六個提交，沒有落後，35 個改檔是 UI／建置／版本變動，未含原生 gameplay Runtime 改寫。

## 整合來源

完整 35 個 upstream delta、15 個 base 原檔均由 GitHub connector 取得並按 Git blob SHA-1 驗證；對已 materialize 的本地工作建立完整 snapshot，以 `git merge-file --diff3` 執行真正三方合併。`main.cpp`、verifier 與 dependencies clean merge。唯一建置腳本衝突保留 upstream WebView2 SDK/loader/licenses/console 流程及本分支 transport 欄位，再由發布者更新至最終 Assist92。沒有把未下載的遠端歷史素材視為刪除。

Formal scratch 在 `work/ra2-hook-audit/diagnostics/baseline-1.0.41-readonly/{base-delta,upstream-delta,current-snapshot,merged}`，完整合併表見 `formal-merge-result.json`。經 root 授權將 35 檔及三方結果套回 checkout；checkpoint 與備份在 `diagnostics/pre1-ui-integration/apply-35-result.json`、`baseline/`、`after-35/`。Scratch 代表套用前的合併檢查點，不冒充後續 Assist92 最終建置。

## UI 行為

Web model 新增 additive flags `groupHeader/collapsible/collapsedDefault/runtimeError/immediate`，透過原生控制項 property 匯出。15 組包括既有十四組及新下令／巡邏參數 9615；原生 fold handler 按 `PageControl.fold` 動態收錄，沒有固定組數。原生收納偏好未被 Web 初始化覆寫；總開關／同列主入口放在 DOM detail body 之外，子參數與正常狀態預設收起。說明保留在 title/hover。真正 Runtime 錯誤及 action ACK 錯誤強制展開並保留明確文字，解除後恢復使用者原偏好。

9470/9471 與科技加强總開關 9097 同在「遊戲規則」，不因歷史 page4 拆到實驗分類；9471 分別顯示入口不兼容、硬體槽不足、Windows API、foreign 斷點、執行緒、模式不兼容及生產保護七類錯誤。Sidebar 顯示完整／只留雷達／全隱藏，縮放以 `viewzoom::caption` 顯示 MAX、66.7%，未寫死 level 數量。相容 overlay HWND 無 compositor cutout 時，雷達模式已套用或切換中保持隱藏。

自毀按鈕 9185 的新 Web 路由攜帶原始 UTC click 時間；Host 入隊另記只由 native 產生的 monotonic admission 時間。執行前兩種 age 均不得超過 150ms，缺失／未來／過期即拒絕並回明確 ACK，未呼叫原生按鈕、沒有延後執行／自動重試。Renderer 不可傳入 native receivedAt；pending 去重與雙擊／按鍵重複保護仍保留。一般設定編輯的 FIFO、owner thread、關閉保存 barrier 與 asynchronous INI 行為保留。

## 驗證

`source/suite/tests/console_ui_regression.py --zig <zig.exe> --out <dir>` 產生 `ra2.console-ui-regression.v1` JSON：source SHA-256、兩架構逐 fixture stdout/EXE hash、`changed_during_run` 及 assertions_enabled。實際抽取 main metadata／9471／radar fallback，並執行 production console_model 與 adapter；所有 C++ 編譯 `-UNDEBUG`，新 fixture 顯式 `#error` 拒絕 NDEBUG。

| 驗證 | x64 | x86 |
| --- | --- | --- |
| production metadata、15 組、9471 -1..-7、雷達 fallback | 154 checks | 154 checks |
| 真 Win32 owner thread/control/過期即時操作/關閉 barrier | 152 checks | 152 checks |
| C++ model/parser/mailbox/serialization | 28056 checks | 28722 checks |
| 真檔案 asynchronous INI／retry／final state | PASS | PASS |
| 原生分類／15 組 fold／錯誤與恢復 | 4478 checks | 4478 checks |

C++ model 並行 slot 的讀取次數隨排程改變；並非固定總數斷言。Win32 fixture 必須附 Common Controls 6 activation manifest；缺少時會因缺少 entry point 啟動失敗，已在可重跑 driver 內為兩架構各生成資源，而不是跳過 fixture。

`node source/suite/tests/console_store_test.mjs --browser` 經本機 Playwright + headless Edge 執行，29 store tests、實際 DOM 的 15 組收納／錯誤恢復／hover／3000 行虛擬化／自毀雙擊均 PASS，沒有 uncaught 或 console errors。Windows 可設 `CONSOLE_PYTHON`、`CODEX_PRIMARY_RUNTIME_NODE_MODULES`、`CONSOLE_BROWSER_CHANNEL=msedge`、`CONSOLE_SCREENSHOT` 選擇本機工具與證據輸出。fixture state 是受控輸入，不宣稱遊戲中畫面已驗收。

結果存 `diagnostics/pre1-ui-integration/{console-ui-regression,settings-folds}/result.json`、`browser.log`、`console-desktop{-final.png,-state.json,-dom.html}`；來源綁定證據見 [console-pre1-integration.json](evidence/console-pre1-integration.json)。完整 WebView2 Host／Runtime／broker 最終配對建置與 manifest 由 root 補。沒有啟動或操作遊戲，沒有實機 FPS 或聯機同步結論。
