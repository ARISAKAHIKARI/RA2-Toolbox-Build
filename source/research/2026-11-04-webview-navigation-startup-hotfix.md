# WebView2 控制臺啟動導航自我取消修復

日期：2026-11-04。前版來源／包基線：`afc071d00070d392cd1e4be9749ad833029b08d3`。本輪只修 Host 的 WebView2 啟動與錯誤診斷，不改遊戲 Runtime、broker、Shared92 協定或遊戲功能。

## 已重現的原因

本機已安裝 WebView2 Runtime `156.0.4314.58`，失敗發生於 Runtime／controller 建立成功之後。真 Win32 隱藏獨立 HWND、隔離 `LOCALAPPDATA` 與真 production Host 的受控測試重現：`NavigateToString` 的 `NavigationStarting`（ID 2）URI 是該份內嵌 HTML 的 `data:text/html;charset=utf-8;base64,...`。舊判斷只允許 `about:blank`，所以自行 Cancel；同 ID 的 `NavigationCompleted` 回報 `IsSuccess=false`、`WebErrorStatus=14`（OPERATION_CANCELED）、最終 Source `about:blank`，然後鎖成「控制台页面加载失败」。沒有收到前端 ready。

因此這次實證是導航判斷錯誤，不能用重新安裝 Runtime 解釋。before 測試加入診斷與 friend 測試入口，但保留舊導航授權及失敗處理邏輯；其 instrumented CPP hash 與原版 backup hash 分別記錄，沒有把 instrumented 檔冒充未改來源。

## 修正及邊界

Host 以同一份 bundled UTF-8 HTML 事先計算完整 Base64 request URI，只在本次待載入階段允許該字串完全相符的 top-level 導航，記住其非零 NavigationId。FrameNavigationStarting 一律取消且不能取得此 ID；前端消息來源仍嚴格要求 `about:blank`。完成事件必須是本次已授權 ID 且成功、最終 Source 精確為 `about:blank`，才置 ready 並釋放早到消息。

初始化空白頁、其他被拒導航及晚到／重播的完成通知均不會把已載入頁面誤報失敗；本次真正載入失敗仍明確報錯並清除待發消息。ready 後沒有重新授權導航。錯誤會提供 NavigationId、IsSuccess HRESULT、WebErrorStatus 與 Source HRESULT／種類；正常 trace 只記來源種類，不寫入完整 data HTML。CSP、消息來源、下載／新視窗／權限限制保留。

本修復已在 Runtime156 的真實 data-URI 起始路徑驗證；未實證舊 Runtime 是否把 Starting URI 報成 `about:blank`，故沒有擴大為允許任意 data URI 或讓初始化空白頁搶佔本次導航 ID。

## 驗證

* 真 WebView2：production Host + 真 frontend bundle + pinned SDK `1.0.4314.35`，x64 own-process 隱藏 HWND，private cache。after 收到前端 ready，向 production Host.post 發送受控 shell／snapshot，在實際 DOM 讀到兩個控件、切換開關並收到原生 action；實際 CapturePreview PNG 已目視確認。後續 `about:blank`、相同 HTML NavigateToString、外部 HTTPS 三個導航均取消（14），原頁仍 ready、沒有 fatal。
* Fake COM：實際 production CPP，58 個 harness assert，`-UNDEBUG` 且 `#error NDEBUG` 防止被最佳化移除；覆蓋初始空白／重疊 ID、Frame 搶 ID、不同 payload、早到消息、取消的無關完成、owned 真失敗、重播、外來源消息、關閉／異步回呼與 owner-thread。12 個 COM IID 與官方 SDK／IUnknown 定義吻合，production Host 另用真 SDK 編譯連結通過。
* 本輪不啟動／附加遊戲，不操作使用者現有工具箱、瀏覽器或既有 WebView cache。受控 snapshot 不是遊戲資料；不宣稱 RA2OL 或聯機實測。完整正式 Host EXE 的來源配對、PE 檢查與繼承 Runtime 位元組由最終 hotfix build 證據記錄。

重跑真 native smoke（Windows，先配置可寫 Zig cache／TEMP）：

```powershell
python source/suite/tests/web_console_navigation_windows.py --zig <zig.exe> --sdk <SDK目錄> --out <獨立輸出目錄>
```

輸出 `ra2.webview-navigation-windows.v1`：`result.status`、ready／snapshot／DOM／action／denied-navigation／capture booleans、HRESULT、source SHA256、EXE SHA256、隔離 cache 路徑及 `game_process_accessed=false`。`capture.png` 為真 WebView CapturePreview，非 HTML 模擬截圖。

```powershell
python source/suite/tests/web_console_contract_test.py --zig <zig.exe> --sdk <SDK目錄> --report <contract-result.json>
```

輸出 `ra2.webview-navigation-contract.v1`：status、fake_com_executed、assertions_enabled、assertions_in_harness=58、sdk_interface_identities=12、sdk_production_compile_link、source SHA256；此 fixture 不執行 WebView Runtime，與前述真 native smoke 分開記錄。

原始本機證據：`diagnostics/webview-navigation-hotfix/{before,after-complete}/` 與 `contract-result.json`。最終可攜摘要、完整兩份 native result、contract report、來源／檔案 SHA256 及保留的事件欄位見 [webview-navigation-startup-hotfix.json](evidence/webview-navigation-startup-hotfix.json)。before 舊診斷曾輸出有限 Base64 前綴；可攜摘要只保留 source-kind，不複製 HTML payload。

## 官方依據

[Microsoft：載入 HTML 字串](https://learn.microsoft.com/en-us/microsoft-edge/webview2/concepts/working-with-local-content#loading-local-content-by-navigating-to-an-html-string) 說明最終 location `about:blank`、origin `null`；不能據此假設 Starting URI 也相同。[Microsoft：Navigation events](https://learn.microsoft.com/en-us/microsoft-edge/webview2/concepts/navigation-events) 說明不同 NavigationId 的事件可交錯；應按 ID 追蹤各文件。真實 Starting URI 及自我取消的結論來自本輪本機事件，而非僅依文件推測。
