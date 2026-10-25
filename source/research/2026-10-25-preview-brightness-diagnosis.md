# 觀察窗建築偏暗：透明度排除與截圖逐像素對位

## 問題與證據範圍

使用者指出觀察窗建築比遊戲主畫面暗，詢問是否半透明或黑／白底合成造成。本輪先查研究 INDEX、pre.12 光照、RenderFix2、RenderFix3 與現行共用渲染程式；持久逆向筆記由主工作流程另行核對。

- 使用者原圖為 920×564 RGB，SHA-256：`f0580947799b630f14cbf5ba775ef95f35d8188a8bf2167f2a94b413ff972539`。原圖不納入 repository。
- 截圖沒有可辨識的 Toolbox 版本；不可因為目前已發布 pre.4，就把這張圖標成 pre.4。
- 截圖無法確定遊戲 EXE SHA。本輪配套原生函式研究使用的私有樣本 SHA 為 `7b8a068535d6af06845edf95ae829b113d00c02909330e16f197426cd7db94b6`；這是獨立的原生證據，不能冒充截圖程序的已確認身分。
- 本篇是**使用者截圖的量測與程式路徑審查**，不代表修復後 Windows 實景、所有 MOD 或多人驗收。

## 透明度與二次乘光

現行 `external_reader/source/camera_assets.h::renderActor` 的 SHP 與 VXL 解碼明確傳入 alpha 255；`voxel_preview.h::sprite` 對非零 SHP 索引使用 `premultiply(pal[color], 1, alpha)`。索引 0 保留透明，實心像素沒有白／黑底墊色。

`assist/source/watch_engine.h::refreshAssets` 與 `external_reader/source/radar_scene.h::render` 又以 `camera::opaque` 保證非透明色彩像素為 alpha 255。`camera_raster.h::Surface::blit` 對 alpha 255 直接覆寫背景；實心建築像素不會混入畫布的深色底。

最終呈現的 `watch_engine::drawView`、`watch_host::drawView` 都是 PARGB Bitmap → `DrawImage(..., nullptr)`，沒有第二個 RGB/alpha ColorMatrix。`main.cpp::overlayworker::present` 的 `UpdateLayeredWindow` 使用 SourceConstantAlpha 255。實景小地圖走 `SetDIBitsToDevice`，其視窗常數 alpha 同樣是 255。alpha 124 是 `formation_host.h` 呼叫 `camera::previewPixels` 的編隊預覽用途。

這些路徑在本地上一個原始碼基準 `245faa7`（selected source `4485b0a0e3508bba0191049ba6d0dc3df5913792`）亦已存在；不是 pre.4 才剛改成不透明。

目前 `camera_raster.h::Light` 取 cell intensity 或 ambient fallback **擇一**，再乘 tint；沒有把 Ambient/Ground/Level 完整疊乘第二遍的程式路徑。`scene_lighting.h` 裡舊的「GDI+ 對角矩陣」註解不是現行呼叫證據，實際呈現沒有使用該矩陣。

## 可重現的截圖對位

腳本：`assist/tests/preview_brightness_analysis.py`。只讀取私有原圖並輸出 sanitized JSON，不複製或輸出圖片；依賴 Pillow、NumPy、SciPy。

```sh
python source/assist/tests/preview_brightness_analysis.py PRIVATE_IMAGE.png --output PRIVATE_RESULT.json
```

這是針對上述 920×564 構圖的研究工具，不是任意截圖的通用自動對位器。固定四個上窗建築區塊與一個下窗工廠區塊，在合理的整數平移範圍內搜尋 2:1 取樣。比較灰階減去 Gaussian sigma=1.3 模糊後的高頻結構，採 Pearson correlation；要求最佳相關 ≥0.90、勝過次佳至少 0.15，四個上窗區塊還必須同時得到相同平移，才輸出亮度統計。如此不以「看起來像」的粗略裁切比較推斷暗色原因。

座標均為原圖左上角起算、0-based。上窗四棟一致得到：

`native_x = 2 * watch_x + 282`，`native_y = 2 * watch_y - 61`。

| 區塊 | 最佳高頻相關 | 次佳相關 |
| --- | ---: | ---: |
| 電廠 | 0.960601 | 0.500432 |
| 雷達 | 0.937532 | 0.557884 |
| 戰車工廠 | 0.950646 | 0.580306 |
| 兵營 | 0.974075 | 0.506958 |

下窗工廠得到 `native_x = 2 * watch_x + 432`、`native_y = 2 * watch_y - 379`，最佳相關 0.987471，次佳 0.585610。上下小窗的同一工廠區塊 5600 像素中，有 5556 像素（99.21%）RGB 完全一致。

## 實際是通道不同程度變暗，並帶有藍色色調

只依**主畫面原生像素**選取灰白高亮：`min(R,G,B) >= 160` 且 `max(R,G,B)-min(R,G,B) <= 8`。不根據觀察窗的色調篩選。四個區塊共有 1482 個採樣點；去除區塊重疊後為 1429 個獨立像素。去重前後各通道比值的中位數相同：

| 通道 | 觀察窗 / 主畫面中位數 |
| --- | ---: |
| R | 0.787879 |
| G | 0.816143 |
| B | 0.974747 |

反覆出現的灰白色不是單純少取到亮點，而是**同一個已對位像素**被改色：

| 主畫面 RGB | 觀察窗 RGB | 例子：主畫面 → 上窗座標 |
| --- | --- | --- |
| (222, 223, 222) | (175, 182, 217) | (650,309) → (184,185)，工廠 |
| (222, 223, 222) | (175, 182, 217) | (444,329) → (81,195)，兵營 |
| (214, 215, 214) | (170, 176, 210) | (606,295) → (162,178)，工廠 |

同構圖裡雪地常見 `(156,170,214) → (153,168,207)`；原生紅色圓頂常見 `(222,0,0) → (175,0,0)`。建築 R/G 壓低得明顯比 B 多，符合偏藍色調，不符合所有通道一律乘 0.5 或 0.7 的說法。

## 原因判斷與未決項

1. **排除整體半透明、黑／白底滲入實心像素。** 上述程式路徑及截圖的不同通道比值都不支持這個解釋。
2. **排除僅因縮小少取到高亮點。** 0.5 倍最近取樣確實可能改變觀感，但這次已找到同一 source pixel；其 RGB 仍被改色。
3. **物件套用地格 tint 是與數據吻合的具體程式缺陷。** 修復前 `lighting::at` → `cell(..., localTint=true)` 一律從 `CellClass +0x34` 取 LightConvert RGB，覆蓋物件 tint。配套原生研究證明普通 SHP 經 `0x705F7D` → vtable `+0x1E4` → `0x705D70` 選 type `+0xDF0`／owner ColorScheme converter；VXL `0x70718F..0x707194` 同樣選物件 converter。只有建築 type `+0x1702` 的 TerrainPalette 分支在 `0x705EC1..0x705F59` 改選 cell converter。截圖呈現的「雪地接近一致、建築灰白變藍」與這個來源錯配吻合。但截圖沒有附同時刻 converter 記憶體，不能據圖反推出精確 tint 欄位值。
4. **63 階與原生 53 階是另外的忠實度差異。** 本輪原生樣本的 ColorScheme LightConvert `+0x16C=53`，現行 `camera::Light` 固定 63/31。原生 ramp 本身沒有額外神秘提亮；直接換 LUT 不能先被宣稱會修好所有偏暗。這個差異不會單獨解釋本圖不同 RGB 通道的系統性倍率。
5. **陰影與其他顏色效果仍需各自驗證。** `Surface::blit` 的 shadow 會真的把底下 RGB 除 2，錯誤深度會影響局部像素；本圖的穩定偏藍不等於已排除所有局部陰影問題。兩個小窗畫面高度一致，也不能替代修復後實景驗收。

後續修改應修正物件／地形 converter 選擇，在帶有明顯色調的受控資料與實際雪地場景檢查，保留既有不透明建築、編隊預覽透明度和 VXL 方向。不可用全局任意加亮去掩蓋不同光照來源。

本輪完整數值摘要另存 `source/verification/preview-brightness-analysis.json`，不含原圖或像素陣列。
