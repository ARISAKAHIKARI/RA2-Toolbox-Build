# RenderFix2：原版動畫、遮擋、對齊與光照複核

問題：使用者回報上一版仍看不到任何建築動畫，遮擋不正確，非方形小地圖左下留白，SHP/VXL 發黑。已先查 INDEX、pre.12/pre.13 光照、pre.2 渲染與同一持久 SQLite。本篇**更正**前兩篇修復筆記對 OwnerObject、相對座標及 PausedAnimFrame 的判斷；舊筆記保留作歷史。

## 樣本、入口與證據邊界

私有 RA2Master 快照中的遊戲 EXE SHA-256：`7b8a068535d6af06845edf95ae829b113d00c02909330e16f197426cd7db94b6`。不發布 EXE、原始記憶體或遊戲資源。

- `assist/tests/animation_capture_replay.py/.cpp` 使用真實記憶體及逐位元組 validity mask，執行生產採集、解碼、光照/深度和實景合成；只替代不可用的 Win32 磁碟 I/O。
- `assist/tests/animation_native_capture.py` 在 Unicorn 執行快照中的原版座標、層級、排序及顯示幀指令；沒有執行整個遊戲更新/資源載入器/Windows 顯示。
- 數量與原指令 SHA-256 見 `verification/renderfix2/`。編譯、ASan/UBSan、Win32 UI fixture 不等於遊戲畫面逐像素一致。

## 動畫消失：已確定的根因

`Building::PlayAnim 0x451890`（thiscall）反組譯：以 GetRenderCoords 建立**絕對座標**動畫，存入建築 `+0x55C` 的 21 個槽（受損火焰另有 `+0x5C8` 的 8 槽），設 Anim `+0x118`，但不填 `OwnerObject+0xCC`。三份實機快照各有 39、43、44 個有效動畫，**全部 OwnerObject=0**。舊程式要求 OwnerObject 等於母建築，126 筆全部在採集階段被拒絕。不是第一幀無效，也不是最後繪製的排序問題。

新採集以建築 ID、動畫 ID/類型、實際槽成員關係及讀取後重查保護身份。OwnerObject=0 直接使用 Location；真正 SetOwnerObject 附著才加父物件中心。`Anim::GetCenterCoords 0x422BE0` 調用 owner virtual `+0x48`；`Building::GetCenterCoords 0x447AC0` 為 Location 加 `(FoundationW*128-128, FoundationH*128-128, 0)`，不是一律加 Location 或渲染座標。

`Building::GetRenderCoords 0x459EF0`（thiscall(outCoord), ret4）為 Location X/Y 各減 128，即畫面 Y 減 15。身體與槽動畫需共用此錨點。Foundation enum 位於 type `+0xEF0`，原版 0..20 對應寬表 `0x8192B8`、高表 `0x819310`；custom foundation 仍由既有 INI 路徑處理，未驗證擴充附著中心。

`Anim::Draw_It 0x42303F..0x423061` 使用 `Animation.Value+0xAC + AnimType.Start+0x2B4`，包含暫停/缺電；`PausedAnimFrame+0x11C` 是更新狀態，不是另一個顯示幀。缺電 `+0x19E` 與 Paused `+0x11A` 不同。第 0 幀合法，不人工播放缺電油井。原指令重放涵蓋每份快照 15 個缺電動畫及 1 個非零 Start。

重放成功採集全部 126 筆。第二、三份各有 GACNST_A、GAPOWR_A 的 SHP 已載入，共 4 次實際解碼且可見；其餘 122 筆 SHPReference 尚未載入，此測試的磁碟 I/O 被替代，**不能聲稱 126 個全畫出了**。生產資產入口另測磁碟 SHP、六氣候油井/三陣營基地命名和幀選擇。

## 遮擋與深度

`Anim::GetYSort 0x422BC0` 返回 render X+Y 加 `+0x104`（leptons）；`GetLayer 0x424CB0` 對附著動畫返回 2，否則用 type `+0x364`。本次建築動畫均為地面層 2。改按 Layer/YSort 插入物件，不再全部留到最後；同錨點先身體後動畫，使前方單位能蓋住後方動畫。

建築繪圖片段 `0x43D6FF/0x43D787` 使用 BUILDNGZ、參考點 (198,446)、type `+0x1530/+0x1534` ZShapePointMove 與 foundation 遠角，寬度 >=8 跳過形狀。新入口優先讀已載入 BUILDNGZ 指標 `0x89DDBC` 及 `NormalZAdjust+0x1520`。這些是反組譯證據，未執行整個 Draw_It。

VXL 內部面 Z 只解決模型內遮擋。場景合成改用快取形狀的站立梯度與未加 padding 的模型底部，避免直接帶入與 TMP/BUILDNGZ 基準不相容的模型相對 Z。依據下列 CNCMaps VoxelDrawable，未宣稱完整重現原版 voxel raster。

## FA2 / CNCMaps 與發黑

具體對照版本與 primary sources：

- [EA Mission Editor 6abf0f5](https://github.com/electronicarts/CNC_TS_and_RA2_Mission_Editor/tree/6abf0f557469baea73079c6bf6550709e2e3584e)：`MissionEditorPackLib.cpp::RenderVXLSection` 算法線與光向點積，`MissionEditor/IsoView.cpp` 用 `(200+300*l/255)/255` 調整 RGB；這條路徑沒有 VPL。不能泛稱所有 FA2/FA2sp 都相同。
- [CNCMaps VxlRenderer.cs 56712d7](https://github.com/zzattack/ccmaps-net/blob/56712d7c9c5048afbfe9bddb01c0ec43f6fc008a/CNCMaps.Engine/Rendering/VxlRenderer.cs)：原始 normal table、朝向、漫反射和鏡面項決定 VOXELS.VPL 頁，先轉色盤索引，缺 VPL 才簡化回退。HVA 影響幾何，VPL 選頁不使用變形後法線；本次修正此差異，變形法線只用於回退光照。
- [Palette.cs](https://github.com/zzattack/ccmaps-net/blob/56712d7c9c5048afbfe9bddb01c0ec43f6fc008a/CNCMaps.Engine/Rendering/Palette.cs)：Ambient/Ground/Level、燈光色調、最大通道歸一化、63 檔亮度，物件色盤 240..254 自發光。工具已讀原版地格結果，不能再完整疊加一次高度/燈源。
- [ShpRenderer.cs](https://github.com/zzattack/ccmaps-net/blob/56712d7c9c5048afbfe9bddb01c0ec43f6fc008a/CNCMaps.Engine/Rendering/ShpRenderer.cs)、[VoxelDrawable.cs](https://github.com/zzattack/ccmaps-net/blob/56712d7c9c5048afbfe9bddb01c0ec43f6fc008a/CNCMaps.Engine/Drawables/VoxelDrawable.cs)：深度與陰影策略。工具此前全部陰影不寫 Z，重疊建築陰影會反覆除 2，確實有快速變黑的路徑。現建築陰影在身體後提交並寫地面深度；同深度嚴格拒絕，單位/動畫仍只測試。此項是上游源碼與受控像素測試證據，未捕捉使用者發黑畫面的逐像素對照。

另複核樣本 `0x5558E0`（ECX=scale 輸出、EDX=intensity 指標、3 個 RGB 指標棧參數，ret12）、`0x484180`、`0x483E30`、`0x484680`：前者限幅/歸一化 RGB 並輸出 16.16 倍率；Cell `+0x104`、`+0x10A/10C/10E` 為原版已計算結果。本輪僅反組譯，不把上游地址註釋當成已執行 ABI 證據。

快照建築 intensity=1056..1168；已載入物件/ISO 色盤峰值 252，不是未展開的 6 位值。實際光照重放的基地/電廠未全黑。因此不將全部 live RGB 乘 4，**也不宣稱查明了所有發黑情形**。新增每 5 秒受限診斷：類型、ambient、intensity、tint、來源像素峰值、VPL/回退狀態，用來區分缺資源、錯誤光照及過度陰影。

## 小地圖與驗證

舊 layout 先定位近方形框，再依比例縮短圖形，左下模式因此留下底部空隙。現在先擬合實際長寬，再定位/避讓；實景使用等距 2:1 投影比例。左下貼實際窗口左/下邊緣，保留既有小外邊距。自訂 X/Y 為佈局框左上角，新增水平左/中/右、垂直上/中/下，保存為 `Display.MinimapAlignX/Y`。

測試含寬/高圖貼底、九種自訂對齊、10368 種既有佈局組合、實際控制項及 INI 保存/重啟。保留先發布地形、載入中保留完整實景、公平解碼、隊形 124/255 透明度。未宣稱 Windows 遊戲實景、所有 MOD、完整瞬時動畫陣列或多人驗證已完成。
