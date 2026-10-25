# 覆蓋層切回遊戲後消失：整組層級保活

## 問題與證據範圍

使用者在 pre.5 回報：小地圖、單位統計面板和監控副攝像機在切回遊戲後仍可能消失，切換 Tab 統計面板後恢復。調查基於已發布 pre.5 來源 `308c619c6436b307eb0a4234f4b4d707c56359e6`，先查閱 INDEX 及同一 SQLite 筆記的 pre.13 覆蓋層恢復、pre.13 三窗與 pre.14 層級／同步繪製紀錄。

本次新結論來自宿主程式碼與受控 Win32 邊界測試，不需要新增遊戲函式地址、ABI 或欄位偏移。沒有新的遊戲 EXE 執行證據或適用 EXE SHA-256；原有資料布局與顯示准入條件保持不變。使用者回報是實機觀察，以下可重現缺陷與該觀察相符，但未取得發生當下的桌面 Z-order/DWM 記錄，因此不宣稱它是所有平台上消失的唯一成因。

## 已存在的定時恢復為何無法救回整組

pre.13 已加入 `overlayui::Recovery`：正常每秒重新上傳／置位，失敗或隱藏時每 250 ms 重試，失焦與菜單時暫停。統計面板及監控窗也確實會呼叫它。此次不是再增加一份相同的計時器。

共用 `overlay_stack.h` 為保持層級，將當前視窗插入最近的「更高層且 WS_VISIBLE」視窗後面。若遊戲在恢復前台或顯示模式時被提升到整組覆蓋層之前，原本高層的 cursor/watch 等 HWND 仍可保持 WS_VISIBLE，卻已在遊戲後面。較低層每次保活仍插到這個已被遮住的高層之後，因而可以無限重試而始終留在遊戲後面。

例如：原先為 `cursor > watch > minimap > statistics > world > game`，遊戲提升後變成 `game > cursor > watch > minimap > statistics > world`。舊版 statistics 再次插到 minimap 後面，不能改變遊戲仍在整組之前的事實。不能用 IsWindowVisible=true 推斷視窗已在遊戲上方。

舊 `overlay_stack_windows.cpp` 只檢查工具箱視窗互相的順序、顯隱與裁剪；上述兩種排列都滿足其內部順序斷言，因此未覆蓋這個回歸。

## 小地圖另外漏接了週期修復

`minimap::update()` 只在視窗首次顯示、位置或尺寸變動時呼叫共用 `place()`。雖然每次都 InvalidateRect/UpdateWindow，但這只重畫 GDI 內容，不能修復仍為 WS_VISIBLE 的錯誤 Z-order。小地圖沒有像監控窗一樣接入 `overlayui::Recovery`，因此在幾何與地形均靜止時，也沒有週期性的層級檢查入口。

## 修正

- 正常 `place()` 仍按原有 Layer 錨點處理請求視窗的位置與尺寸。
- 共用 `keepAbove()` 在目前前台 root HWND 改變時立即檢查桌面實際順序；同一前台每 250 ms 至多檢查一次。只有已註冊、目前可見且屬本程序的覆蓋層實際落在該前台 HWND 後面時，才修復整組。
- 修復將目前可見的覆蓋層依 Layer／同層 HWND 順序放回 topmost band。同一 UI thread 持有的整組使用 `DeferWindowPos` 批次；混合 UI owners 使用由高到低的個別 `SetWindowPos` anchor 鏈。兩條路徑均不改位置尺寸、不帶 SHOWWINDOW、不啟用視窗，故不會復活使用者關閉的面板。registry mutex 在呼叫 Win32 前已釋放，另用非阻塞 atomic guard 避免位置回呼重入修復。
- 小地圖接入既有 Recovery，包含隱藏時 suspend、同幾何每秒保活，以及重新符合顯示條件時立即恢復。
- 原有 foreground、菜單／非戰場、聊天、功能開關與主動關閉准入保留。保活沒有修改 Tab 顯示意圖、註冊全域置頂定時器、模擬 Tab 或發送遊戲命令；呼叫端先決定此時是否允許顯示。

兩條路徑皆採用最高層 HWND_TOPMOST、其餘逐個以緊鄰高層 HWND 作 anchor 的明確順序。[Microsoft DeferWindowPos](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-deferwindowpos) 明載同一批次的視窗必須具有相同 parent；該條件不能當成跨 UI thread 批次一定成功的證據。依 [EndDeferWindowPos](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-enddeferwindowpos) 文件會發生位置訊息回呼，因此不在 registry lock 內執行。個別鏈使用 [SetWindowPos](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwindowpos) 的明確 insert-after 語義；mixed-owner API 失敗的實測與更正如下。

## 回歸與反向證明

`source/assist/tests/overlay_keepalive_test.py` 直接 include production `overlay_stack.h`，並抽取執行 production `minimap::update()`／`hide()`；僅以邊界替身提供 Win32 HWND 順序、worker 完成圖與記憶體狀態，沒有重寫被測恢復演算法。

同一新測試使用舊 pre.5 header 分別得到兩個預期失敗：

1. 全部覆蓋層仍為可見，將遊戲升到最前；舊 statistics place 後，`repair must lift the whole visible overlay group above the promoted game` 失敗。
2. 小地圖幾何與可見標誌不變，經過 1000 ms；`unchanged visible minimap must periodically repair its z-order` 失敗。

修復後兩組通過，涵蓋五次反覆提升遊戲、靜止游標、無 Tab 操作、內部層級保持、不必要批次抑制、隱藏面板不復活，以及小地圖在失焦、功能關閉、聊天和 renderStatus 非戰場期間持續隱藏，重新符合條件後自動恢復。

`source/assist/tests/overlay_stack_windows.cpp` 擴大實際 Windows HWND 門檻：統計視窗由另一個 UI thread 持有，其 WM_WINDOWPOSCHANGED 回呼會取 registry mutex；四次提升真實 game fixture HWND，檢查共用修復恢復全組高於 game、跨執行緒回呼不在鎖內阻塞、內部層級、前台不被激活與隱藏面板保留。第二 owner 結束後再提升一次 game，檢查同 owner 的批次分支。測試向共用檢查函式傳入受控 game HWND，沒有搶走 CI 桌面的真正前台。Windows 執行結果以本版 Actions／發布證據為準；此紀錄不先宣稱該門檻已通過。

以上不能證明實際 DirectDraw/DWM 裝置丟失、所有平台宿主的前台判定或真實 Alt-Tab 畫面已驗收。沒有改動渲染亮度、資源選取、攝像機投影、追蹤目標生命週期或遊戲命令路徑。

## pre.6 首輪 Windows fixture 等待設計與驗證界線

首輪 pre.6 Actions `40155817238` 已建置成功，但 Windows job `121058491758` 停在 `Real overlay window stacking` 超過數分鐘。當時尚無階段日誌或執行緒堆疊，不能確定卡在初始化、位置批次、關閉或 assert 的 CRT 對話框，也不能據此宣稱 production `EndDeferWindowPos` 已被證明死鎖。

審查確認 fixture 的主執行緒已建立 HWND，卻以不派送訊息的 `WaitForSingleObject(ready)` 等第二個 UI thread，並在 `PostMessage(WM_CLOSE)` 後直接 `join()`。這不符合 [Microsoft Creating Windows in Threads](https://learn.microsoft.com/en-us/windows/win32/procthread/creating-windows-in-threads) 要求的窗口 owner 等待模型：另一個 owner 在建立、改變或銷毀視窗時可能同步傳送訊息，形成等待循環。production registry mutex 的 snapshot 仍在批次呼叫前釋放；本次先修正已確認不適當的 fixture 等待方式，保留跨 owner 批次本身供後續檢驗。

fixture 現在保留原有第二 UI thread、回呼取 registry mutex、四次 game 提升、全組層序、隱藏面板及 stale HWND 檢查，另作以下更正：

- ready 與 shutdown 使用有界 `MsgWaitForMultipleObjectsEx`／`PeekMessage` 訊息泵；各最多等待 5 秒。關閉時先等實際 native thread HANDLE 成為 signaled，再 `join()`。
- 260 ms 保活間隔也派送訊息，避免讓窗口 owner 在測試延遲期間停止處理同步通知。
- 每階段輸出立即 flush 的 checkpoint。位置呼叫、統計視窗 owner 狀態和迴圈序號也保留在 atomic 診斷欄位。
- 所有 `assert` 改成不可因 `NDEBUG` 消失的 `CHECK`，失敗時列印 expression、行號及階段後 `ExitProcess(2)`，避免 CRT assert UI 把失敗表現成長時間無輸出。
- 不持有 HWND 的獨立 watchdog 最多等待 20 秒；卡在同步 User32 呼叫時也能輸出最後階段並 `TerminateProcess(..., 124)`，讓 CI 明確失敗。

這次只更正 fixture 與診斷，沒有放寬層序斷言、移除跨執行緒覆蓋或改成非同步置位以避開被測路徑。編輯時本地沒有 Windows runtime，仍須由下一輪 Windows CI 驗證；不能把等待模型修正或 Linux 原始碼檢查寫成 Windows gate 已通過。

為了立即取得這項門檻的結果，發布驗證把它拆成平行的 `overlay-build`／`overlay-windows`，主成品仍由 `build`／`windows-ui` 驗證。發布器要求同一 SHA 的四個 job 全部成功；確切 Windows 結果以 `release-manifest.json` 指向的最終 Actions run 為準，首輪卡住的成品不能被拿來發布。

## 真 Windows 定位：混合 UI owners 的 deferred 修復失敗

加入明確失敗輸出後，Actions `40157058311` 的 `overlay-windows` job `121061004171` 到達 `promoted-game-recovery`，在第 0 次呼叫回報：

```text
FAIL line58 keepAbove(game) error=87 (ERROR_INVALID_PARAMETER)
phase=keep-above-batch statistics=message-loop iteration=0
```

這次是 production `keepAbove()` 的批次路徑回傳失敗，並非 watchdog 證實的死鎖。相同執行中，先前個別 `place()` 的跨 owner `SetWindowPos` 和重複內部層序檢查已通過。以上直接支持「本測試的 mixed-owner deferred 修復不可用」，不把它擴大解讀成所有 Windows 版本都禁止任何跨 thread deferred 呼叫；官方文件沒有明載這種普遍限制，也未提供此執行中是哪個 User32 子呼叫產生 87 的堆疊。

最小修正依 `GetWindowThreadProcessId` 檢查可見組的 owner IDs。同 owner 繼續使用原批次；mixed owners 在鎖外依相同排序逐個 `SetWindowPos`，每次成功後才以前一個高層 HWND 作下一個 anchor，任一呼叫失敗仍回傳 FALSE。250 ms 節流、atomic 重入 guard、逐個 HWND 身分／可見性重查及全部 NOACTIVATE／NOOWNERZORDER／NOMOVE／NOSIZE／NOSENDCHANGING flags 保留，沒有 SHOWWINDOW。

Portable double 現在具有獨立 thread IDs，並在 mixed-owner deferred group 回傳 FALSE，避免再次以永遠成功的 stub 掩蓋 API 邊界。使用修正前 header 執行新測試：single-owner case 通過，mixed-owner `place()` 斷言失敗；同一測試配合修正後 production header，single-owner batch、mixed-owner ordered chain 及 minimap lifecycle 三組全部通過。兩種組合均驗證五次重複遊戲提升、層序、隱藏面板與不必要重排抑制。真 HWND fixture 保留原跨 owner 斷言，再加同 owner 分支；修復後的 Windows 成功仍須以後續 CI 結果確認。

## 排除與未決事項

- `tabui::Controller::loseFocus()` 已只清除按住狀態，保留 latched 顯示選擇；本次沒有證據要求重新設計 Tab 語意。
- `suite::statsVisible()` 沒有被小地圖或監控視窗用作共同顯示開關，沒有確認「統計面板開關直接控制其他覆蓋層」這條路徑。
- 不同 PID 的平台 wrapper foreground 仍值得在有具體 HWND 日誌時另測；本次沒有為了保活而放寬原有 foreground 准入。
- 若實機仍消失，需要分清 HWND 被隱藏、被遊戲遮住或 layered/GDI surface 丟失；本次已修正能由現有程式和受控重現證實的層級與小地圖保活缺口。
