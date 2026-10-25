# 跟蹤窗 2／3 窗快速重排：取樣結果不應控制窗口生命週期

## 問題、沿用證據與範圍

使用者回報同時有三個跟蹤窗時，排版會在兩窗、三窗之間高速切換。調查前已讀 INDEX、pre.13 跟蹤組筆記、RenderFix3 非同步筆記及同一持久 SQLite。

這次新增證據是**現行生產程式的來源檢查、注入讀取失敗的 C++ 執行，以及真實 `asyncwork::Latest` 背景執行緒配合記憶體／GDI／Win32 邊界替身**。未新增原版 EXE 反組譯或受控原指令執行，沒有實機 Windows 錄影或實戰 FPS 測量。既有欄位所屬支援樣本 SHA-256 為 `7b8a068535d6af06845edf95ae829b113d00c02909330e16f197426cd7db94b6`；此雜湊不代表本次在該 EXE 上實際重現了 Windows 合成。

沿用物件欄位：vtable `+0x0`、ID `+0x10`、Health `+0x6C`、Alive `+0x74`、Limbo `+0x81`、OnMap `+0x90`、位置 `+0x9C/+0xA0/+0xA4`、Transport `+0x11C`、CloakState `+0x220`。本次沒有修改遊戲 ABI、相機投影、遊戲物件或任務。

## 可直接重現的原因

`watchengine::Renderer::update` 舊流程會把本輪 `watch::sample` 結果直接覆寫舊 Frame，再用 `bool(frame.focus)` 決定窗口是否存在。外部讀取暫時拿不到焦點座標時，sample 返回空 Frame；排版隨即按兩窗計算。下一輪讀取成功又按三窗計算。這與素材是否完成解碼無關：只要單次焦點取樣失敗就能重現。

以同一 fixture、三個仍然存活且已固定的目標，每隔一輪只令第三個目標的 `+0x9C` 讀取失敗，實際編譯執行結果為：

| Renderer | 六次連續更新的可見窗口數 |
| --- | --- |
| 修改前 HEAD `fd62492` 的 `watch_engine.h` | `3,2,3,2,3,2` |
| 本次修改 | `3,3,3,3,3,3` |

此對照使用相同的構造記憶體、現行採樣依賴與邊界後端，只替換舊／新 renderer。它證明這條程式路徑能產生同型症狀，沒有據此宣稱已確定使用者機器上每一次閃爍的底層讀取錯誤碼。

另外確認三條會放大不穩定的路徑：

- `follow` 只有 ID 和 Health 的讀取失敗得到處理；其餘生命旗標透過 `GameView::get` 失敗時成為 0，可能誤判死亡。
- 每個非同步 request 都覆寫 worker 的 tracks。UI 尚未收到 worker 的死亡／組內接替結果時，已排隊的舊 request 可把舊目標重新帶回 worker。
- UI 重複消費同一個完成結果，且用 request 起始時間的 500 ms 門檻隱藏窗口。素材工作較慢會被當成窗口失效，舊結果也可能覆寫前台剛確認的生命週期變化。

## 修正後的行為

窗口槽位由持續存在的 Track／自動目標控制，不由一輪畫面是否取樣成功控制。相同目標失讀時保留最近完整畫面與固定位置；首次取樣尚未成功的已新增槽位保持位置，不以其他目標的舊圖填補。不同目標、手動關閉、確認死亡且沒有原組接替者、對局／程序更換仍走真正的生命週期切換。

Frame 採用候選副本，只有成功的 refresh／sample 才提交。這也避免 refresh 已修改部分 actor，之後發現另一個讀取失敗而污染已顯示的完整 Frame。失讀期間不更新 motion，恢復後重取實際狀態。

所有死亡判斷依賴的欄位改成顯式檢查讀取結果。未知讀取保留跟蹤；存活的 off-map、載具內、limbo 或隱形狀態不使窗口消失。讀到確定死亡或 pointer＋ID 身份已更換時仍按原組、五格內最近成員接替，沒有候選就清除，不將已死亡目標永久釘在窗口中。

Worker 接受新增／關閉／對局或設定切換產生的新控制 revision；同一 revision 的後續 request 不再覆寫 worker 已更新的跟蹤狀態。UI 每份完成結果只消費一次，移除以 500 ms 渲染耗時決定窗口可見性的規則。菜單、失焦、聊天、功能關閉與對局失效的既有隱藏／重置仍生效。watchEnabled／watchDanger 的設定切換也推進控制 revision，保證快速開關時清除自動跟蹤抑制的決定不會在待處理快照替換中遺失。

### 與動態地圖邊界的非同步發布整合

邊界整合 review 確認：正常 `ReadSession::read` 使用 4 KiB page cache，因此 sample 對 `Map+0xF4` 開始的六個整數重讀，只能確認本輪快取內一致，不能排除資源解碼期間劇情觸發改寫 VisibleRect。沿用新 `mapbounds::Bounds` 的 width／height／left／top／columns／rows 六欄，本次沒有新增其原版語意的反組譯證據。

`watch_host.h` 在整批渲染完成、準備返回 Result 的末端，使用 **`renderReader.readFresh`** 雙讀當前 Bounds。任一 shown 且有有效 frame.focus 的畫面若採樣 Bounds 與當前 Bounds 不同，或 fresh 讀取無效，就拒絕整批完成結果。`Latest` 保留已發布的結果，UI 的槽位與排版因此不隨拒絕切換；下一份完整新邊界畫面才替換它。Result 同時攜帶 Frame.bounds，便於檢查發布域。這不是把讀取延遲判為目標死亡，也沒有以舊對局或控制 revision 冒充新結果。

## 驗證

- `source/assist/tests/watch_host.cpp`：三窗共用 300 種模型的既有回歸；新增 120 輪第三目標生命欄位失讀／恢復，要求所有槽位、位置、大小固定，失敗時仍是最近完整 Frame；地圖指標暫時失讀；載具狀態；真正死亡移除；舊控制快照不能復活；新的控制 revision 可以明確重新跟蹤。
- `source/assist/tests/watch_slots.cpp`：逐個令 vtable／ID／Health／Alive／Limbo／OnMap／Transport／CloakState 讀取失敗，保持原跟蹤；存活 off-map 保留、確認死亡的 off-map 成員接替；沿用五格邊界、指針重用、原選中組限制與連續死亡測試。
- `source/assist/tests/watch_async_test.py`：執行真實生產 host 與真實背景 worker，於外部 reader 邊界刻意阻塞。主執行緒可繼續更新，模擬時鐘推進 1.2 秒後三窗仍固定；同一完成結果不會重新覆蓋已確認的死亡；渲染中關閉使舊完成與排隊快照失效；快速切換自動跟蹤設定仍能正確清除抑制。另以真正分離的 cached read／readFresh 邊界，在 renderer 已採樣並完成畫面後暫停 fresh 發布讀取；保持 control revision 與 simulation frame 不變，把 VisibleRect.rows 從 64 改為 40。確認舊域完成結果被拒絕，UI 三窗的像素指標與框位不變，下一輪才發布 rows=40 的新結果。此具體 review 風險的單項 gate 已通過。
- `source/assist/tests/watch_windows_test.py`：修復它自 pre.3 抽出 renderer／非同步 host 後已失配的測試支援，接回真實 `watch_host.h`；確認現行生產窗口處理器的單擊相機、Shift＋右鍵關閉、不搶焦點、縮放、定期恢復和菜單隱藏。Win32 與 GDI 仍是邊界替身，不當作 Windows 實機驗收。

以上四個針對性 gate 通過。舊 `watch_windows_test.py` 不在 pre.4 的七項 Python workflow 清單內，且本次調查時因缺少新 host／overlaystack 依賴而編譯失敗；不能把此前 pre.4 CI 成功解讀為該舊 fixture 也通過。新兩項 Python gate 需納入後續驗證流程。

## 尚未證明

沒有使用者機器的 ReadProcessMemory 錯誤碼、三窗動畫錄影或同場景 Windows 合成量測，因此無法將每次既有閃爍都歸於唯一底層失讀來源。修正針對已重現的窗口生命週期／畫面可用性混用與非同步舊狀態回寫，不以加延時或忽略真正死亡掩蓋問題。失讀時保留舊畫面並非聲稱場景仍在實時刷新；讀取恢復後才以新完整 Frame 更新。
