# 礦柱：SpawnsTiberium 的原版色盤、亮度與垂直位置

## 問題與證據邊界

使用者指出礦柱的色盤不正確，並特別提醒 FA2 本身也可能畫錯，不能以與編輯器相同作為正確證據。本次直接檢查支援映像的 Terrain 繪圖指令及初始化流程。

- 支援 EXE SHA-256：`7b8a068535d6af06845edf95ae829b113d00c02909330e16f197426cd7db94b6`，映像基址 `0x400000`。
- 私人 capture：`RA2Master-Capture-20261025-003340-3568` 的 `01-off`、`02-on`、`03-off`。原 EXE、原始記憶體、SHP 和 palette 不進入公開儲存庫。
- 調查前已檢索 `INDEX.md`、既有資產／光照筆記及持久 SQLite v40。使用者本次新截圖與舊 capture 並非同時刻，沒有把 capture 的物件位置或類型編號直接套到截圖。
- 新證據包括原始指令反彙編、原始選擇指令的受控執行、實際 capture 的類型／converter 綁定，以及生產資產載入程式配合檔案與記憶體替身的回歸。沒有執行完整遊戲繪圖、Windows 合成或聯機更新。

## 原因：礦柱不是一般樹木的 ISO 路徑

原程式將所有 `kind == 4` 的 TerrainClass 物件視為一般樹木：根據素材副檔名或地圖 theater 選 ISO 色盤，並使用 cell 的 terrain brightness 和 RGB tint。這對礦柱不成立。

三份 capture 均有 18 個礦柱：`TIBTRE01` 5 個、`TIBTRE02` 8 個、`TIBTRE03` 5 個。這三種 TerrainType 的 `SpawnsTiberium +0x2B1` 和 `IsAnimated +0x2B3` 都為 1，SHP 各有 22 幀。生產修復以 **runtime SpawnsTiberium 旗標** 判斷，沒有硬編碼 TIBTRE 名稱。

| 原版分支 | 色盤／converter | 亮度 | 垂直位置 |
|---|---|---|---|
| `TerrainType +0x2B1 != 0` | 全局 `[0x87F6BC]` resource converter | cell 有符號 16 位 `+0x10A` | 傳入繪圖點 Y 再減 16 像素 |
| 一般 Terrain | cell `+0x34` ISO LightConvert | cell 有符號 16 位 `+0x10C` | 不加礦柱的 -16 偏移 |

礦柱這個分支讀取的是普通 object intensity，沒有改用橋層 `+0x10E`。因此不能只換貼圖色盤而繼續套入 ISO converter 的藍色色調。

### 地址與 ABI

- `TerrainClass` 全局 vector：`0xA8E988`；物件 vtable `0x7F522C`；Type 指標 `+0xC8`，動畫 stage `+0xAC`。
- `TerrainTypeClass` vtable `0x7F5458`；`SpawnsTiberium +0x2B1`、`IsAnimated +0x2B3`。
- 原版繪圖入口 `0x71C1B0` 和另一繪圖入口 `0x71C360` 均以 ECX 傳 Terrain 物件，結尾 `ret 8`。本次只執行其中來源選擇區段，沒有執行整個繪圖函式。
- `0x71C280..0x71C2BC`：測試旗標後，礦柱在 `0x71C294` 選全局 converter，`0x71C29A` 讀 `+0x10A`，`0x71C2A9` 減 Y 16。普通分支在 `0x71C2B2` 選 cell converter、`0x71C2B5` 讀 `+0x10C`。
- `0x71C40F..0x71C44B`：另一繪圖入口有相同選擇；礦柱偏移位於 `0x71C438`。

## 全局礦柱 converter 的來源

三份實際 capture 的 `[0x87F6BC]` 均與 `[0x87F6B4]` 指向同一個 converter，vtable 為 `0x7E5358`、BytesPerPixel 為 2、ShadeCount 為 53。這是 **plain ConvertClass**，不是 LightConvertClass；不可把其物件尾端之後的 `+0x188` 當成 raw palette 指標。

初始化區段 `0x52BFCE..0x52C082` 以 `0x5B40B0` 載入 `UNITSNO.PAL`，把 768 個來源通道左移兩位，呼叫 `ConvertClass` 建構入口 `0x48E740`，其參數為展開後色盤、`0x885780` 的八位模式基準色盤、surface、53 階及 false。結果存入 `[0x87F6B4]`。`0x52C1EA..0x52C200` 再將此指標複製至 `[0x87F6BC]`。

**capture 的 theater 為 0，該初始化來源仍是 UNITSNO.PAL。** 它不是由礦柱素材的 `.TEM`／`.SNO` 副檔名選出的 ISO 色盤，也不是礦石／寶石 overlay 的 TEMPERAT 分支。修復優先讀原版實際使用的 converter，不憑檔名猜測；只有 live palette 無法使用時，才回退這段原始初始化已確認的檔案來源。

本次執行了原始檔名選擇、768 通道展開和 converter alias 指令；檔案載入、配置記憶體、最後 converter 建構為受控邊界。測試以合成 palette 核對傳入建構器的全部 768 個通道，不是把私人 palette 複製到結果檔。

## 實作

- `scene_lighting.h` 新增 `spawnsTiberium` 與 `terrainObject`。一般樹保留 ISO brightness／tint；礦柱使用 cell `+0x10A`，plain Convert 的 tint 為中性。若實際 converter 為已支援的 LightConvert，則讀取它自己的 tint；一階 converter 維持正常亮度。
- `camera_assets.h` 的 Terrain 資產路徑對 SpawnsTiberium 優先讀 `[0x87F6BC]`。LightConvert 可使用既有 raw-palette 讀取；plain Convert 僅讀它確實擁有的 `+0x174` 中心 RGB565 表，檢查 vtable、BytesPerPixel、階數、地址範圍與指標一致性。失敗才回退 `UNITSNO.PAL`。
- 對成功解碼的礦柱 image 加原版的 Y=-16 偏移。這在共用資產層處理，小窗與實景小地圖得到同一個結果。
- 小窗／實景小地圖的 kind 4 光照呼叫改用 `terrainObject`。沒有調整攝像機角度、一般樹木的素材座標、全局增益或視窗透明度。

## 驗證

公開檔案：

- `assist/tests/native_terrain_palette_capture.py`
- `verification/native-terrain-palette-capture.json`
- `assist/tests/game_assets_host_test.py`
- `assist/tests/scene_lighting.cpp`

| 檢查 | 結果 |
|---|---|
| 原版兩個繪圖分支，3 份 capture，flag 0／1／2、不同 object／terrain 亮度及正負 Y | 144 例，converter／亮度／位置零差異 |
| 原版初始化檔名、合成 palette 展開、53 階建構參數、UNIT→resource alias | 3 組全部通過；共 2304 通道逐一相等 |
| 生產資產管線：非 TIBTRE 名稱但 SpawnsTiberium 為真、雪地／溫帶、ISO 樹木對照、-16 偏移、失效表回退 | 通過 |
| 同一份最終資產回歸對 pre.5 未修改的 camera_assets.h | 在「礦柱必須用 resource converter」顏色斷言失敗；修復後通過 |
| 光照 helper：object／terrain／bridge 三個亮度不同、plain converter 尾端偽造 RGB、有效零亮度、普通樹、LightConvert 及一階特殊表 | 通過 |

```sh
PYTHONPATH=/path/to/native-deps:source/assist/tests \
  python3 source/assist/tests/native_terrain_palette_capture.py \
  /path/to/private-capture --output /path/to/result.json
python3 source/assist/tests/game_assets_host_test.py
g++ -std=c++17 -O2 source/assist/tests/scene_lighting.cpp -o /tmp/scene-lighting
/tmp/scene-lighting
```

原碼範圍 SHA-256 及各階段觀察值保存在公開 JSON；完整 palette、SHP、framebuffer 和私人映像不包含在其中。

## 排除與未決事項

1. 與 FA2 顯示相同不能證明正確；本次以原版來源選擇及初始化指令為依據。
2. 不是普通樹木都要改成 unit palette，也不是所有 Terrain 都要去掉 cell tint；旗標為零的原版分支保持原行為。
3. 礦柱與地面的 ore／gem overlay 是不同物件與繪圖分支，本次沒有以礦柱的規則覆蓋所有礦石貼圖。
4. 中心 RGB565 表再套用既有合成器的亮度仍是近似。原版 53 階與目前 63 階、RGB565 展開和既有中間像素量化的少量差異沒有在此修復中全面替換，不能宣稱逐像素等同原引擎。
5. 沒有完整新版實機截圖驗收，亦未宣稱所有 MOD 自訂繪圖 hook 都已覆蓋；原版旗標與實際 converter 是目前支援的依據。

## 一手欄位名稱參考

實際行為以以上原指令為準；欄位名稱另對照固定提交的 YRpp：

- [TerrainTypeClass.h](https://github.com/Phobos-developers/YRpp/blob/d9795546950fcc75a94325ffc83b44d16ec34ce2/TerrainTypeClass.h)
- [TerrainClass.h](https://github.com/Phobos-developers/YRpp/blob/d9795546950fcc75a94325ffc83b44d16ec34ce2/TerrainClass.h)
- [FileSystem.h](https://github.com/Phobos-developers/YRpp/blob/d9795546950fcc75a94325ffc83b44d16ec34ce2/FileSystem.h)
- [ConvertClass.h](https://github.com/Phobos-developers/YRpp/blob/d9795546950fcc75a94325ffc83b44d16ec34ce2/ConvertClass.h)
