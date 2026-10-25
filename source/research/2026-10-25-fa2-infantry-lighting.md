# FA2 HDM 光照核對與原版步兵額外亮度

## 問題與調查前依據

使用者回報觀察窗的步兵、戰車仍偏暗，要求參考 FA2 HDM Edition 的光照沙盒。本輪先讀研究 INDEX、持久 SQLite 中的 pre.6 VPL／ExtraUnitLight 筆記、pre.5 物件 LightConvert 筆記，再查 FA2 與支援映像。pre.6 已明確留下 Infantry 類尚未驗證；不能把已修的 Unit／Aircraft 欄位當作步兵已覆蓋。

支援 EXE SHA-256：`7B8A068535D6AF06845EDF95AE829B113D00C02909330E16F197426CD7DB94B6`。本輪只讀私人 capture `RA2Master-Capture-20261025-003340-3568`，不把 EXE、記憶體、模型、色盤或 framebuffer 加入倉庫。

## FA2 HDM 參考所得

固定參考 [handama/FA2sp](https://github.com/handama/FA2sp/tree/f2a461f1788b0ce0667a58ca69f1a931d8be946c)。

- [Palettes.cpp 316–360](https://github.com/handama/FA2sp/blob/f2a461f1788b0ce0667a58ca69f1a931d8be946c/FA2sp/Miscs/Palettes.cpp#L316)：物件環境亮度為 `Ambient + LampAmbient - Ground + Level*Height`，再按 Unit／Infantry／Aircraft 分別加 `ExtraUnitLight`／`ExtraInfantryLight`／`ExtraAircraftLight`。普通光照預設不把地圖 RGB 色調乘到可重著色單位上；局部燈的 RGB 也不直接染單位。
- [CMapData/Body.cpp 6243](https://github.com/handama/FA2sp/blob/f2a461f1788b0ce0667a58ca69f1a931d8be946c/FA2sp/Ext/CMapData/Body.cpp#L6243)：三個額外亮度分別讀合併後 RulesMap 的 `[AudioVisual]`，預設均為 0.2。這是 FA2 的 INI 形式，不能據此把遊戲 runtime 設定讀成 float。
- [VoxelDrawer.cpp 8](https://github.com/handama/FA2sp/blob/f2a461f1788b0ce0667a58ca69f1a931d8be946c/FA2sp/Miscs/VoxelDrawer.cpp#L8) 將可設定光向交給 CncVxlRenderText；預設為 `(0.05,1,0.2)`，見 [FA2sp.cpp 540](https://github.com/handama/FA2sp/blob/f2a461f1788b0ce0667a58ca69f1a931d8be946c/FA2sp/FA2sp.cpp#L540)。對應 [handama/CncVxlRenderText shadow 分支](https://github.com/handama/CncVxlRenderText/blob/755dbb6e9bf751d7c3c4c1c2dc07ac8bb094d1d0/src/vxlfile.cpp#L413) 用變換後法線與光向的夾角選 VPL 頁，再映射 palette index。
- 我們的光向已有 pre.4 原版受控驗證，這次沒有以 FA2 預設覆蓋它。pre.6 也已確認有效 VPL 路徑不會再乘近似 Lambert，不重複加一層泛用增益。

## 新原版結論：ExtraInfantryLight 是 signed integer

| 地址／欄位 | 原版證據 |
| --- | --- |
| `[0x8871E0]` | 當前 RulesClass 指標 |
| Rules `+0x17D8` | 步兵獨立的 ExtraInfantryLight，signed 32-bit，capture 值為 200 |
| `0x83A23C` | `ExtraInfantryLight` key 字串 |
| `0x66B6EF` | ReadDouble 呼叫 |
| `0x66B6F4..0x66B705` | 乘 `[0x7E4658]` 的 double 1000、`_ftol`、把 EAX 寫入 Rules `+0x17D8` |
| `0x51933B..0x51944D` | Infantry::Draw 原版普通／高空分支，普通地面由 MapClass 查詢 cell object intensity `+0x10A` |
| `0x519457` | `add esi,[eax+0x17D8]`，加到先前亮度，不是乘法或 IEEE float |
| `0x5195B4` | 把 ESI 亮度入棧，後續 `0x5195E2` 呼叫 virtual `+0x50C` 繪製 |

新的 probe 替身 ReadDouble 返回值，在 x87 放入指定 double，之後執行原乘法、`_ftol` 與寫入。每份 capture 各測 `0.2、0、-0.2、0.75、-2、4`，得到 `200、0、-200、750、-2000、4000`；三份共 18 例。

對真實捕獲步兵執行其原 virtual `+0x1C0` 取得 cell 座標，再執行原 `0x51933B..0x51945D`，沒有替換普通 MapClass 查詢。每個物件交叉六個正／負／零 extra，接著執行原 `0x51958A..0x5195B5`，在實際亮度 push 後停止。每例同時輸入等同捕獲欄位給 production `lighting::object`：原版 raw 值為 `cell object + extra`，production 在既有 0..2000 呈現範圍內一致。三份共 594 例。

首份 `01-off` 的 26 個普通地面步兵中，object intensity=1056 者原版送入1256，object intensity=1168 者送入1368。舊 helper 的 kind=1 從未讀額外亮度，確實少了200。

## 修復與驗證

`scene_lighting.h` 的三類機動物件現在分別讀 `17D4`／`17D8`／`17DC`，保留前後 Rules 指標一致、讀取失敗回退、64-bit 相加及負值歸零。沒有硬編碼200，也沒有再次縮放1000。

- portable `scene_lighting.cpp` 先新增 Infantry assertion，在舊實作失敗；修復後通過。涵蓋三欄位不同值、負／零／極值、最終普通像素與fullbright索引、欄位失讀、換局指標變動、越界指標，以及建築／動畫／礦柱隔離。
- 新 [native_infantry_lighting_capture.py](../assist/tests/native_infantry_lighting_capture.py) 與 [數值結果](../verification/native-infantry-lighting-capture.json) 保存上述18／594例。
- 重跑既有 `native_voxel_vpl_capture.py`，4個穩定VPL快照、3個loader、4,800個fresh/cached scalar像素、12組body raster、18組Unit亮度／36組SHP-VXL dispatch／18組Aircraft加法均通過。
- 重跑既有 `native_lightconvert_capture.py`，63次原版LUT重建、1,440像素、195,840 selector查表、54 source選擇、24,576 VPL index查表通過。

## 排除項與未決點

這輪沒有從舊 capture 找到坦克新增的固定壓暗項；有效VPL下12組body raster使用相同raw palette的luma差仍介於-0.059與+2.707，沒有一致的大幅壓暗。這不是當前使用者畫面的逐像素驗收，不能推斷其畫面已修好。

新 Infantry fixture 明確限於普通地面；跳過承載／無有效地圖位置、橋／特殊結構格與airborne狀態。高空高度、橋上、偽裝、閃光、ABuffer和完整場景縮小取樣仍有既存驗證邊界，不把獨立加法證據寫成所有特殊狀態已逐位元復刻。原版受控執行不等於 Windows 實機顯示或聯機測試。

```sh
g++ -std=c++17 -O2 source/assist/tests/scene_lighting.cpp -o /tmp/scene-lighting
/tmp/scene-lighting
PYTHONPATH=/path/to/native-deps:source/assist/tests \
  python3 source/assist/tests/native_infantry_lighting_capture.py \
  /path/to/private-capture --output source/verification/native-infantry-lighting-capture.json
```
