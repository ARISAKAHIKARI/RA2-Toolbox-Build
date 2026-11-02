# 劇情觸發改視野後，小地圖仍沿用舊範圍

基線為 `8437c37`。問題是遊戲觸發改變 VisibleRect 後，小地圖雖重新採樣，顯示範圍仍不正確。本輪先重讀 [live bounds](2026-10-25-live-map-boundaries.md)、[Tactical 可繪製邊緣](2026-10-25-renderable-map-borders.md) 及 [pre.12 Radar raw 邊界](2026-10-25-minimap-bottom-pixels-pre12.md)。持久 Library 本輪不可用，新增發現記在本頁供原筆記後續寫回；沒有建立替代資料庫。

## 根因與原生證據

Geometry、Sampler、SceneBank 和發布檢查原本都比較 live VisibleRect。問題不是完全忘記失效，而是把 **已配置 RGB backing 的尺寸**當成**目前雷達取景範圍**。

本輪唯讀樣本 SHA-256：`268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0`。使用者已更換私人樣本，不能把前次 `247f7288…` 當成本輪的輸入。沒有啟動、停止或連接遊戲程序；不發布私人 PE、反組譯轉儲或原始記憶體。

| 原生路徑 | 本輪確認 |
| --- | --- |
| Radar `SetVisibleRect`，`654490..654644` | 呼叫 Map 基底，再以 Cell 迭代及 mode-1 usable 求 `+1490/+1498` offset 與 `+14A4/+14A8` 活動尺寸；不寫 `+1240/+1244` |
| Map `SetVisibleRect`，`567230` | 交集及 2／2／2／6 內縮仍由原生程式執行；外部讀取 live 結果，不二次內縮 |
| `578350`／`578290`／`578460` | 原生 Cell 迭代與含 Level／頂邊坡面的 usable 判定 |
| backing 建立，`6547C0` | `+123C` 已有 RGB buffer 時跳過配置；`+1240/+1244` 僅於空 buffer 配置分支 `65486B..654877` 寫入 |

原先 pre.12 用 raw backing 確認像素跨度，適用於 backing 與當前取景一致的捕獲。這不能推廣為每次 SetVisibleRect 都會重新配置 backing。

`radar_trigger_native.py` 在隔離 x86 emulator 執行樣本原始 `654490`、`567230`、`421B60`、`578350`、`578290`、`578460`。只替換 redraw 通知，物件向量設為空；矩形交集、內縮、Cell 迭代及坡面／高程判定都是原指令。

64×64 平坦合成地圖的例子，同一 simulation frame 內：

| live VisibleRect | 當前 offset | 當前跨度 | 已配置 backing |
| --- | --- | --- | --- |
| `{2,2,60,56}` | `{59,69}` | `120×114` | `120×114` |
| `{12,18,24,20}` | `{39,101}` | `48×42` | `120×114` |
| `{12,24,24,14}` | `{39,113}` | `48×30` | `120×114` |
| `{6,8,50,46}` | `{51,81}` | `100×94` | `120×114` |

三種 Map 尺寸、平坦／0..6 格高與坡面、縮界／只改上下／擴界／恢復，共 15 組。9 組活動跨度已變而 backing 尺寸不變；編譯後 C++ 取景計算與原始 native 結果逐項比較。

## 修正

`radar_map.h::ActiveProjection` 在只讀 Cell 座標、Level 和坡面後，依同一 mode-1 usable 結果求當前雷達取景。結果依來源幾何身分快取，正常每次渲染不重掃全圖。所需字段失讀或座標身份不符時拒絕新候選。這是 **Radar 的取景矩形**，不把 usable 當成 Tactical 的逐格像素遮罩，也不修改原生可用區、相機或遊戲資料。

Geometry 將私人呈現 offset／span 與來源 backing 身分分開。原始 `+1240/+1244` 和 native offset 仍用於 fresh publication／Live-mask 身分檢查；取景、目標像素地形、示意圖、camera rectangle、導航及面板比例使用當前 span。否則把新 `48×42` 和仍為 `120×114` 的原生配置相等比較，會拒絕所有新畫面。

此流程僅由小地圖的 Sampler 選用。共用 SceneBank 的投影已相容時仍直接使用；活動 span 與 native allocation 不同時由小地圖完整重採樣，其調整後的私人 bank 不回寫共用世界，避免改變其他 Tactical 相機的來源語義。恢復到一致投影後可重新共用。

## 驗證與限制

`radar_trigger_projection.cpp` 接真正 Sampler、SceneRenderer、Live shroud、publication、layout 及 navigation。它保持同一 House、Cell array、frame 和 `120×114` backing，依原生重放輸出改變 live bounds／offset：

- 舊取景保留 `120×114` 的反例仍可重現；修正版依序為 `48×42`、`48×30`、`100×94` 並能恢復。
- 像素、單位投影、導航及面板尺寸隨當前跨度改變；舊世界完成圖不符合新 bounds，不能重用。
- 調整後的取景仍通過實際 native allocation 的 fresh 身分驗證，Live mask 能完成。
- 讀取失敗保留已完成 owner，恢復後可發布；不變幾何的 projection cache 不重讀 Cell，reset 後不借用同地址舊資料。
- 明確保留 Native-backing 模式給其他呼叫端；沒有改遊戲尋路、Tactical 合法邊緣或素材像素。

`tools/test_radar_trigger_windows.py` 記錄來源／二進位雜湊、x86/x64 的上述整合及原指令對照，再執行既有 visibility 發布、邊界、最終星級、探圖連續性、compact 採樣、大圖與 SceneBank 發布回歸。此證據是隔離原生函式及工具箱 C++ 的行為，不宣稱完整遊戲觸發、GDI 畫面或雙機聯機已現場驗收。

[最終 source-bound 報告](evidence/radar-trigger-active-projection.json) 綁定 14 個來源檔、9 組編譯後 fixture 及 x86/x64 各 15 組原指令／C++ 對照；全部通過。私人 PE 驗證前後 SHA 相同。
