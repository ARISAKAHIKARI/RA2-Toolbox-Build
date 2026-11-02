# 隱藏原光標：保留設定，補齊原生提示框佈局

本輪確認兩個獨立問題。使用者目前選擇「保留原光標」，所以未呼叫 HideCursor；舊版啟動遷移又曾在缺少 `CursorNoCodeWriteDefaultsVersion` 時把已保存的 `hide` 改存為 `keep`。現在取消這個自動改寫，保留已保存的 hide/keep/sha；無法從已改成 keep 的設定分辨它是遷移結果還是本人選擇，因此不把所有 keep 強制改回 hide。使用者已明確要求移除隱藏光標的「高危」標籤，相關主開關、模式與提示已同步。

另一個實際漏修是提示框仍受第二次原生可見性判斷阻擋。舊修復只接管 `7F74D8` 的文字方法 `479050`：這能取回單位名稱和建造價格，但 `ToolTipManager 724AD0` 隨後在 `724B3E` 呼叫 vtable+4 的 `478BA0` 計算提示矩形。該方法在 `478BB4` 遇 WWMouse RefCount 負數就返回 false，原生 manager 因而清掉整個提示框。

新增 `7F74C8` 佈局 vtable 槽接管。只有確認是本工具仍持有的同一 WWMouse、恰好一個 Hide（RefCount=-1）、原生 getter/文字/佈局簽名吻合時，才重播 `sub esp,1C; mov [esp+8],ecx`，以可見分支的 EAX=0 經 `478BBA` 繼續原生佈局。保留原生字體測量、矩形定位、裁剪和 thiscall 參數回收；不改 `.text`、RefCount、HideName 或模擬資料。額外遊戲 Hide（-2）、外部 getter、不同物件或未知方法保持原生結果。root 已獨立反彙編確認這個 continuation 的暫存器、堆疊及原生 caller。

兩槽分別經既有 manager/registry 記錄實際寫入及恢復責任；部分失敗、foreign slot、暫時失讀不冒稱卸載完成。BuildTimeTooltip 單獨使用時仍只持有文字槽；關閉建造時間不會撤掉 Cursor 要用的文字，關閉 Cursor 後保留仍啟用的建造時間文字槽。Cursor IPC 更新為 v155，新增提示修復狀態；介面分開顯示等待 Hide、等待提示、提示修復失敗和已就緒，切換時記一條狀態日誌，沒有逐幀刷檔。

Hook allowlist 現為 52 處：診斷器原來的 48 個可排程站點、3 個已禁用站點，以及 1 個 Cursor 專用佈局槽。Assist-only 診斷器明示跳過該 Cursor 槽，不能把單獨 BuildTime 測試當成兩槽都已測試。

## 驗證

- `assist/tests/cursor_startup_activation.py` 抽取實際 startup/start/status 函式，28 項各在 x86/x64 通過：保存偏好、命令列覆蓋、局中首次 Hide、Keep 的資源讀取、等待/失敗/成功狀態。Hide 不依賴 Frame Hook 或 Assist 功能開關。
- `tools/test_cursor_tooltip_windows.py --game-exe ...` 在隔離 x86 Win32 測試程序內載入本機 PE 的完整 `724AD0..724B7C` manager 和 `478BA0..478DAD` 佈局方法，僅字型測量/文字來源使用合成 stub。8 組兩側欄方向、正常/邊緣位置、單位/價格對比均通過：原生可見、原始 Hide、只修文字的舊失敗、雙槽修復、額外 Hide。新結果的文字、整個提示物件、矩形與 active item 與原可見路徑相同，callee-saved registers/ESP 不變。
- 同一測試執行真正 CursorRuntimeMessage 的首次啟用、配對恢復、暫時失讀、物件更換、foreign slot、部分安裝/還原失敗、共享建造時間開關、失焦保持，以及原有幀通知回歸；全部通過。registry 與診斷清單回歸亦通過。

本輪私有 PE 在測試前後 SHA-256 均為 `247f72881e1a68c8fc305e3702dc0100a72d17601305999026393c340d5daeb0`；與先前研究的 `268a81...` 樣本不同，未改寫舊研究的樣本事實。本輪 runner 固定允許已核對樣本，記錄所用完整方法的 SHA-256，拒絕其他檔案。公開證據 [cursor-hide-tooltip-layout.json](evidence/cursor-hide-tooltip-layout.json) 只有結果和雜湊，不含私有 PE 或原始 dump。

上述為隔離程序的原生流程及來源測試；未操作正在執行的遊戲，未宣稱完成 RA2OL 現場畫面或雙機聯機測試。尚未證實的 Windows SetCursor／建造欄切換猜測沒有順帶改動。
