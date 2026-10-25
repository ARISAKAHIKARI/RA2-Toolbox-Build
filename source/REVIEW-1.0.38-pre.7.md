# 1.0.38-pre.7 資訊刷新率自適應

## 問題與設定所有權

舊資訊顯示只接收固定 `Display/FPS`；統計頁的資訊刷新率是透過 suite bus 交給 assist 所屬 UI 執行緒保存。光標的 `suite::refreshRate()` 則已包含使用者指定的光標 Hz，不能代表螢幕目前模式。光標動畫的 AdaptiveTiming 又是另一個補間時長設定。

本版保留 `Display/FPS` 的 5–500 手動值，新增 `Display/AdaptiveFPS`。缺少新設定時保持原手動模式；auto 不改寫手動數字。`DisplayState` 追加模式與有效 Hz，既有選項 ID 保持不變。Windows 控制項 9160 是 assist owner，單位統計頁透過原有訊息路徑轉發。

## 螢幕讀取

共用 `monitorrefresh::Tracker` 由各 UI 執行緒持有。使用有效 game HWND；沒有遊戲時使用工具箱視窗。每次控制服務只檢查目標與 monitor handle，模式查詢限於目標／螢幕改變、強制更新、通知或距離上次讀取滿一秒。繪製只讀快取，不逐幀查詢顯示模式。

`MonitorFromWindow` 按視窗與螢幕的最大交集選取，螢幕外用最近螢幕。`EnumDisplaySettingsExW(ENUM_CURRENT_SETTINGS)` 檢查 `DM_DISPLAYFREQUENCY`；0／1 是硬體預設標記，並非可直接採用的 Hz。失敗或無效資料使用 60，超過 500 則夾至 500。

- [Microsoft MonitorFromWindow](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-monitorfromwindow)
- [Microsoft EnumDisplaySettingsExW](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-enumdisplaysettingsexw)
- [Microsoft DEVMODE](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/ns-wingdi-devmodea)
- [Microsoft WM_DISPLAYCHANGE](https://learn.microsoft.com/en-us/windows/win32/gdi/wm-displaychange)

通知不能取代週期重查：解析度不變時，Hz 仍可能變更。此設定也不代表與掃描線鎖相或控制 VRR。

## 顯示與工作負載

assist 的高精度資訊時鐘、顯示採樣、跟蹤窗和小地圖請求使用解析後的 Hz。原有 16 ms 控制服務、4 ms 快速輸入及遊戲命令時間基準保持獨立。

小地圖等待改用微秒，避免 144 Hz 被毫秒整除成 6 ms；單純 Hz 改變喚醒等待，不增加世界 generation，不清地形、素材或已完成畫面。

統計面板原來的固定 16 ms 展示改為高精度單 pending 時鐘，控制服務與 250 ms 數量採樣獨立。它在自己的 UI 執行緒讀取同一策略；當助理模組停用或演示位於另一螢幕時，當前 Hz 顯示使用統計自身實際目標。

## 驗證界線

螢幕解析測試覆蓋 60／144／165／240 Hz、同 HWND 跨螢幕、同 monitor 模式改變、無效值／缺欄位、讀取失敗、節流、時鐘回繞和純快取讀取。主線測試覆蓋手動值保留、設定同步與時鐘目標，統計與小地圖另檢查訊息合併、停止、改 Hz 喚醒及快取保留。

Windows smoke 使用出貨 EXE 的真正控制項和 INI，切換 auto、讀取有效 Hz、重啟恢復、關閉 auto 後恢復手動值，並執行統計演示與關閉。原有 PE／ABI、視窗和渲染數值門檻繼續要求通過。

發布結果以 `release-manifest.json` 的確切提交與 Actions run 為準。受控多螢幕輸入不是實體多螢幕測試；未宣稱完整遊戲 FPS、所有顯示驅動或聯機已驗收。本版未新增引擎地址或逆向結論。
