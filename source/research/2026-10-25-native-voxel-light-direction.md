# 原版 voxel 光源、VPL 選頁與模型 Y 坐標校正

## 問題與適用範圍

使用者觀察原版光線在畫面上由左下射向右上，詢問光源高度，並要求外部渲染遵循遊戲邏輯。本項以 `v1.0.38-pre.3` / `4485b0a0e3508bba0191049ba6d0dc3df5913792` 為基線，核對原版光照指令、有效記憶體樣本、公開的原始碼及現有 renderer。

支援的原版映像 SHA-256：

`7b8a068535d6af06845edf95ae829b113d00c02909330e16f197426cd7db94b6`

私人樣本是 `RA2Master-Capture-20261025-003340-3568` 的 `00-clean`、`01-off`、`02-on`、`03-off`。映像基址為 `0x400000`。本筆記及公開 JSON 只包含數值、地址、指令區間雜湊與測試結果，不包含原 EXE、記憶體 dump、SHP/VXL 素材或擷取圖。

開始調查前已讀 `INDEX.md`、RenderFix2、RenderFix3、pre.12/pre.13 光照研究及持久筆記中對應的原版動畫／光照紀錄。原有「Cell 亮度已包括 Ambient/Ground/Level，不能重複相乘」的結論維持不變。

## 結論

1. 使用者說的**畫面左下 → 右上**與原版 voxel 指向光源的向量取反後吻合。
2. 原版是一套帶有歷史數值特性的方向光／高光選頁運算，沒有可據此換算成公尺的光源高度。它的 diffuse 世界方向在本樣本中有 **−21.233607°** 的帶符號仰角；另有 `(0,0,1)` 參與高光的 halfway vector。不能把它簡化為一個位於正高度的物理太陽。
3. 舊版不只是亮度參數不同：VPL 使用了 ccmaps/WAE 的平面簡化光向量，還漏了原版模型坐標到 preview 坐標的局部 **Y 反向**。兩者一起修正才可讓模型左右、原始 normal index 與畫面受光方向一致。
4. `Normals4[241].x` 的舊值符號也不符合本版原遊戲，應為負值。
5. 相機的 `project()`、全局 `visual_pose::radians()` 及地圖投影定義沒有改動。改動發生在 VXL/HVA 模型坐標轉換與光照選頁。

## 原版地址、ABI 與有效樣本

| 地址／欄位 | 已驗證用途 |
|---|---|
| `0x754C00` | `Set_Voxel_Light_Angle`：identity → RotateY(angle) → 轉換初始 `(-sqrt(.5),-sqrt(.5),0)`；`ret 4` |
| `0x88746C` | light angle，四個樣本均為 `0.7853749990463257f` |
| `0x887470` | view-space 指向光源向量，三個 float |
| `0x887420` | voxel shadow shift 向量，四個樣本均為 `(2.9999964237,0,0)` |
| `0xB44318` | `Matrix3D::VoxelDefaultMatrix`，3×4 float |
| `0x887430` | `Drawing::VoxelTransformMatrix`，本樣本為 identity |
| `0x706F23..0x706F4D` | Techno 繪製呼叫雙項光照，傳入 intensity `3.0f`、上述光源與 transform |
| `0x753D00` | `SetupVoxelDoubleLighting`：ECX=VoxLib、EDX=section/header index；stack 為另一 section index、draw matrix、transform matrix、light pointer、float intensity；`ret 0x14` |
| `0x753D86..0x753D9E` | 原版 secondary vector 初始化為 `(0,0,1)` |
| `0x7586F0` | 原始 normal table 的 diffuse + specular → VPL page；ECX=已轉換 light、EDX=secondary vector；stack=intensity、normal mode；`ret 8` |
| `0xB45990` | 256-byte normal-index → VPL-page 暫存表 |
| `0x8469E0` / `0x8469F4` | 各 NormalMode 的向量表指標／長度；mode 1..4 長度為 16、36、64、245 |
| `0x846F78` | 原版 mode 4 normal table |
| `0x847AC4` | mode 4 index 241 的 X float；四樣本均為 `-0.3281880021095276` |
| `0x5AFC20` / `0x5AF4D0` | 轉置旋轉矩陣／RotateVector；光照只用旋轉，不套平移 |
| `0x5AF1A0` / `0x5AF980` / `0x5AFB80` | RotateZ／矩陣乘法／矩陣套用到點，用於 native 幾何回放 |
| `0x4CACB0` / `0x4CAD00` | 8192 級角度查表的 sin／cos；負角度 index 的取整不是一般對稱四捨五入 |
| `0x4CAC40` / `0x7C5F00` | 查表 sqrt／`_ftol`；本遊戲控制字 `[0x822D80]=0x0E7F`，儲存 float 朝零截斷 |

四份 capture 的光源、矩陣、shadow shift、angle 所涵蓋 bytes 全部 `valid=1`、`stable=1`，且逐值一致。index 241 三分量亦分別核對四份 valid/stable 樣本。`00-clean` 中實際執行區間的 SHA-256 與長度收錄在 `source/verification/voxel-lighting-native.json`。

## 向量、坐標與「高度」

原版記憶體中的指向光源向量為：

```text
L_view = (-0.4999994338, -0.7071059942, +0.4999994338)
```

這裡的 Z 是 voxel view-space 軸，不能直接把 `+0.5` 當成世界高度。實測 default matrix 的旋轉部分為：

```text
 0.708190620   0.706021249   0
-0.353166908   0.354252070   0.865897536
 0.611342013  -0.613220513   0.500221372
```

原版使用轉置將光源轉入模型空間。在 identity 模型旋轉（facing 8）下，原指令回傳：

```text
L_native_world = (+0.2013021857, -0.9101138711, -0.3621709347)
```

轉到本專案 preview 的世界坐標時，Y 要反向：

```text
L_preview_world = (+0.2013021857, +0.9101138711, -0.3621709347)
```

套用**未改動**的 preview projection，畫面採 X 向右、Y 向下：

| 語意 | 畫面向量 | 方位 |
|---|---|---|
| 從表面指向光源 | `(-0.5012055494,+0.7065941452)` | 左下 |
| 光線射入／傳播方向 | `(+0.5012055494,-0.7065941452)` | 右上 |

以原版世界 Z 定義計算 `atan2(z,hypot(x,y))`，diffuse 方向的帶符號仰角為 **−21.233607°**。這是原版非物理光照運算的參數解讀。光源只具有方向，沒有距離，所以不能回答「光源在多少公尺高」。

`0x7586F0` 還會將 model-space light 與 secondary `(0,0,1)` 相加，再正規化為 halfway vector。identity 旋轉時，該 halfway 方向的仰角約 **+34.383246°**；它用來算高光，並非第二個可直接當太陽高度的數字。原版 normal-lighting 因此會混合一個帶負世界 Z 的 diffuse 項及一個被上方 secondary vector 推高的 specular 項。

Phobos 官方文件有相同歷史行為的說明，並提供可選的 `UseFixedVoxelLighting` 改變此 secondary vector。本輪忠於原版，沒有替原遊戲啟用這個 MOD 修正。

## 選頁公式與舊版差異

設 `a` 為本專案 preview angle，`D` 為原版 default matrix，`N` 為原始 VXL normal table 向量：

```text
M = D * Rz(-a)
L = transpose(M) * L_view
V = (0,0,1)                       # 本樣本 transform 為 identity
H = normalize(L + V)
d = max(dot(N,L), 0)
h = dot(N,H)
s = max(h / (3 - 3*h + h), 0)
page = truncate(16 * (d+s))
```

Normal indices 253、254、255 強制為 page 16。VPL 仍使用**原始 normal index**；HVA 影響幾何，不要再把 HVA 變形過的 normal 當成原始 VPL table normal。無 VPL 回退才使用 HVA／scale 變換後的 normal。

舊版先取 `heading=a+pi/2`，再使用 `(-cos(heading),-sin(heading),0)`；這與 pinned ccmaps/WAE 的簡化公式相同，但不同於本版遊戲實際的 `D`、光源向量及其量化。之前的「與 ccmaps 代數一致」不能推出「與 gamemd 原指令一致」。

新程式也保留了原版角度查表與 float/sqrt 截斷對 page 邊界的影響。數學表由標準函式重建，不附帶原遊戲查表 bytes，也不改變宿主執行緒的 FPU 控制字。每次 render 按 normal mode 及 body/turret 方向預先算 page table；不在每個面重做完整運算。

Mode 4 index 241 原本寫成 `(+0.328188,.140251,.93414301)`，本版原遊戲為 `(-0.3281880021,.1402509958,.9341430068)`；已修正 X 符號。這一點與來源開源表也有差異，採實際支援映像的數值。

## 模型 Y 校正與相機區別

原版 draw matrix 使用 `D*Rz(-a)`，之後畫面 Y 取反。本專案現有等距投影以另一個世界 Y 符號表示同一觀察方向。要保持兩者一致，local HVA 完成後必須使用：

```text
p_preview_local = (p_native_after_HVA.x,
                  -p_native_after_HVA.y,
                   p_native_after_HVA.z)
screen = project(rotate(p_preview_local, +a))
```

舊版缺少這個 local Y 反向。沿模型 X 的車頭方向仍容易看起來正確，但模型側向特徵及 normals 會互相不一致。例如 `a=0`、原模型點 `(0,1,0)`，native 畫面約為 `(+.706,-.354)`，舊 preview 是 `(-.707,+.354)`。

修正將 Y 反向放在 **HVA 後**，所以局部旋轉、炮管抬角及 HVA 的 Y 平移也一起轉到正確坐標；變換後的 fallback normal 同樣反向 Y。頂點繞序也反轉，以保留向外的幾何 face normal。原始 VPL normal table 不鏡射，因為其光源已用 native draw matrix 轉入相同模型坐標。

炮塔／炮管點使用自身 facing，turret offset 使用車身 facing。`posePoint()` 是原有 renderer lambda 的共用化，供 native anchor regression 直接檢查相同路徑，沒有另設相機角度。

## 驗證結果

完整機器可讀結果：`source/verification/voxel-lighting-native.json`。

| 檢查 | 結果 |
|---|---|
| 原版 light／矩陣／shadow／angle 來源 | 4 份 capture，全部範圍 valid/stable，一致 |
| mode 1..4 × 32 facings 的有效 normal-page | **11,552** 個 |
| 舊版與原版不同 | **6,916 / 11,552**，含舊 normal 241 符號 |
| 新版與原版不同 | **0 / 11,552** |
| 特殊 indices 253..255 | **384 / 384** 一致，均為 16 |
| 非對稱點、3 種 local HVA、32 facings、獨立 body/turret offset 組合 | **800** 組 |
| local Y 修正前最大畫面 XY 距離誤差 | **36.0333517 px** |
| local Y 修正後最大畫面 XY 距離誤差 | **0.0911934 px** |
| 修正後最大 depth 差 | **0.0795051** |
| 公開 C++ 直接 capture → HVA → posePoint → project anchors | 20 個 native golden anchors，包含四個主方向、炮塔 offset、炮管局部變換，通過 |
| VPL 像素回歸 | 兩個可見側面，左下側平均亮度大於右下側 1.5 倍，通過 |
| 無 VPL 像素回歸 | 使用同一方向的 fallback，左下側相同判定通過 |
| 獨立炮塔光照 | 改變 body facing、保持 turret facing，炮塔像素不變，通過 |

約 0.09 px 的幾何殘差來自維持現有相機的理想 sin/cos 投影，而原版 matrix/RotateZ 使用查表近似。這個結果**不是**偷偷調整相機去達成零誤差。原 `project()`、`visual_pose::radians()` 及全局地圖 projection 的公式保持不變。

`voxel_lighting.cpp` 對原版生成的 128 個完整 256-byte tables 做 FNV-1a golden 比對；無遊戲檔案也能在 portable/Windows gate 執行。`voxel_preview.cpp` 走實際的 read-only VXL/HVA capture，再比對原始指令生成的幾何 anchors。

重現命令（私人 capture 僅留在本機）：

```sh
g++ -std=c++17 -O2 source/assist/tests/voxel_lighting.cpp -o /tmp/voxel-lighting
/tmp/voxel-lighting --dump-pages /tmp/portable-voxel-pages.bin
python source/assist/tests/voxel_lighting_native.py /path/to/private/capture --portable-pages /tmp/portable-voxel-pages.bin --output /tmp/voxel-lighting-native.json
g++ -std=c++17 -O2 source/assist/tests/voxel_preview.cpp -o /tmp/voxel-preview
/tmp/voxel-preview
```

Native replay 需要 Python `unicorn`。它只執行所列數值函式及可控的合成 VoxLib／矩陣記憶體，不啟動遊戲更新、renderer、Win32 顯示或多人遊戲。

## 排除的假設與限制

- **不能從 WAE/ccmaps 的 light.Z=0 推出原版世界光源仰角 0°。** 那個平面簡化沒有重現本版 native default matrix 與完整 light vector。
- **不能直接將 `0x887470.z=+0.5` 解讀為正的世界高度。** 它是 view-space 向量，需要轉置矩陣。
- **不能用固定「45° 太陽」取代原版雙項公式。** −21.23° diffuse 與 +34.38° halfway 都是方向運算的解讀，沒有距離資訊。
- 本輪採四份樣本證明的**原版預設值**；沒有新增 live 全域讀取或 cache fingerprint。Phobos 自訂／動態變更的 `VoxelLightSource`、`UseFixedVoxelLighting` 不在本輪支援範圍。
- 本輪方向光套用在 VXL。建築等 SHP 的明暗／投影是素材內烘焙內容，仍走既有 palette 與 scene lighting，不能拿 voxel normal 重新打光。
- 現有 voxel shadow 使用 flatten 與固定約 `+3` 的偏移。本輪核對了原版 shadow shift 數值，**沒有**重建或驗證物理光源高度到陰影長度的投射；不能將本項寫成「所有陰影已按物理光源重算」。
- 數值指令回放、native golden anchors 和實際外部 raster 像素回歸均通過；尚不能據此宣稱每種遊戲素材、Windows 整體顯示或多人遊戲均已實測。

## 第一手公開來源

- [EA Mission Editor `RenderVXLSection`](https://github.com/electronicarts/CNC_TS_and_RA2_Mission_Editor/blob/6abf0f557469baea73079c6bf6550709e2e3584e/MissionEditorPackLib/MissionEditorPackLib.cpp)：normal/HVA 與 screen-space light 的 editor shading；不是本版遊戲 VPL 雙項運算的直接替代。
- [ccmaps `VxlRenderer.cs`](https://github.com/zzattack/ccmaps-net/blob/56712d7c9c5048afbfe9bddb01c0ec43f6fc008a/CNCMaps.Engine/Rendering/VxlRenderer.cs)：舊版參考的水平 YRLight、raw normal 選頁及 shadow shift 註解。
- [WorldAlteringEditor `VxlRenderer.cs`](https://github.com/CnCNet/WorldAlteringEditor/blob/d9f409a5568ecd47a1d1d82063fc147e7fac075a/src/TSMapEditor/Rendering/ObjectRenderers/VxlRenderer.cs)：確認 ccmaps 引用的平面 light / Blinn-Phong 實作。
- [Phobos voxel light source 文件](https://github.com/Phobos-developers/Phobos/blob/e81bf980f4606272a5928edef111852c81961433/docs/Fixed-or-Improved-Logics.md#voxel-light-source-customization)：方向不考慮距離；vanilla 約 `(0.201,-0.907,-0.362)`。文件為近似值，本次採 native 精確採樣，Y 為約 `-0.910114`。
- [Phobos `ReplaceVoxelLightSources`](https://github.com/Phobos-developers/Phobos/blob/e81bf980f4606272a5928edef111852c81961433/src/Ext/Rules/Body.cpp)：自訂世界方向 normalize 後乘 `VoxelDefaultMatrix`，寫入遊戲 light source。
- [Phobos `VoxelCalcNormals_NullAdditionalVector`](https://github.com/Phobos-developers/Phobos/blob/e81bf980f4606272a5928edef111852c81961433/src/Misc/Hooks.BugFixes.cpp)：`0x753D86` 原版 secondary `(0,0,1)` 與可選修正的區別。
- YRpp pinned `d9795546950fcc75a94325ffc83b44d16ec34ce2`：[Unsorted.h](https://github.com/Phobos-developers/YRpp/blob/d9795546950fcc75a94325ffc83b44d16ec34ce2/Unsorted.h)、[Matrix3D.h](https://github.com/Phobos-developers/YRpp/blob/d9795546950fcc75a94325ffc83b44d16ec34ce2/Matrix3D.h)、[Drawing.h](https://github.com/Phobos-developers/YRpp/blob/d9795546950fcc75a94325ffc83b44d16ec34ce2/Drawing.h)、[Dir.h](https://github.com/Phobos-developers/YRpp/blob/d9795546950fcc75a94325ffc83b44d16ec34ce2/Dir.h)：全域地址、矩陣運算 ABI、雙光源函式及 facing 角度符號。
- [ThomasSneddon VXL renderer `d3d.h`](https://github.com/ThomasSneddon/vxl-renderer/blob/48a5c1b4064c1ec0128cb87ddefd484d4e37ad49/d3d.h)：工具預設世界向量 `(0.2013022,-0.9101138,-0.3621709)`，與此次 native 轉置結果吻合。
