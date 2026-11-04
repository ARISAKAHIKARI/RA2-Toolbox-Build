# 折疊內的真正錯誤與還原失敗保持可見

此文件保留 WebView2 合入前的 14 組原生 UI 歷史檢查點。最新 15 組 Web／原生整合與來源綁定見 [後續紀錄](2026-11-03-console-pre1-integration.md)，下列 source/build hashes 只對應當時檢查點。

使用者要收起更多按鈕與狀態，把正常計數與說明移到 hover，同時不能隱藏真正錯誤。首次 14 組收納會把 CRC、事件所屬方、跨玩家控制、科技的失敗狀態一起藏進預設閉合組；低傷害過濾關閉後，原生判斷還原失敗的錯誤列也會消失。這是 Host/UI 狀態問題，沒有新增原生地址或遊戲程式碼寫入。

## 修正

`failedSettingGroups` 獨立於使用者的 `openedSettingGroups`。CRC `9613`、事件所屬方 `9614`、控制實作 `9612`、科技 `9607` 的負狀態強制展開，按鈕顯示「錯誤 · 已展開」，保留完整錯誤文字；錯誤未解除前不能藏起。解除後恢復原先收起／展開偏好，不修改功能值、已填文字或子控制項依賴狀態。

CRC、事件所屬方及命令所屬方的錯誤優先於失聯、關閉及 Master 模式提示。控制狀態文字移到一般狀態更新，讀不到 roster 時仍可顯示未還原錯誤。低傷害過濾關閉後仍顯示原生還原失敗；成功還原後移除錯誤列與空白。

截圖中的正常低傷害模式、停用自動 CS、移動任務／乘員／逃生／磁電累計數繼續移到 hover。維修例外仍用 `Modes.RepairUnrepairable`，只改到「遊戲規則」；自毀預設關閉、未綁定快捷鍵、獨立即時路由及不排隊／不重試不變。CRC 高危標籤與原生武器／射程查詢的原分類保留。

## 驗證

| 原碼抽取 fixture | x64 | x86 |
| --- | --- | --- |
| `settings_folds`：分類、14 組收納、完整 status block、五種負來源、同時失敗、恢復原偏好、低傷害關閉後還原失敗 | 4063 PASS | 4063 PASS |
| `destruct_owner_host`：既有即時自毀、INI、熱鍵回歸 | 85 PASS | 85 PASS |
| `building_event_host`：既有建造、設定、路由回歸 | 134 PASS | 134 PASS |

編譯含 `-UNDEBUG`，測試在引入標頭後另以 `#error` 拒絕 NDEBUG；每個 `check()` 實際呼叫 assert。fixture 抽取 production 函式與完整狀態分支，不改寫其中表達式；Win32 視窗 I/O 及 Shared 資料由測試提供。

本次測試綁定 `main.cpp` SHA-256：`4b491c372bc98b4904533d46c90bc962e35cf1ecb752230f81ce5b063854468f`。完整三模組 x64 Host 已有一個通過編譯與連結的歷史檢查點，EXE SHA-256 `36a3a82a47df54610d1f9e4a7cf081012c5c49e3ef7967c7e55f047ce3b76330`；它在最後的 9194 錯誤優先序修正之前，**不是最新完整產物**。最終含 handshake 的完整編譯及來源綁定待 root。各架構結果、source/fixture/報告 hashes 收在[證據](evidence/console-fold-error-visibility.json)，原始歷史檢查點 SHA-256 `3c9ecdfc076ccc2adff8879aa3768603dcc877f47953da09f0704318a61bf15a`（後續僅追加 historical/superseded 標記）。

舊 [console-folds-selected-owner-destruct.json](evidence/console-folds-selected-owner-destruct.json) 已標明歷史檢查點；原 3797 項結果與舊 source hashes 保留，不能冒充最新產碼驗證。

從工作區根目錄重跑：

```powershell
work/toolchain/python/python.exe work/ra2-hook-audit/checkout/source/assist/tests/settings_folds.py --zig work/toolchain/zig-windows-x86_64-0.13.0/zig.exe --out work/ra2-hook-audit/diagnostics/next-ui-ore-view/settings-folds-error-review
work/toolchain/python/python.exe work/ra2-hook-audit/checkout/source/assist/tests/destruct_owner_host.py --zig work/toolchain/zig-windows-x86_64-0.13.0/zig.exe --out work/ra2-hook-audit/diagnostics/next-ui-ore-view/destruct-host-error-review
work/toolchain/python/python.exe work/ra2-hook-audit/checkout/source/assist/tests/building_event_host.py --zig work/toolchain/zig-windows-x86_64-0.13.0/zig.exe --out work/ra2-hook-audit/diagnostics/next-ui-ore-view/building-host-error-review
```

報告在 checkout 同層 `diagnostics/next-ui-ore-view`。這些受控測試不證明真實 Windows 畫面、遊戲事件執行或 RA2OL 雙機同步；未啟動或附加遊戲進程。
