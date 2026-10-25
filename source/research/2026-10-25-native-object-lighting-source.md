# 建築／戰車偏暗：物件與地形 LightConvert 來源分離

## 問題與證據範圍

使用者回報上一版戰車受光方向已正確，但建築與戰車仍比遊戲畫面暗，並詢問是否由透明度、黑底或白底造成。本次先核對原版色階與實際 converter 選擇，再修正已找到的來源錯配。沒有改相機投影、voxel 光源方向或全局亮度增益。

支援映像 SHA-256：

`7B8A068535D6AF06845EDF95AE829B113D00C02909330E16F197426CD7DB94B6`

映像基址 `0x400000`。只讀樣本為私人 capture `RA2Master-Capture-20261025-003340-3568` 的 `01-off`、`02-on`、`03-off`。公開測試與 JSON 不包含原 EXE、dump、完整 LUT、VPL、SHP 或截圖。

調查前已讀研究 INDEX、pre.12 光照筆記、RenderFix2 原版動畫／光照筆記及 pre.4 voxel 光源筆記；持久研究 notebook 的本輪檢索由主代理完成。以下區分原指令受控執行、capture 數值和使用者附圖分析。

## 結論

普通 SHP 物件和 VXL 的 RGB 色調取自**物件所屬的 ColorScheme／CustomPalette converter**。只有明確使用 TerrainPalette／UseCellLightConvert 的路徑才取 `Cell+0x34`。舊 `lighting::at` 在所有物件上都把 cell converter 的 RGB 覆蓋到 scene light，因此雪地地形的偏藍 tint 也被套到普通建築和戰車，會壓低紅、綠通道。

使用者附圖的[獨立對位分析](2026-10-25-preview-brightness-diagnosis.md)也呈現通道差異：例如遊戲灰白像素 `(214,215,214)` 對應小窗 `(170,176,210)`，而雪地本身接近；完整數值見[截圖差分摘要](../verification/preview-brightness-analysis.json)。這與 converter 來源錯配相符，並不符合單一透明度係數。附圖不是本次舊 capture 的同一畫面；不能聲稱舊 capture 本身重現了該藍色 tint。

色階數確有另一個既存近似：這批 unit ColorScheme 是 **53 級、中心第 26 級**，目前 `camera::Light` 使用 **63 級、中心第 31 級**。完整原版 LUT 與實際單像素 blitter 的量化證明，這個差異不足以解釋附圖的大幅紅／綠衰減。本輪保留 brightness 計算，修正 RGB 來源，不添加補償增益。

截圖對位方法與數值另見 [亮度診斷](2026-10-25-preview-brightness-diagnosis.md) 及 `verification/preview-brightness-analysis.json`。

## 原版來源選擇：地址與規則

| 地址／欄位 | 已核實行為 |
|---|---|
| `0x705D70`，thiscall，ECX=Techno，回傳 EAX | `GetRemapColour`：`Owner+0x16054` 為 scheme index；`Type+0xDF0` 非空時用該 CustomPalette vector，否則用全局 `0xB054D0`；回傳 scheme `+0x30C` |
| `0x705F7D..0x705F89` | 普通 SHP `DrawObject` 呼叫 virtual `+0x1E4`，即上述物件 converter |
| `0x705EC1..0x705F59` | 建築 `Type+0x1702` TerrainPalette 為真時，改選 cell `+0x34`，並以 cell `+0x10C` 覆蓋傳入亮度；此時先前傳入的 ExtraLight 不再生效 |
| `0x70718F..0x70719C` | VXL 最終繪圖也呼叫同一個 `GetRemapColour`；這條路徑不看建築 TerrainPalette |
| `0x423114..0x423358` | Anim draw 的 converter／brightness 選擇，詳見下表 |
| `Anim+0xD4` | 已指派的 LightConvert；建築動畫通常由母建築設定 |
| `Anim+0xFC` | 上述 draw 分支實際讀取的 brightness；pinned YRpp 中欄位名為 TintColor，本筆記按實際指令用途解讀 |
| `Anim+0x196` | UseCellLightConvert |
| `AnimType+0x355` | IsVeins 特殊調色盤分支 |
| `AnimType+0x35D` | UseNormalLight：把 brightness 固定為 1000，**保留已選 converter 的 RGB** |
| `AnimType+0x361` | AltPalette；未指派 converter 時選全局 scheme 0 |
| `LightConvert+0x188` | 原始 RGB palette 指標；捕獲的 owner schemes 均指向 scheme `+4` |

原版動畫分支的優先序如下。這是原函式的分支順序，不能把「屬於建築的 slot」一概當作 cell-light 動畫。

| 條件 | Converter | UseNormalLight=false 時的亮度 |
|---|---|---|
| IsVeins | 當前玩家的全局 ColorScheme | cell terrain intensity `+0x10C` |
| UseCellLightConvert | cell `+0x34` | cell terrain intensity `+0x10C` |
| Anim `+0xD4` 非空 | Anim 已指派的 converter | Anim `+0xFC` |
| AltPalette | 全局 scheme 0 | cell object intensity `+0x10A` |
| 其餘 | 全局 ANIM Convert `[0x87F6C0]` | cell object intensity `+0x10A` |

UseNormalLight 在每個分支都只把亮度改成 1000。單色階 converter 在實際 blitter 中忽略 brightness 選階；外部 helper 對這種動畫以 1000 表達其結果。

`ANIM Convert` 是 `ConvertClass`，不是 `LightConvertClass`。本樣本前者 vtable 為 `0x7E5358`，後者為 `0x7ED0A4`。普通 Convert 沒有 LightConvert 的 `+0x188` raw palette 及後續 RGB 欄位；盲讀會讀到相鄰配置。本次動畫 raw palette 採樣只接受已核實的 LightConvert vtable，普通 ANIM 仍使用其 ANIM.PAL 來源。

## 完整 LUT 與 blitter 量化

`0x556090 LightConvert::UpdateColors` 為 thiscall，ECX=converter，stack 為 red、green、blue、tinted，`ret 0x10`。此次沒有在寫入 RGB 欄位後提前截停，而是把每張目的 LUT 先填成 `0xA5`，再執行完整原版函式。

| 欄位／地址 | 用途／捕獲值 |
|---|---|
| Converter `+4` | BytesPerPixel=2 |
| `+0x16C` | ShadeCount=53 |
| `+0x170` | FullColorData，53×256 個 RGB565 words |
| `+0x174` | PaletteData=FullColorData+26×512 |
| `+0x190` | 忽略一般色調的 index mask；此樣本為 240..254 |
| `+0x198..0x1A0` | 普通 RGB，1000 為 1.0 |
| `+0x1A4..0x1AC`、`+0x1B0` | 暫時 RGB 及啟用旗標 |
| `0x829D20` | 原版 pixel format 分支；此樣本為 2 |
| `0x8A0DD0..0x8A0DE4` | 實際色位移／精度，組成 RGB565 |
| `[0x822D80]` | x87 control word `0x0E7F`，朝零截斷 |

普通色階是固定點 RGB 乘法、飽和限幅及 16-bit 色彩量化；未找到會讓普通物件整體增亮的隱藏加成。受忽略 mask 影響的自發光索引在最暗幾級仍有原版淡出，不能把外部永遠保留 240..254 的近似視為所有亮度下逐像素相同。

`0x490E50 SelectRLEBlitter` 在受控正常旗標 `0x6E00` 選到 vtable `0x7E53A0`、繪圖函式 `0x4990E0`。其 converter RGB 查表過程為：

```text
shade = min(254, (261 * max(0, brightness)) >> 11)
row = min(ShadeCount-1,
          alphaBuffer * shade * (ShadeCount-1) / 32258)
pixel565 = FullColorData[row * 256 + paletteIndex]
```

原版中間表保存 `row<<8`，再 OR palette index。中性 ABuffer 值為 127。三份捕獲的 ABuffer 數值介於 2..127，沒有高於 127 的額外提亮；其內容不是視窗的 alpha channel。

以第一張 LightGold scheme 的 255 個非透明索引等權比較，RGB565 使用標準位元複製展開成 RGB8，差值定義為「目前 renderer − 原版」：

| Capture 中實際 object brightness | Native row | 目前 63 級 level | 平均 luma 差，0..255 | 單 channel 最小／最大差 |
|---:|---:|---:|---:|---:|
| 1056 | 27 | 32 | -0.312 | -9 / +7 |
| 1140 | 29 | 35 | +1.146 | -6 / +7 |
| 1168 | 30 | 36 | +0.621 | -6 / +7 |

這是 palette 等權數值比較，不是 SHP 像素頻率加權，也不是使用者附圖的像素重播。中性例子：raw `(120,120,120)`、brightness=1000，原版 word 為 29614，展開為 `(115,117,115)`；目前為 `(120,120,120)`。因此不能以「53 級應改成 63 級」或任意全局 gain 來解釋／修正這次偏藍、偏暗。

## VXL 與 SHP 的關係

原版 `0x7576B9..0x7576F8` 先用 normal index 讀 `0xB45990` 的 page，再以 `page*256+原始色號` 查 `0xB41178` 的 VPL，將結果寫入暫存 indexed image。捕獲的 VPL page count 為 32。這一步沒有額外 RGB 乘法；後續才由物件所選 converter 完成最終套色。

此次對全部 32×256 個 page/color 組合執行原始 lookup slice；每份 capture 的 8,192 例都吻合。這不是重新驗證 voxel 幾何或相機方向。方向已由上一輪原版數值驗證與使用者實機回饋確認，本輪沒有變動。

## 實作

- `scene_lighting.h` 新增 `objectConverter`、`object` 及 `animationSource`／`animation`。普通物件保留 cell 已算好的 brightness，但從 owner/custom scheme 取得 RGB。TerrainPalette 保留原版例外，並避免再加一次 ExtraLight。
- 動畫依原版優先序取 converter、palette 與 brightness。UseNormalLight 不再用空 `State{}` 清掉 tint。
- `camera_assets.h` 的動畫 palette 優先讀實際 LightConvert `UsedPalette1`，讓 raw RGB 和最後的 tint 使用同一來源。讀不到時保留檔案／父建築回退。普通 ANIM Convert 不會被當成 LightConvert。
- 建築 runtime TerrainPalette 直接選 ISO palette；不以 owner/custom palette 蓋過這個原版選擇。
- 觀察窗、實景小地圖與隊形預覽的呼叫點由主代理改用共同 helper。Tint 在採樣／合成時讀取，未烘入已快取的姿態像素，也未引入持久 LUT cache。converter 與 vector 讀取有範圍及一致性檢查。

相機、voxel diffuse/specular 選頁、陰影係數、一般窗體 alpha、隊形預覽 opacity、現有 brightness 量化與素材內烘焙的 SHP 明暗都保持原邏輯。

## 驗證與可重現命令

- [native_lightconvert_capture.py](../assist/tests/native_lightconvert_capture.py)
- [native-lightconvert-capture.json](../verification/native-lightconvert-capture.json)
- [scene_lighting.cpp](../assist/tests/scene_lighting.cpp)
- [game_assets_host_test.py](../assist/tests/game_assets_host_test.py)

| 驗證 | 結果 |
|---|---:|
| 完整 LUT 先清空再由原版重建 | 63 張，每張 13,568 words，全部逐位元一致 |
| 原版 brightness×ABuffer selector | 195,840 例，零差異 |
| 真正執行原版 RLE blitter 的單像素案例 | 1,440 例，全部吻合 LUT |
| 原版 source 選擇分支：SHP/VXL/custom/terrain/五種動畫×NormalLight | 54 例，全部通過 |
| 原版 VPL page/color lookup | 24,576 例，全部通過 |
| 實際有效建築／車輛的 owner/custom converter 綁定 | 159 筆，全部吻合 |
| portable scene lighting 專項 | 通過 |
| production asset host 專項 | 通過 |

```sh
PYTHONPATH=/path/to/native-deps:source/assist/tests \
  python3 source/assist/tests/native_lightconvert_capture.py \
  /path/to/private-capture --output /path/to/result.json

g++ -std=c++17 -O2 source/assist/tests/scene_lighting.cpp -o /tmp/scene-lighting
/tmp/scene-lighting
python3 source/assist/tests/game_assets_host_test.py
```

Private capture 可為解壓 session 目錄或 ZIP。依賴 Unicorn，沒有呼叫遊戲 process、磁碟 loader、Win32 顯示或更新迴圈。JSON 保存各階段 code-range SHA-256、受控條件、case counters、類型／數值與差分摘要。

Portable regression 明確設置「cell 藍色、owner 中性」；同一個不透明灰白像素在黑底與白底上得到完全相同輸出，並驗證 object 不再染上地形 tint。另涵蓋 custom vector、當前／暫時 RGB 切換、橋上 brightness、TerrainPalette、動畫 assigned/cell/default/AltPalette/IsVeins，以及 UseNormalLight 保留 tint。

Host fixture 用不同的原始 RGB palette 驗證 Anim `+D4` 和 UseCellLightConvert 的 palette 來源，並驗證建築 runtime TerrainPalette。此測試只合成 Windows 檔案／記憶體邊界，不能替代使用者新版本的實機畫面確認。完整 portable／Windows build gates 由主代理另行執行。

## 限制與已排除的解釋

1. 正常觀察窗／實景小地圖的實心物件像素 alpha=255，合成時直接覆蓋目的色；黑底或白底不會透過實心像素改變顏色。素材透明孔、陰影與刻意半透明的隊形預覽各有自己的用途。
2. 舊 capture 的 owner schemes RGB 均為中性，沒有重現附圖中的雪地 cell tint。根因由原版分支／目前程式對照及附圖通道差分共同支持，不能把它寫成同場景全流程 native 截圖重播。
3. 完整 LUT 測試排除了「原版另有未處理的普遍大幅增亮」這個候選；53/63 級、RGB565 展開和最暗 emissive 索引仍有少量既存近似，本輪沒有任意一併調整。
4. 本輪沒有改動已確認正確的受光方向，也未驗證／調整使用者顯示驅動的 gamma 或遊戲外包裝器。

## 公開欄位參考

主要判斷以支援映像的原始指令及 controlled execution 為準；以下 pinned YRpp 只用來對照欄位名稱與類別結構：

- [ColorScheme.h](https://github.com/Phobos-developers/YRpp/blob/d9795546950fcc75a94325ffc83b44d16ec34ce2/ColorScheme.h)
- [ConvertClass.h](https://github.com/Phobos-developers/YRpp/blob/d9795546950fcc75a94325ffc83b44d16ec34ce2/ConvertClass.h)
- [AnimClass.h](https://github.com/Phobos-developers/YRpp/blob/d9795546950fcc75a94325ffc83b44d16ec34ce2/AnimClass.h)
- [AnimTypeClass.h](https://github.com/Phobos-developers/YRpp/blob/d9795546950fcc75a94325ffc83b44d16ec34ce2/AnimTypeClass.h)
- [ObjectTypeClass.h](https://github.com/Phobos-developers/YRpp/blob/d9795546950fcc75a94325ffc83b44d16ec34ce2/ObjectTypeClass.h)
