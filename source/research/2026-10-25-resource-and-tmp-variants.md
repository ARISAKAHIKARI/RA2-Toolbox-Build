# 礦石圖案池與地形 TMP 變體

## 問題、基線與證據

使用者指出相同礦石覆蓋圖和部分地形，在遊戲中有多種圖案，工具箱則重複同一張。基線是 v1.0.38-pre.7 的 `04fd222f0912f0c0c7c55d3f694c6018c6c9986a`。調查前讀取研究 INDEX、SQLite 的 pre.8/pre.13 資源與覆蓋物紀錄、礦柱及亮度修復紀錄。pre.13 已明列礦石變體尚未復刻，本次補上這一缺口。

支援 EXE SHA-256：`7b8a068535d6af06845edf95ae829b113d00c02909330e16f197426cd7db94b6`。只讀私人 `RA2Master-Capture-20261025-003340-3568` 的 `01-off`、`02-on`、`03-off`。原始 EXE、記憶體、圖案、查表內容不進入倉庫。新增證據包括有效 bytes 反組譯、原指令受控執行與實際 production C++ 對照、兩個真實採樣器及場景合成回歸。沒有執行完整遊戲、Windows 合成或雙機聯機。

## 礦石的儲存索引不是實際 SHP 索引

原版 `Cell::DrawOverlay 0x47F6A0` 是 thiscall，兩個 stack 引數，`ret 8`。它先按 cell 的原始 `OverlayTypeIndex +0x44` 計算位置與判斷類型。原始類型 `+0x2A9` 為資源時，`0x47F860` 呼叫 `0x5FDD20`（ECX=原始 overlay index，EAX=資源類型索引，無 stack 引數），然後 `0x47F871..0x47F900` 選另一個 OverlayType 的圖像。

| 欄位 | 原版用途 |
| --- | --- |
| OverlayType vector `0xA83D80` | 資料指標 `+4`，數量 `+16` |
| Tiberium vector `0xB0F4E8` | 資料指標 `+4`，數量 `+16` |
| Tiberium `+0xE0` | 圖案池第一個 OverlayType 指標 |
| 第一個 OverlayType `+0x294` | 池起始 overlay index |
| Tiberium `+0xE8` | 平地圖案數量 NumImages |
| Tiberium `+0xEC` | 坡面圖案總數 NumSlopes |
| Cell `+0x24/+0x26` | signed short X/Y |
| Cell `+0x11C` | 坡面種類 |
| Cell `+0x11E` | SHP frame；圖案池選型後仍保留此值 |

令 `product = cellX * cellY`、`first = Image.ArrayIndex`：

* 平地：`drawIndex = first + product % NumImages`。
* 非零坡面：`perSlope = NumSlopes / 4`；`drawIndex = first + NumImages + product % perSlope + (SlopeIndex-1)*perSlope`。

三份 capture 均有 Riparius/TIB01 的 12+8 圖案池、Cruentus/GEM01 的 12+0、Vinifera/TIB2_01 和 Aboreus/TIB3_01 的 12+8。修復讀取 live 池，不硬編碼這些名稱、12 張數量或固定 overlay index；同一 cell 不會因渲染次數而隨機換圖。缺失、零 NumImages、無可用坡面、越界或不一致的池配置安全回退原始圖案，避免除零與越界。

原版先用儲存類型計算 `GetOverlayOffset`，再用池選出的類型取 SHP。故 `CellArt.overlay` 和新增 `overlaySourceType` 保留原始類型，`overlayType` 是實際繪圖類型。快取自然按實際 type/frame 區分，不能只改 filename 而仍共用原始 type 的圖片。

## 部分地形另有鏈式 TMP 變體

`Cell::DrawIsoTile 0x480350`（thiscall、三個 stack 引數、`ret 0xC`）在 `0x480389..0x480403` 選擇變體。IsoTileType `+0x2F0` 是變體數量；`+0x2BC` 連到下一個變體。三份 capture 的類型庫含 1、2、4、8 個變體的地形；01-off 為 708/94/30/6 類。

`0x5471F0`（thiscall、subtile index、`ret 4`）從主 TMP 的 relocated frame directory 查所選 subtile 的 header `+36` flag bit 2。設置時使用 Cell `+0x140` 的 bit 13，決定正常／損壞變體。

其他情況呼叫 `Cell 0x4814F0`（thiscall、tile index 和變體數量、`ret 8`）。它保留原版對非零 subtile 的 root-coordinate 調整與朝零整數除法；變體數量不超過 4 時，按低兩位 X/Y 查 `0x81CCA8` 的 4×4 表；更多變體按低三位查 live `0x89E620` 的 8×8 表。後者由原版初始化，`0x89E7C7` 未就緒時外部 reader 保留基圖，絕不初始化或改寫遊戲的隨機狀態。

`0x547D15..0x547D68` 把選擇值按 count 取餘，沿 `+0x2BC` 走到真正繪製的 type。因此即使兩格的原始 IsoTileTypeIndex 和 subtile 一樣，仍可能畫不同 TMP；不能固定從基類型的 `+0xA4`／filename 讀圖。

## 共用修復與光照

新增 `external_reader/source/art_variants.h`；觀察窗及實景小地圖的採樣器共用同一選型程式。地形原有 cache key 含選中 type/subtile，覆蓋圖 cache key 含選中 type/frame，不會將池內不同 SHP 或不同 TMP 合併。

另外修正 `radar_scene.h` 的既有光照不一致：它原先把所有覆蓋圖套地形亮度，只處理橋例外；觀察窗已包含原版資源及牆分支。現在兩者都在橋層優先後，依**原始儲存類型**的 `+0x2A9` 對資源使用 1000 強度、依 `+0x2A8` 對牆使用 object cell 強度，其餘使用 terrain 強度。這符合 `0x47F852` 在池選型之前的原版類型分支。資源色盤、礦柱路徑、SHP frame 和原有攝像機投影不另行猜測。

## 驗證與界線

* `assist/tests/art_variants.cpp`：固定儲存索引的十二種平地／八種坡面結果，TMP 表與鏈選型、損壞旗標、失讀／零數量／缺鏈／未初始化表回退，並直接執行 production watch::sample 和 radar::Sampler，要求相鄰同儲存類型的兩格選出不同圖案且兩個採樣器一致、frame=7 保留。
* `assist/tests/art_variants_native.py`：三份 capture 執行原版 `5FDD20`、`47F871..47F900`、`480389..480403`、`4814F0`、`5471F0` 和 `547D15..547D68`。GetImage 邊界返回合成 TMP；cell/type/table 是合成輸入。與實際編譯的 production helper 比較 **8,505 組，零差異**。可公開的數量、配置與 code-range SHA 保存在 `verification/art-variants-native.json`。
* `assist/tests/radar_scene.cpp` 新增實際輸出像素檢查：地形強度 0，資源／牆仍用 1000；原始與繪圖類型旗標相反時依原始類型；橋層優先使用 500。原有場景與 watch_view 回歸保持通過。

重現命令：

```sh
g++ -std=c++17 -O2 source/assist/tests/art_variants.cpp -o /tmp/art-variants
/tmp/art-variants
PYTHONPATH=/path/to/native-deps:source/assist/tests python3 source/assist/tests/art_variants_native.py /path/to/private-capture --output /tmp/art-variants-native.json
```

FA2 的 OverlayData 逐幀載入不足以解釋本次缺口；SHP decoder 已能取不同 frame，缺的是 frame 之外的**圖案池選型**與 TMP 變體鏈。公開一手交叉參考：[ccmaps SpecialOverlays.GetPooledDrawId](https://github.com/zzattack/ccmaps-net/blob/56712d7c9c5048afbfe9bddb01c0ec43f6fc008a/CNCMaps.Engine/Game/SpecialOverlays.cs)。原版重播另獨立確認以上 live 欄位與公式，未用該註解代替原指令證據。

尚未驗證所有 MOD 對池／TMP 的擴展、原版坡面像素變形與橋特殊幀表、完整場景逐像素等同或實機效能。受控選型零差異不擴大為遊戲畫面或聯機驗收。
