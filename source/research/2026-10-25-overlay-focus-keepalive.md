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
- 修復以一次 `DeferWindowPos` 批次將目前可見的覆蓋層依 Layer／同層 HWND 順序放回 topmost band。批次不改位置尺寸、不帶 SHOWWINDOW、不啟用視窗，故不會復活使用者關閉的面板。registry mutex 在呼叫 Win32 前已釋放，另用非阻塞 atomic guard 避免位置回呼重入修復。
- 小地圖接入既有 Recovery，包含隱藏時 suspend、同幾何每秒保活，以及重新符合顯示條件時立即恢復。
- 原有 foreground、菜單／非戰場、聊天、功能開關與主動關閉准入保留。保活沒有修改 Tab 顯示意圖、註冊全域置頂定時器、模擬 Tab 或發送遊戲命令；呼叫端先決定此時是否允許顯示。

批次採用最高層 HWND_TOPMOST、其餘逐個以緊鄰高層 HWND 作 anchor 的明確順序。本程序註冊的這些覆蓋層均為桌面頂層視窗，符合 [Microsoft DeferWindowPos](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-deferwindowpos) 的同 parent 條件；依 [EndDeferWindowPos](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-enddeferwindowpos) 文件會發生位置訊息回呼，因此不在 registry lock 內執行批次。

## 回歸與反向證明

`source/assist/tests/overlay_keepalive_test.py` 直接 include production `overlay_stack.h`，並抽取執行 production `minimap::update()`／`hide()`；僅以邊界替身提供 Win32 HWND 順序、worker 完成圖與記憶體狀態，沒有重寫被測恢復演算法。

同一新測試使用舊 pre.5 header 分別得到兩個預期失敗：

1. 全部覆蓋層仍為可見，將遊戲升到最前；舊 statistics place 後，`repair must lift the whole visible overlay group above the promoted game` 失敗。
2. 小地圖幾何與可見標誌不變，經過 1000 ms；`unchanged visible minimap must periodically repair its z-order` 失敗。

修復後兩組通過，涵蓋五次反覆提升遊戲、靜止游標、無 Tab 操作、內部層級保持、不必要批次抑制、隱藏面板不復活，以及小地圖在失焦、功能關閉、聊天和 renderStatus 非戰場期間持續隱藏，重新符合條件後自動恢復。

`source/assist/tests/overlay_stack_windows.cpp` 擴大實際 Windows HWND 門檻：統計視窗由另一個 UI thread 持有，其 WM_WINDOWPOSCHANGED 回呼會取 registry mutex；四次提升真實 game fixture HWND，檢查共用批次恢復全組高於 game、跨執行緒回呼不在鎖內阻塞、內部層級、前台不被激活與隱藏面板保留。測試向共用檢查函式傳入受控 game HWND，沒有搶走 CI 桌面的真正前台。Windows 執行結果以本版 Actions／發布證據為準；此紀錄不先宣稱該門檻已通過。

以上不能證明實際 DirectDraw/DWM 裝置丟失、所有平台宿主的前台判定或真實 Alt-Tab 畫面已驗收。沒有改動渲染亮度、資源選取、攝像機投影、追蹤目標生命週期或遊戲命令路徑。

## 排除與未決事項

- `tabui::Controller::loseFocus()` 已只清除按住狀態，保留 latched 顯示選擇；本次沒有證據要求重新設計 Tab 語意。
- `suite::statsVisible()` 沒有被小地圖或監控視窗用作共同顯示開關，沒有確認「統計面板開關直接控制其他覆蓋層」這條路徑。
- 不同 PID 的平台 wrapper foreground 仍值得在有具體 HWND 日誌時另測；本次沒有為了保活而放寬原有 foreground 准入。
- 若實機仍消失，需要分清 HWND 被隱藏、被遊戲遮住或 layered/GDI surface 丟失；本次已修正能由現有程式和受控重現證實的層級與小地圖保活缺口。
