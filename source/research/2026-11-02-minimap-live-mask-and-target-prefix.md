# 實景小地圖：探圖閃爍、整圖遍歷與最終尺寸地形重用

基線為 `2bb821569c19e79a5869ec043b2ee47fcade1d7a`。本輪處理新探開區域反覆變暗及整圖刷新成本。只讀遊戲資料；沒有新增 Hook、遊戲寫入或原生函式呼叫。沒有現場 RA2OL、硬體 GPU 或遊戲 FPS 驗收。

持久研究 Library 本輪不可用，新增發現保存在本筆記供原檔後續寫回。沿用已記錄的 Map `87F7E8`、Cell 陣列 `+13C/+140`、投影位移 `+1490/+1498`、Cell 座標 `+24/+26`、探索 `+12C`、fog `+140` 與 selector 表 `7F4194`。本輪不新增地址、ABI 或原生函式推論；上一輪核對樣本 SHA-256 為 `247f72881e1a68c8fc305e3702dc0100a72d17601305999026393c340d5daeb0`。以下 fixture 使用合成記憶體及編譯後的正式 C++，不執行或發布私有遊戲 PE。

## 已重現的閃爍路徑

`SceneRenderer` 原先只有局部 `region.active` 相機取得即時完整 visibility；整圖小地圖使用 Terrain bank 的 shade。多個工作者的 Terrain bank 由分批讀取形成，其 actor 觀測時間較新，不代表該 bank 每格 shade 也較新。交替收到舊、新探索 bank 時，小地圖可交替重播黑幕。

此外，`sourceHidden` 原本參與整圖靜態來源比較，隱藏格也從靜態 TMP 清單剔除。因此探圖會使不變的地形重建，較舊 bank 又能暫時移除剛探開格的地形來源。

現在整圖最終遮罩也使用本次讀取的完整 visibility；失讀、Cell 身份／座標不符或世界改變時拒絕候選，由宿主保留上一完成畫面。遮罩有獨立的 33ms 完整捕獲預算，不限制動態單位的呈現頻率。陰影的合法變暗仍生效，沒有把已探開 bits 永久 OR 起來。TMP 來源獨立於探索保存，由最終目的地遮罩決定是否可見；圖解、物件與 overlay 的原有可見性准入不因此放寬。

真 `SceneRenderer` fixture 先使 Cell 即時探索，再交替送入舊、新 shade/sourceHidden bank。基線輸出交替黑／亮；修正版像素保持一致，靜態地形 draw 數不增加。另覆蓋失讀保留與恢復、合法重新變暗、frame 倒退、尺寸與世界檢查。16 個獨立 visibility 捕獲 epoch 中讓單位持續移動，避免整幀重用；每幀與冷建 Renderer 的像素相同，靜態地形快取仍命中，真正 alpha 改變則失效。

## 整圖還有哪些成本

小地圖在基線已啟用 `outputSized(true)`，TMP、單位及遮罩的光柵尺寸跟隨面板，並不是本輪才改為畫小圖。整圖仍需取得全圖狀態；把視窗縮小不會使遠處 Cell 自動不需要讀取。

本輪修正三處具體成本：

1. Sampler 的 4096 預算原先按 512×512 固定地址槽計算。空槽與非法格不需跨進程讀取，卻消耗每輪預算。小地圖現在跳過這些槽，仍限制每輪最多 4096 個實體主格讀取。128×128、32640 實體 Cell 的完整 bank 為 64 輪縮為 8 輪，所有顏色、shade 與材質身份一致。這是發布延遲改善，不能當作 FPS 八倍；完整掃描的 CPU 工作並未消失。
2. CPU 呈現原先即使已保存不可變地形繪製前段，每幀仍重播全部 TMP。本輪在最終面板尺寸保存地形 RGBA、Z、原生 Z 與 A；每幀只重新繪製動態物件。原繪製前段、base/depth owner、尺寸、原生比例、精度、遮罩及 alpha 命令／素材必須一致才重用。遮罩在原生色盤變換之前生效，不能事後把舊顏色乘黑代替。
3. Live visibility 的 8192 頁快取只能容納 32MiB 頁 payload；完整 256 圖約需 10731 個打包 Cell 頁，捕獲中途反覆清空並重新配置。本輪改為有界 16384 頁並重用已配置節點，所有頁 bytes 每 epoch 仍重新讀取。初次 unordered_map 候選直接 hash 4096 對齊地址，碰到 libc++ 的冪次 bucket 產生嚴重碰撞，已捨棄；正式版 hash `page >> 12`。

每個 Live 實例的頁 payload 最多 64MiB，雜湊節點、bits 及指標陣列另計；多個相機分別持有自己的 cache，不能把單實例上限當成整個程序 RAM。CPU 地形前段只保存不超過 2M 面板 pixels 的平面，最多約 22MiB 平面 payload，另有最多 8192 筆 alpha 命令引用；這些數字是所持有／計費的 payload，並非工作集或程序 resident RAM。

## 完整大圖與 GPU 回退

完整 256×256 等角地圖含 130816 實體 Cell，超過先前 Live／GPU mask 的 65536-cell 准入。visibility 與 CPU 繪製命令改為有界 262144；mask 最多兩組 stamps，原有 8M coordinate/reference 與 64MiB packed metadata 預算保持。位移／長度計算以 64-bit 在轉為 GPU word offsets 前檢查。

不能直接提高實際 GPU 場景的 draw 准入。已核對基線 `shaders/vulkan_scene.comp`，所有分支會把 reference 的低 16 位作為 draw index，高 16 位另供只讀繪製區間使用。GPU 仍以 65536 為安全上限；更大場景執行完整 CPU 回放，沒有默默截斷索引，也沒有重編或修改 shader。

另一個真實回退耗時是：整圖 GPU mask 超過既有 512Mi 候選工作預算後，CPU `gpushroud::reference` 仍依 32px bin 反查，可掃描數十億候選。現在完整場景在規劃階段檢查相同預算，超額直接以逐 stamp 的 CPU Raster 建立最終面板遮罩；不先建立不可執行的 GPU 遮罩再重播其昂貴查詢。GPU 的工作與素材內存預算沒有放寬。

## 驗證及可重現資料

`tools/test_minimap_refresh_windows.py` 接受 Zig、2bb8215 基線標頭根目錄與輸出目錄。它建立來源 SHA 綁定，編譯 x86/x64 的新 fixture，並執行既有可見性發布、邊界、原生陰影、面板星級和 CPU 重用回歸。

- `assist/tests/radar_live_exploration.cpp`：真正 Renderer 的基線閃爍反例、獨立捕獲預算、短讀與多 epoch／移動單位對照。
- `assist/tests/radar_sampler_compact.cpp`：32640 格完整 bank 的內容、輪數與每輪主格讀取預算。
- `assist/tests/cpu_terrain_prefix.cpp`：10000 個靜態 TMP 呼叫加移動單位，600×300 面板、6000×3000 原始範圍；逐幀 RGBA/Z/nativeZ/A 對照，另測 alpha／比例／owner 失效。
- `assist/tests/radar_large_map.cpp`：130816 實體格、320×160 輸出、15360×7680 原始範圍的完整 CPU 呈現，要求 GPU 時的完整回退，以及短讀保留／恢復。檢查超 65536-cell 捕獲、metadata offsets 與既有字節預算。

小圖 visibility 計時直接比較 2bb 原始 Live。大圖原始 Live 會拒絕，另在測試副本只提高 cell-count 准入，保留其 8192-page/std::map 演算法後再比較頁快取成本；報告明示這項唯一測試修改，不把它當作原版本來就能完成的大圖基線。

最終 x64 對照：

| 固定工作 | 基線 | 本輪 | 取樣 |
| --- | ---: | ---: | --- |
| 10000 TMP＋移動單位，600×300 CPU 回放 | 2.684ms | 0.364ms | 三輪各 32 幀平均彙整；RGBA/Z/nativeZ/A 相等 |
| 128×128 圖 visibility 完整捕獲 | 3.076ms | 2.478ms | 8 次交錯暖態捕獲；原始 Live 基線 |
| 256×256 圖 visibility 完整捕獲 | 25.292ms | 10.512ms | 8 次交錯暖態捕獲；僅提高准入的基線副本 |
| 32640 實體格完整 bank 發布 | 64 輪 | 8 輪 | 每輪仍最多 4096 主格讀取；CPU 完整掃描約 5.4ms |

x86 的相同四組新 fixture 亦通過；CPU 地形回放三輪彙整約 3.563→0.482ms。大圖首次配置捕獲約 22ms，暖態仍需約 10.5ms；沒有把冷啟動成本、取樣成本或 GPU 工作隱藏在 FPS 倍數中。舊 `radar_boundary` fixture 在空素材 catalogue 時原先留下 index 0，與已存在的完整性檢查矛盾；本輪將其無素材欄位明設原生 -1，保留全部原有邊界斷言。

最終 [source-bound 結果與逐條命令／完整輸出](evidence/minimap-refresh-cpu-visibility.json) 記錄 13 組 fixture、22 個來源 SHA；全部通過。所有數值是受控 CPU／記憶體 fixture，不能相加為遊戲總提速，也不能代替實際畫面、硬體 GPU 或 RA2OL 雙機驗收。
