> 歷史檢查點：下列 3797 項摺疊測試及 source hashes 屬於首次收納版本，不能當作最新產碼驗證。後續已修正折疊內的真正錯誤與還原失敗可見性，見[4063 項後續記錄](2026-11-03-console-fold-error-visibility.md)；最終完整編譯另待 root 綁定。

# 控制臺收納與按當下選取所屬方自毀

基線：`5983b943e90694f7b2630608bc464d6f8ee148e3`。本頁記錄 Host/UI 路由與受控測試，不增加遊戲原生地址結論；原生選取／House 身分與事件入隊的 Runtime 驗證另由對應實作負責。未啟動、附加或操作遊戲，沒有雙機／RA2OL 驗收結果。

## 問題與結果

- `780`「允許維修通常不可維修的建築」移到「遊戲規則」，保留原設定與執行路徑。
- 單位類型清單預設摺疊；新增超武目標操作、控制全部實作與狀態、CRC 狀態、事件所屬方保留狀態的摺疊，合計 14 組。總開關保持可見。卸礦統計、科技生產進度、192px 計算模式詳情隨所屬設定收納。
- 長段說明原先已由 `note()` 存入 hover tooltip。本次移除剩餘低傷害／自動 CS 正常狀態及移動、卸載、逃生、磁電累計數的常駐列，改為對應開關／按鍵的 hover 資訊。真正的失敗、衝突及待處理命令仍可見；正常回復後，整列與空白一起移除。摺疊只是調整可見性和座標，保留 HWND、已填值與依賴狀態。
- 「全圖 CRC 原值鏡像」的開關、狀態、tooltip 與頁首說明統一標示高危；原生武器／射程查詢仍不因這次調整變成高危。

## 自毀輸入契約

「自毀選定單位的所屬方」位於實驗性功能，`Experimental/DestructSelectedOwner` 預設 `0`，`Hotkeys/DestructSelectedOwner` 預設未綁定 `0`。自願開啟後，可點擊立即執行或按獨立快捷鍵，不再使用舊所屬方下拉選擇來授權自毀，也不顯示確認彈窗。

Host 只發一次既有 `DestructPlayer`：`arg=0`、`arg2=0`、`requestToken=0`，要求 Runtime 從當下原生選取解析 Owner。Runtime 必須驗證完整非空選取中的存活單位、UID、Owner 及 House 身分完全一致；混合所屬方、失效或換代拒絕。聯機跨方仍依既有事件所屬方保留條件，未滿足回傳 `-42`。空選／混合／身分失效使用 `-7`，Host 顯示明確原因。這個 UI 改動不新增遊戲程式碼補丁。

Host 額外拒絕關閉、失焦、聊天、連線未就緒、命令／選取／指標請求忙碌與視角更新。發送前重讀快照後，PID、match 或 Shared 連線改變，或這次輸入已超過 150ms，也拒絕。事件不進命令排隊、不重試、不在連線恢復後補發；僅開啟這個功能也會建立必要的 Runtime 連線。

熱鍵 `1735` 在一般 pending/backlog 之前抽走。即使開啟「執行全部匹配功能」，自毀也不允許與其他動作共用物理組合；按下／松開／雙按模式共享同一物理鍵仍視為衝突。註冊、編輯及實際派發都檢查，避免稍後改鍵引入衝突。過期 WM_HOTKEY、延遲手勢與意外進入一般執行佇列的自毀動作都拒絕，持鍵不重複提交。

## 驗證與邊界

結果和 12 個 production 檔案、6 個 fixture 檔案的 SHA-256 收在 [證據 JSON](evidence/console-folds-selected-owner-destruct.json)，SHA-256：`c4305c289fe9b103e7c02c15b61bf9bed201a3ab4e6f905e14c095861d070281`。

| 真實 production 路由抽取 | x64 | x86 |
| --- | --- | --- |
| `destruct_owner_host`：send、排隊拒絕、當次輸入、WM_HOTKEY、過期／延遲／持鍵、INI、設定編輯 | 85 PASS | 85 PASS |
| `settings_folds`：分類、14 組、重排、搜尋、焦點、已填值、真正錯誤列出現與消失 | 3797 PASS | 3797 PASS |
| 既有 `building_event_host`：放置、模式、設定與熱鍵回歸 | 134 PASS | 134 PASS |

基線反例直接取編輯前 production 原檔：單位清單未摺疊、卸礦統計不屬於 9605、科技進度不屬於 9607、計算模式詳情不屬於 9609、CRC 開關未標高危；本次五項都相反。這些是程式碼／受控路由結果，並非原生面板視覺截圖或遊戲實測。

在工作區根目錄可重跑：

```powershell
work/toolchain/python/python.exe work/ra2-hook-audit/checkout/source/assist/tests/destruct_owner_host.py --zig work/toolchain/zig-windows-x86_64-0.13.0/zig.exe --out diagnostics/console-ui-followup/destruct-host
work/toolchain/python/python.exe work/ra2-hook-audit/checkout/source/assist/tests/settings_folds.py --zig work/toolchain/zig-windows-x86_64-0.13.0/zig.exe --out diagnostics/console-ui-followup/settings-folds
work/toolchain/python/python.exe work/ra2-hook-audit/checkout/source/assist/tests/building_event_host.py --zig work/toolchain/zig-windows-x86_64-0.13.0/zig.exe --out diagnostics/console-ui-followup/building-host
```

未決：真正遊戲中的事件執行、平台轉發和聯機同步。Host fixture 的模擬 Win32 與受控 Runtime 身分測試不能證明這些。
