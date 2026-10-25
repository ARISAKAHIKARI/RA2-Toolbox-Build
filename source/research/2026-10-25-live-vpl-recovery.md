# VXL 仍偏暗：原生額外亮度與 VPL 快取恢復

## 問題、結論與範圍

使用者實測 pre.5 後回報 SHP 亮度已改善，但 VXL 戰車仍偏暗。先前 pre.4 的受光方向已獲實機確認，本輪不修改光源方向、相機角度、投影、法線選頁或全局亮度。

這次找到兩項獨立缺口。**直接影響正常車輛亮度的是漏讀 `ExtraUnitLight`**：原版 Unit::Draw 會在 cell object brightness 後加上 Rules 的此欄位，捕獲值為 200。AMCV／HARV／SMIN 的原版實際傳入亮度因此是 1256／1368／1256，舊 helper 卻是 1056／1168／1056。Aircraft::Draw 也有自己獨立的 `ExtraAircraftLight` 加法；本輪補上已驗證的加成，沒有擴改飛行高度計算。

另一項是 VPL 來源／快取缺口：`captureActor` 能從遊戲記憶體取得 VXL/HVA，但 `Context::vpl` 只嘗試從檔案讀 `VOXELS.VPL`；失敗仍記下檔案 revision，後續一直使用近似 Lambert 回退。原版已載入的 VPL 就在固定記憶體緩衝，外部 renderer 卻沒有讀取它。即使之後取得正確表，舊的 asset revision 也沒有包含 VPL 內容，已快取姿態不會因此更新。

此次補上原生設定的額外亮度，以及原生 VPL 優先、檔案回退、失敗重試和姿態失效，各有紅綠回歸。**沒有直接觀察到使用者 pre.5 執行時的 VPL readiness，因此不能宣稱 missing VPL 是新截圖偏暗的唯一原因，也不能把受控驗證寫成已通過新版本實機畫面驗收。**

支援映像 SHA-256：

`7B8A068535D6AF06845EDF95AE829B113D00C02909330E16F197426CD7DB94B6`

映像基址 `0x400000`。私人 capture 為 `RA2Master-Capture-20261025-003340-3568` 的 `00-clean`、`01-off`、`02-on`、`03-off`。調查前已查閱研究 INDEX、SQLite notebook、[voxel 光源方向](2026-10-25-native-voxel-light-direction.md)與[物件 LightConvert 來源](2026-10-25-native-object-lighting-source.md)。公開輸出只有統計、地址、hash 與測試程式，不含原 EXE、dump、VPL、模型、palette 或 framebuffer。

## 原版實際送入的額外亮度

### 欄位是已縮放的 signed integer

| 地址／欄位 | 已確認用途 |
|---|---|
| `[0x8871E0]` | RulesClass 全局指標 |
| Rules `+0x17D4` | ExtraUnitLight，執行期為 signed 32-bit integer |
| Rules `+0x17DC` | ExtraAircraftLight，執行期為 signed 32-bit integer |
| `0x83A250`、`0x83A228` | 原始 INI key 字串 `ExtraUnitLight`、`ExtraAircraftLight` |
| `0x66B69E..0x66B6B6` | 讀 ExtraUnitLight double，乘 `[0x7E4658]` 的 double 1000，呼叫 `_ftol`，將 EAX 寫入 `+0x17D4` |
| `0x66B730..0x66B74E` | ExtraAircraftLight 採相同縮放，將 EAX 寫入 `+0x17DC` |

Pinned [YRpp RulesClass.h](https://github.com/Phobos-developers/YRpp/blob/d9795546950fcc75a94325ffc83b44d16ec34ce2/RulesClass.h) 把這些欄位宣告為 float，但支援映像的實際載入、加法指令與 capture 都是整數。不能因 header 名稱或型別把 raw integer 200 當成 IEEE float。

新增 probe 只替身 INI ReadDouble 的返回值：把指定 double 放到 x87，再執行原版的乘法、`_ftol` 和寫入，未重造解析器。兩個欄位各測 `0.2、0、-0.2、0.75、-2、4`，共 12 例，得到 `200、0、-200、750、-2000、4000`。因此 production 必須讀當前 signed integer，不能固定加 200，也不能再次乘 1000。

### UnitClass：SHP 與 VXL 都使用同一項加成

`0x73D0C3..0x73D0D7` 在讀到 cell object brightness 後，無條件把 `Rules+0x17D4` 加到 EBP，並保存到 stack。之後 `0x73D317` 才檢查 `Type+0x236`：

- VXL 分支 `0x73D32D` push EBP，`0x73D359` 呼叫 virtual `+0x554`。
- SHP Unit 分支 `0x73D363` 同樣 push EBP，`0x73D38F` 呼叫 virtual `+0x558`。

這是 UnitClass 的規則，不能限於 voxel flag。Buildings 和建築動畫有各自的來源，不應拿此欄位套到它們。

以 `01-off` 中三個普通車輛的真實 virtual `+0x1B8` 取得 cell 座標，再執行原版 `0x73CF70..0x73D0DB`，包含原版 map 查詢，結果如下：

| 捕獲單位 | Cell object brightness | 捕獲 ExtraUnitLight | 原版傳入亮度 | 修正後 helper |
|---|---:|---:|---:|---:|
| AMCV | 1056 | 200 | 1256 | 1256 |
| HARV | 1168 | 200 | 1368 | 1368 |
| SMIN | 1056 | 200 | 1256 | 1256 |

每個單位再交叉 extra=`0、200、-200、600、-2000、4000`，合計 18 組 base+extra 原指令回放。`0x70676F..0x7067E4` 的 VXL wrapper 在這些普通狀態下保持傳入值。另對兩種 Type voxel flag 執行原版 dispatch，於真正 SHP／VXL draw 入口檢查 brightness stack argument，共 36 次，數值一致；draw 本身在此邊界停止。

這找到了前面「相同 brightness 下 scalar 都正確」未涵蓋的差異：**舊 helper 使用的 brightness 本來就少了原版規則加成**。它不是相機、VPL normal-page 或 palette 中的另一層全局乘法。

### Aircraft：只修獨立加法，保留高度計算的驗證界線

`0x414925` 讀 Rules 指標，`0x41492A` 讀 `+0x17DC`，`0x414936` 加到 EBX；`0x41493C` 再加 ESI 的高度項。`0x414996` push EBX，後續 `0x4149D7` 呼叫 virtual `+0x510`，進入 VXL draw wrapper。

Probe 用三個捕獲 cell 的 base 值和六個正／負／零 extra，控制高度項為零，執行原版 `0x414925..0x41493E` 與 `0x414987..0x414997` 的加法／入棧 slice，共 18 例。production 的 kind=0 使用獨立 `+0x17DC`，與這些受控值一致。這裡使用合成 object layout，沒有聲稱是完整 Aircraft::Draw 或捕獲中某架飛機的全流程回放。

原版高度項的反組譯另顯示：virtual `+0x1C8` 取得距地高度，除以 `2*[0x889EC8]`，再乘 Scenario Level；普通 Level 來自 `+0x3544`，storm 分支來自 `+0x355C`。此 capture 的高度步長為 104、兩個 Level 為 28／0。距地高度不是直接使用 world Z。本輪沒有完整驗證高空／橋／特殊天候路徑，因此不修改既有高度近似，也不宣稱全高度亮度已逐位元符合原版。

### Production 邊界與特殊效果

`scene_lighting::object` 僅在 kind=2 讀 `+0x17D4`、kind=0 讀 `+0x17DC`；以 64-bit 檢查指標範圍與相加，前後 Rules 指標一致才採用。指標失效、欄位失讀或 Rules 換局時保留原 cell／fallback brightness，不代入猜測的 200。

原版 Unit::Draw 的加法本身沒有飽和。工具維持既有 0..2000 呈現範圍；負結果需要歸零，避免被 `State::intensity<0` 的 fallback sentinel 誤當成 ambient。測試把 raw native 加法結果和呈現範圍分開保存，沒有把這個範圍描述成 Unit::Draw 內部的運算。

獨立審查另核對下游 RLE：`0x4990EE..0x4990F8` 將負 brightness 歸零，`0x499104..0x499125` 計算 `(261*brightness)>>11` 並限制 shade 到 254。以 `01-off` 中首個 `ColorScheme+0x310>1` 的 converter，呼叫 `Native.call(0x490E50, converter, [0x6800])` 選 blitter，再使用既有 `assist/tests/native_lightconvert_capture.py::Native.pixel(blitter, value, 127, 48)`：`-2000/-944/-1/0` 四值皆輸出黑，`1999/2000/5056` 三值輸出同一最高階 RGB565。這七個已執行的邊界檢查與後文 4,800 次矩陣分開計數，支持正常下游呈現的飽和處理；沒有把原始 Unit 加法說成有 clamp。

以下特殊狀態仍是既存未模型化路徑，本輪不擴大修改：

- SHP `0x706328`／VXL `0x706780` 呼叫 virtual `+0x464`。普通 Unit 實作 `0x70D190` 在 `Object+0xF0` bit 1 未設時保持傳入值；設置時會依亮度改為 2000 或 500。
- Virtual `+0x160` 的計時狀態可能啟用 `0x70E380`／`0x70E4B0` 效果。三個 `01-off` 樣本沒有啟用，所以前述 wrapper 回放保持原值。
- SHP 的非 clearly-visible disguise 特殊分支可能以 cell terrain brightness 覆蓋先前加成。一般 SHP Unit 的分流證據不等於此例外已模型化。
- Infantry 有獨立欄位和 drawing 路徑；本輪未外推套用 Unit／Aircraft 的設定。

Building 的 ExtraLight、建築動畫 assigned converter／intensity、礦柱的 object intensity 來源均維持原規則，portable regression 明確檢查這些結果不受兩個新欄位影響。

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

新增 [native probe](../assist/tests/native_voxel_vpl_capture.py) 與 [數值結果](../verification/native-voxel-vpl-capture.json)；production portable 回歸在 [scene_lighting.cpp](../assist/tests/scene_lighting.cpp)、[voxel_lighting.cpp](../assist/tests/voxel_lighting.cpp) 和 [game_assets_host_test.py](../assist/tests/game_assets_host_test.py)。

| 驗證 | 實際結果 |
|---|---|
| 舊 production host：記憶體 VPL 已就緒、檔案缺失後恢復 | 新 assertion 失敗，SIGABRT；修復後通過 |
| 僅依 files revision 的中間版本：imagePath=false、跨 match 都 revision=0 | 新 assertion 失敗，SIGABRT；直接 identity reset 後通過 |
| 舊 object helper：Unit 1056＋Rules 200 | 新 1256 assertion 失敗；讀 live ExtraUnitLight 後通過 |
| 僅修 Unit 的中間版本：Aircraft 獨立設定 | 新 assertion 失敗；讀 live ExtraAircraftLight 後通過 |
| 正／負／零規則、兩種 Unit art、欄位隔離、失讀／換指標、最終 gray pixel | 通過；building／animation／礦柱保持原值 |
| 原版 Rules scaled-integer 寫入 | Unit／Aircraft 各 6 例，共 12 例 |
| Unit base+extra／VXL wrapper／SHP-VXL dispatch | 18／18／36 例，普通 capture 狀態 |
| Aircraft extra 加法與 brightness 入棧 | 18 例，高度項受控為零，非完整 aircraft draw |
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
g++ -std=c++17 -O2 source/assist/tests/scene_lighting.cpp -o /tmp/scene-lighting
/tmp/scene-lighting
python3 source/assist/tests/game_assets_host_test.py
PYTHONPATH=/path/to/native-deps:source/assist/tests \
  python3 source/assist/tests/native_voxel_vpl_capture.py \
  /path/to/private-capture --output /path/to/native-voxel-vpl-capture.json
```

紅例 log 保留於工作環境 `/tmp/ra2-vpl-pre6-red.log`、`/tmp/ra2-vpl-identity-red.log`、`/tmp/ra2-unit-light-red.log`、`/tmp/ra2-aircraft-light-red.log`，綠例 `/tmp/ra2-vpl-pre6-green.log`、`/tmp/ra2-unit-light-green.log`。原始像素和先期探索 probe 只留私人工作目錄，未列入發佈。全量 build／Windows gates 由主代理另行執行，portable 與原版受控執行不等於使用者新版本的實機驗收。
