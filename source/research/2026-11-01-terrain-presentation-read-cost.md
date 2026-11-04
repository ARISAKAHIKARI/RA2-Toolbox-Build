# 地形準備：單次畫面的共用資料重讀

使用者回報優化仍不足，並指出目前未開啟遊戲畫面升級。重新檢查既有實機小地圖日誌的 219 個五秒抽樣：地形平均 16.36 ms、峰值 322.44 ms；palette 平均 16.31 ms，而 VXL／decode 平均只有 0.69／0.85 ms。這些是舊包的稀疏抽樣，不是本輪優化後的 FPS 或完整延遲分布。彙整位於本機 `diagnostics/actor-render-performance/existing-live-minimap-detail.json`。

## 已改的重複工作

`SceneRenderer` 每次仍逐格檢查當前地形光照。原本 `lighting::TintCache` 只重用 converter 的 tint，`converterMetadata` 卻仍對每格讀格式兩次、rows、vtable、Scenario、game frame、bpp 與 data 指標；即使 table 本體已快取，仍有大量重複驗證與讀取介面呼叫。

現在同一個、僅存活一個 presentation 的 TintCache 也重用 converter metadata。每格 intensity 不快取；fallback 的 rows／scalar／rgb555 是輸入 key，保留原函式在失讀時部分欄位沿用 fallback、table 清空的行為。下一次 presentation 重新採樣。原本 `fresh` 檢查傳入空 TintCache，仍直接讀取，沒有用新快取跳過提交前的光照一致性檢查。

全圖穩定來源的 `verifyVisible` 沒有 crop 可判斷，現在省去重算已確認 cell 的 tileOrigin；全格 signature 已比較成功時，也不再把同一批 Paint 複製到臨時陣列後立即清掉。局部 crop、未完成／失敗建圖、已記錄 probeChanged、素材與地形變更仍走原路徑。沒有縮小世界、跳過地形格或降低刷新率。

這不等於減少同數量的系統讀取：小地圖的 SharedReadView 使用正式 ProcessReader 頁快取。收益包含減少頁查表、欄位驗證、signature 前置與臨時複製；不能把每次 read 介面呼叫都說成一次 ReadProcessMemory。

## 量測與驗證

`assist/tests/terrain_presentation_performance.cpp/.py` 將保存的舊 SceneRenderer 與舊 cell 葉函式，和目前 production 同時編譯。在 10,000 個地形格、600×300 輸出、24 個新遊戲幀上，每幀比較完整 RGBA。使用合成記憶體 reader，量測沒有 GPU、真實遊戲或平台參與。

| 同一完整場景，每次 presentation | 原實作 | 新實作 |
| --- | ---: | ---: |
| read 介面呼叫 | 120,016 | 40,024 |
| converter／格式讀取介面呼叫 | 60,005 | 11 |
| 每格光照檢查 | 10,000 | 10,000 |
| 全圖無用幾何檢查 | 10,000 | 0 |
| 已驗證 Paint 再複製 | 10,000 | 0 |
| terrain 準備平均 | 0.664550 ms | 0.527329 ms |
| 完整 render 平均 | 0.896258 ms | 0.754000 ms |

另外 36 組檢查逐欄位比較 metadata 失讀／非預設 fallback；同 game frame 的下一 presentation 變更 intensity、tint、格式，仍重新驗證。次幀 converter row bytes、地形高度、底色及 tile 移除，前後完整輸出相同。

`assist/tests/terrain_light_reads.cpp` 另用**正式 ProcessReader**，只讀本測試程序自己配置的 10,000 個 Cell。兩個獨立 reader 各跑 30 個新 read epoch、交替順序；每輪比較完整 State 與 table bytes，並變更個別 Cell intensity。這裡沒有啟動、讀取或寫入遊戲。

| 正式頁快取，10,000 Cell／epoch | 原實作 | 新實作 |
| --- | ---: | ---: |
| 真正 ReadProcessMemory 次數 | 1,255 | 1,255 |
| read 介面呼叫 | 120,007 | 40,015 |
| 葉函式批次平均 | 2.156813 ms | 1.633977 ms |

必要頁仍全部讀取；此受控批次約減少 24.2% CPU 時間。不能把它直接套用到舊日誌的 16.36 ms，也不能和另一項 palette 優化百分比相加。既有 `radar_cpu_reuse.cpp` 另驗同幀光照／raw palette、次幀 converter rows、失讀恢復與 decoration 排除。

## 可重查來源

本機 evidence 根目錄：`work/ra2-hook-audit/diagnostics/terrain-presentation-performance/`。`current/test.log` 是完整 SceneRenderer；`reader/test.log` 是正式頁快取；`radar-cpu-reuse.log` 是既有回歸。before 與 after-terrain 保留當時源碼，不包含私有遊戲素材。後續另加失敗診斷可能改變工作樹 radar_scene.h 的 hash，不覆寫此量測 snapshot。

- before radar_scene SHA-256：`396a24b46e57411c2ee33b3cc5b0c58c383b6b0db3a152a7cbc60bec8078bfb1`
- before scene_lighting：`aab801dc022619552e0e03e35e1da2feaf69d3fe518eb7fcae8de773f8523cc7`
- after-terrain radar_scene：`cf7ba611e3569090fc825ef5adf6c77685a5a92f8055d2dabfb67b5e31ba9aca`
- after-terrain scene_lighting：`be08ca2269788c7080482fc2adf23185d4713076d453cf4982ce307110e2e1e4`

```text
python source/assist/tests/terrain_presentation_performance.py --zig <zig.exe> --baseline-scene <before/radar_scene.h> --baseline-lighting <before/scene_lighting.h> --build-dir <output>
python source/assist/tests/terrain_presentation_performance.py --zig <zig.exe> --baseline-scene <before/radar_scene.h> --baseline-lighting <before/scene_lighting.h> --build-dir <reader-output> --reader-only
```

本輪不涉及 Hook、命令或遊戲記憶體寫入；仍需新包實機日誌確認哪些對局與地圖受益。
