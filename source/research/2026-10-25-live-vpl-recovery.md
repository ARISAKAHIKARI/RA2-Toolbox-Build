# VXL 仍偏暗：原生 VPL 來源與快取恢復

## 問題、結論與範圍

使用者實測 pre.5 後回報 SHP 亮度已改善，但 VXL 戰車仍偏暗。先前 pre.4 的受光方向已獲實機確認，本輪不修改光源方向、相機角度、投影、法線選頁或全局亮度。

已證實一個可以重現的來源缺口：`captureActor` 能從遊戲記憶體取得 VXL/HVA，但 `Context::vpl` 只嘗試從檔案讀 `VOXELS.VPL`；失敗仍記下檔案 revision，後續一直使用近似 Lambert 回退。原版已載入的 VPL 就在固定記憶體緩衝，外部 renderer 卻沒有讀取它。即使之後取得正確表，舊的 asset revision 也沒有包含 VPL 內容，已快取姿態不會因此更新。

此次補上原生表優先、檔案回退、失敗重試及姿態失效。這是有紅綠回歸的來源／快取修復；**沒有直接觀察到使用者 pre.5 執行時的 VPL readiness，因此不能宣稱它是新截圖偏暗的唯一原因，也不能把它寫成已通過新版本實機畫面驗收。**

支援映像 SHA-256：

`7B8A068535D6AF06845EDF95AE829B113D00C02909330E16F197426CD7DB94B6`

映像基址 `0x400000`。私人 capture 為 `RA2Master-Capture-20261025-003340-3568` 的 `00-clean`、`01-off`、`02-on`、`03-off`。調查前已查閱研究 INDEX、SQLite notebook、[voxel 光源方向](2026-10-25-native-voxel-light-direction.md)與[物件 LightConvert 來源](2026-10-25-native-object-lighting-source.md)。公開輸出只有統計、地址、hash 與測試程式，不含原 EXE、dump、VPL、模型、palette 或 framebuffer。

## 原版 VPL 載入與讀取

| 地址／欄位 | 原始指令與受控執行確認的用途 |
|---|---|
| `0x753B70` | ECX=FileClass，無 stack 參數，EAX=錯誤旗標；建立 loader，指定 palette `0xB2FB78` 和 pages `0xB41178`，呼叫 `0x758A30`，之後才公開 header 欄位 |
| `0x758A30` | thiscall，ECX=loader，stack=FileClass，`ret 4`；依序讀 16-byte header、768-byte palette、`pageCount*256` bytes，沒有改寫 page 對應 |
| `0xB2FB58` | 已載入的 page count；四份 capture 均為 32 |
| `0xB41178..0xB43178` | 原版固定 page buffer，共 8,192 bytes；外部 capture 必須拒絕 count=0 或 count>32 |
| `0xB2FB74`、`0xB4317C` | Remap 起訖；capture 為 16、31 |
| `0xB43178` | Header 保留欄位；capture 為未初始化值，不能拿它當有效性判斷 |
| `0x7576B9..0x7576F8` | `0xB45990[normal]` 得到 page，再以 `0xB41178[page*256+sourceIndex]` 產生暫存 indexed image |

四份 capture 的完整 8,192 bytes 都有有效／穩定標記，且 table SHA-256 相同：

`d4aca52746acb55d9893a3c2c347ddd8b506447e3945c9c0ff2e3cfba1f81bf0`

新增 probe 執行完整 `0x753B70` 與 `0x758A30`，只替身 FileClass 的 open/read/close 邊界。合成 1、16、32 頁，各次實際讀長都是 16、768、count×256，palette 與所有 page bytes 原樣進入目的地址；在讀完資料之前 count 尚未發布。這驗證的是原版載入語意，不是外部檔案探索是否能在使用者安裝目錄成功。

## 亮度排查分成兩段

### 原版 indexed voxel raster 與同一張 raw palette

以 `01-off` 中 AMCV、HARV、SMIN 的 **body model**，各取 facing 0、8、16、24，共 12 組，執行原版以下流程：

| 函式 | 呼叫邊界 |
|---|---|
| `0x5AF1A0` | ECX=rotation matrix，stack=float angle，`ret 4` |
| `0x5AF980` | ECX=destination matrix、EDX=left matrix、stack=right matrix，`ret 4` |
| `0x753D00` | ECX=voxel library，EDX=section/header index，stack={另一 section index, draw matrix, transform matrix `0x887430`, light pointer `0x887470`, float intensity}，`ret 0x14`；本測試 section=0、intensity=3 |
| `0x753E00` | 清空原版 voxel buffers，無 stack 參數 |
| `0x7540F0` | ECX=voxel library、EDX=section，stack={0, limb draw matrix}，`ret 8` |
| `0x754510` | ECX=輸出矩形，無 stack 參數；完成 indexed raster 與 bounds |

所有呼叫皆檢查 EIP 正常返回與 ESP 清理量。輸出的 palette index 用同一個 owner ColorScheme 原始 RGB 展開；production 一側使用相同模型、`capturePart`、`Vpl::capture` 與 `voxel::render`，另跑無 VPL 的現有回退。原始資料只以 stdin 傳給暫存 C++ probe，不寫入公開檔案。

比較的是物件內部 indexed raster，**尚未經過 scene LightConvert、ABuffer、場景遮擋、陰影和縮小取樣**。不是執行完整 `0x4AF2A0`，也不是新截圖的逐像素對位。原版與外部幾何覆蓋不同，不能把均值接近說成每個像素相同。

差值定義為 production 減原版，luma 使用 RGB8 的 0.2126/0.7152/0.0722 加權：

| Body／facing | 正確 VPL 的平均 luma 差 | 無 VPL 回退的平均 luma 差 | 回退相對原版 |
|---|---:|---:|---:|
| AMCV／0 | +0.674 | +3.948 | +6.80% |
| AMCV／8 | +2.594 | -10.012 | -11.20% |
| AMCV／16 | +1.224 | +1.211 | +1.79% |
| AMCV／24 | +2.260 | -10.608 | -11.40% |
| HARV／8 | +1.725 | -6.388 | -6.73% |
| SMIN／24 | +1.004 | -2.149 | -2.69% |

全部 12 組使用正確 VPL 時，平均 luma 差介於 -0.059 與 +2.707；無一組有 opaque index-zero pixel。完整數值、bounds 和覆蓋像素數在 [JSON](../verification/native-voxel-vpl-capture.json)。這排除這些樣本在「正確 VPL＋raw palette」階段有一致的大幅壓暗；無 VPL 回退則有明確的朝向相關差異，不能靠一個全局增益修正。

### Fresh 與 cached 最終 scalar blitter

原版新建 VXL image 經 `0x490B90 SelectBlitter`，已快取 VXL image 經 `0x490E50 SelectRLEBlitter`。兩者都是 thiscall，ECX=converter，stack=flags，`ret 4`。

| 受控 flags | Fresh plain draw | Cached RLE draw |
|---|---|---|
| `0x2800` | `0x494B60` | `0x497FD0` |
| `0x6800` | `0x4958D0` | `0x4990E0` |

Fresh draw 的 8 個 stack 參數為 destination、indexed source、count、Z level、Z buffer、ABuffer、brightness、附加參數，`ret 0x20`。Cached draw 的 10 個 stack 參數依原版 RLE 呼叫邊界設定，`ret 0x28`。

新增測試在三份 capture 中實際執行這四個函式，交叉 8 個 brightness、5 個 ABuffer 值、10 個 palette index 與兩種 flags，合計 **4,800 次單像素繪製**。Fresh 與 cached 都逐位元等於同一 converter 的 LUT；也確認它們使用相同 FullColorData 與亮度查表。沒有在這些原版最終 scalar 路徑找到 VXL 獨有的額外整體增亮。

這不代表外部 compositor 已逐位元等於原版：目前 63 級對原版 53 級、RGB565 量化、場景 ABuffer 和幾何取樣仍有既存近似，詳見前述 LightConvert 筆記。這次沒有重跑整個場景 wrapper，也沒有宣稱解決所有可能的視覺差異。

程式追蹤亦確認正常外部 VXL 使用原始 owner/custom palette，VPL 只映射 index，沒有先把 scene tint 烘入姿態；`camera::Surface::blit` 才套一次 `camera::Light`。SHP 使用相同場景 light helper。VPL ready 的分支不再套 Lambert diffuse，因此沒有找到「VPL 與 Lambert 都套上」的 double shading。沒有調整 alpha、白底／黑底、陰影係數或相機。

## 修復與快取語意

1. `Vpl::capture` 讀 page count 和完整表兩次，前後再查 count，一致才替換。頁數限制 1..32；失讀或變動保留上一份有效資料。全黑的自訂 VPL 合法，不以「有非零色」作為有效性條件。
2. `Vpl::signature` 以 page count 與表內容計算，檔案和記憶體來源內容相同時 signature 相同。空表為零。`Context::revision` 納入此 signature，正確表到達後舊姿態需重算。
3. `Context::update` 每秒最多探查一次原生表，成功就優先使用；原生不可讀且尚無有效表才讀檔案。沒有來源時持續有界重試，不再永久記住一次失敗。成功檔案回退也繼續探查原生來源。
4. 同一局短暫讀不到表，保留已驗證的 snapshot；檔案 revision 變更時清空。不同 process／match 直接清表及重試狀態，不只依賴檔案 revision。這點有獨立紅例：`imagePath` 失敗時 Files 會回 revision=0，前後兩局都為 0，僅比檔案 revision 會錯留上一局表。

有效 file-only fallback 的 loose VPL 原地改寫仍依檔案 revision 更新；本輪未擴大為一般檔案監視。原生 VPL 已可讀時，以遊戲實際載入的表為準，會定時比較新內容。

獨立整合審查確認 `Context::Read` 使用 `readFresh`，上述一致性讀取不會被同頁快取替代。`watch_host.cpp` 與 `radar_scene.cpp` 另直接執行 production renderer：保持同一物件／pose，先快取兩個朝向；相同 revision 不重畫，revision 改變後重新 capture source art、捨棄兩個舊朝向並產生新色像素，之後連續 20 次穩定 revision 不重複捕獲／繪製。這驗證表內容 signature 接到實際姿態快取失效的整條路徑。

## 測試、紅綠證據與重現

新增 [native probe](../assist/tests/native_voxel_vpl_capture.py) 與 [數值結果](../verification/native-voxel-vpl-capture.json)；production portable 回歸在 [voxel_lighting.cpp](../assist/tests/voxel_lighting.cpp) 和 [game_assets_host_test.py](../assist/tests/game_assets_host_test.py)。

| 驗證 | 實際結果 |
|---|---|
| 舊 production host：記憶體 VPL 已就緒、檔案缺失後恢復 | 新 assertion 失敗，SIGABRT；修復後通過 |
| 僅依 files revision 的中間版本：imagePath=false、跨 match 都 revision=0 | 新 assertion 失敗，SIGABRT；直接 identity reset 後通過 |
| 原生 32 頁 capture、全部 index 對應、檔案／原生 signature 相等 | 通過 |
| 五處 read 失敗、兩處 count race、table race、count 0/33/256/MAX | 全部拒絕並保留有效表 |
| 原生優先、file→native、missing→loose file、同 file revision 表變動 | 真實 production renderActor 的 index 與 RGB 通過 |
| 連續同時刻呼叫、穩定表、不必要檔案讀取 | 通過 |
| 原有法線與特殊 index golden | 11,936 筆通過，受光方向與獨立 turret 測試保留 |
| 原生有效／穩定 VPL table | 四份完整 8,192-byte 表 |
| 原版 loader／fresh-cached scalar／body raster | 3 組／4,800 像素／12 組，範圍分列如上 |

```sh
g++ -std=c++17 -O2 source/assist/tests/voxel_lighting.cpp -o /tmp/voxel-lighting
/tmp/voxel-lighting
python3 source/assist/tests/game_assets_host_test.py
PYTHONPATH=/path/to/native-deps:source/assist/tests \
  python3 source/assist/tests/native_voxel_vpl_capture.py \
  /path/to/private-capture --output /path/to/native-voxel-vpl-capture.json
```

紅例 log 保留於工作環境 `/tmp/ra2-vpl-pre6-red.log`、`/tmp/ra2-vpl-identity-red.log`，綠例 `/tmp/ra2-vpl-pre6-green.log`。原始像素和先期探索 probe 只留私人工作目錄，未列入發佈。全量 build／Windows gates 由主代理另行執行，portable 與原版受控執行不等於使用者新版本的實機驗收。
