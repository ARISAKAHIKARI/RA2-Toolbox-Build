# 動態 VXL／SHP、素材準備與回退後繪製效能

基線 `1e6e25d4b6d7d6185c3d87e94942deff6d555c1d`。本輪針對持續轉向、變換 HVA／砲塔／動畫幀的正式繪製函式；上一輪的靜態整幀重用不能充當動態素材核心加速的證據。

問題是重複計算與重複準備，沒有新增遊戲地址、呼叫 ABI、模擬資料寫入或渲染 Hook。測試使用可再分發的合成素材，未操作使用者正在執行的遊戲。Library 的持久 SQLite 筆記本仍不可存取，倉庫先保存研究與證據，供之後寫回。

## 原始參考與邊界

- [YRpp 固定版本](https://github.com/CnCNet/YRpp/tree/d9795546950fcc75a94325ffc83b44d16ec34ce2)：Drawing 的姿態快取、ObjectType 的四組 voxel cache、Tactical 的可見區域，以及 ConvertClass 分類的原生 blitter。既有 HF30 已做 1× indexed SHP 路徑與完整 A lookup；本輪不把它們重算成新成果。詳見既有 `2026-10-31-yrpp-native-rendering-performance-reference.md`。
- [VERA20K sprite_atlas.rs](https://github.com/YuriPlanet/vera20k/blob/3d42426b2e9c1b4cf913a7dbbaaf8bf7d3b6bc7f/src/render/sprite_atlas.rs)：977–988 的實體幀去重與 1015 的 source digest/frame 身分。工具箱只參考不可變來源去重的設計，未搬入 Rust 引擎。
- VERA20K 同版本的 batch.rs 與 atlas_growth.rs 使用持久 buffer、按需增長、增量上傳；本專案已有不可變 atlas／髒區與同批 job 去重。本輪新增的是最終裁切範圍為空時，在素材准入前省去無效準備。
- 上游程式與受控 fixture 不代表私人遊戲 EXE 或 RA2OL 的實測；既有私有 EXE 指紋與原生地址結論維持原研究，本輪不重新推導。

## 修正

1. **VXL HVA 與光照。** 相同 section／HVA 矩陣不再為每個面重新求逆；法線光照只求本次姿態實際用到的索引；已映射 VPL 色值的 premultiply 在單次 render 內重用。結果不跨姿態猜測，原生浮點與頁選擇保持。
2. **VXL CPU 像素。** SSE2 同時計算四個獨立像素中心，保留逐項運算順序、完整除法、coverage 容差和嚴格大於的 Z winner；不使用近似倒數或增量插值。x86／x64 與獨立 scalar 參照逐位比較。
3. **縮放 SHP。** 每次 blit 的來源 X 映射與不依賴 per-pixel depth 的列深度不再逐像素重算。保留實際除法／取整、native Z/A、遮罩、陰影、透明度及附件順序；1× 原快路徑不以縮放測試推算收益。
4. **相同瞬態 SHP。** 已完成不可變 capture、沒有 VXL／砲塔的 animation／particle／projectile，原先只因不同 object/id 就重複解碼。共享 key 改按完整素材與姿態；仍包含 process/match、來源雙重指紋、完整 palette、type/owner/parent、pointer、姿態與深度設定。每物件的 capture／存活驗證沒有刪除；live-pointer、不完整或混合素材保留原行為。
5. **GPU VPL 衍生表。** 每 presentation 仍雙讀原生 VPL、核對頁數與內容。只有新鮮捕獲成功且 signature 不變，才跳過相同 256×256 GPU 表重建；失讀／競態仍使當次光照不可用，並非使用舊光照冒充新結果。
6. **GPU 裁切前准入。** 每個 packet 先保留完整合法性驗證，再計算既有 drawBounds 與最終 frame clip；空範圍不安排 VXL/xBRZ、atlas 來源、Float64 要求或 cloak 串行 pass。原始 draw ID／順序不變，未以迷霧或估計遮擋刪除有覆蓋的物件。

## 量測與驗證

[實際編譯、測試、前後對照與產物雜湊](evidence/actor-render-performance-build.json) 保存本輪命令和結果。計時為受控 CPU 工作量，不能換算遊戲 FPS 或 GPU 實際時間。

- `voxel_dynamic_cost.cpp`／`voxel_kernel_equivalence.cpp`：96 個變換姿態、每單位 2,382 個面。前後交錯跑五組的 HVA 10.027→7.349 ms、GPU 輸入準備 29.424→27.577 ms、CPU body 40.974→34.932 ms、shadow 33.447→30.100 ms，digest 相同；另驗證三角形邊界、equal-Z、HVA 奇異／交錯和 x86／x64。上述是每批 96 姿態的成本，不是每個單位耗時。
- `shp_raster_kernel.cpp/.py`：正式舊／新 Surface 在同一程序對照，12,864 次有序繪製，RGBA、float Z 位元、native Z、A 全等；160 個動態 SHP 的 0.75× 與 1.5× nearest 初次中位數約下降 32%／38%，1.5× smooth 約 17%。1× 無改動、只有計時波動。
- `actor_material_reuse.cpp/.py`：96 組動畫／粒子／投射物、body＋shadow＋intrinsic alpha 與直接 decode 相同；完整 source、palette、owner、frame、epoch、失讀／競態分離。128 個同類動態實例、五組各八幀，在已完成 capture/fingerprint 後的素材準備階段，實際 build 5,120→40，初次量測 3.316→0.069 ms/frame。它不包含進程讀取、最終 framebuffer 或完全不同素材的場景成本。
- 同測試的 fresh VPL capture＋GPU 表初次中位數 25.459→9.283 μs/presentation；每次五次原生讀取仍在，signature 變化／失讀／雙讀競態均檢查。
- `gpu_scene_admission.cpp`：900 個獨立 packet（SHP/xBRZ/VXL 各 300）、90 個與最終 clip 有交集，VXL 與 xBRZ 各 300→30 jobs、來源規劃位元組 9,830,400→983,040，CPU 規劃約 0.158→0.030 ms/frame。2,000 組 fractional/scale/clip 覆蓋與舊 bounds∩shader clip 相同。沒有執行 GPU、沒有 GPU FPS 結論。

計時不設脆弱的速度斷言；資料與像素等價、實際 build/job 次數才是正確性依據。完整建置重跑可能受系統負載影響，以 evidence 原始結果為準。

## 尚存成本

後續檢查既有使用者安裝的三份 Runtime 日誌，Host/DLL 雜湊與基線 `1e6e25d` 一致；只讀日誌與檔案，沒有操作遊戲。219 個五秒詳細抽樣中，127 次沒有產出畫面；其中有一段約十分鐘的連續失敗抽樣。已記錄 terrain／palette／decode，voxel／actors 尚未記錄，能把有 decode 耗時者收斂到 live-VXL 的提前返回，但原日誌缺少原因碼，不能斷言哪個條件失敗。

新診斷在同一五秒記錄窗補上 palette／VXL 的具體失敗分支、物件身分、姿態、新鮮光照、倍率與素材預算；不跳過常駐單位、不放寬有效性要求。原先 `gpu=0` 在失敗畫面上只代表尚未成功使用 GPU，不能當成使用者選擇 CPU。9060×2130 世界對 906×213 小地圖的倍率核對為 1，沒有找到誤用主畫面高倍率的證據。

地形階段重複 converter metadata 和無用 Paint 準備另見[地形對照](2026-11-01-terrain-presentation-read-cost.md)；動態物件首次欄位批讀另見[調色捕獲對照](2026-11-01-palette-read-batching.md)。後者在 800 個合成動態物件、包含建築附屬動畫的正式路徑，真正 RPM 55,201→25,601，palette payload 仍雙讀 1,600 次；初次最終配對中位數 13.891→7.377 ms。這些數字不能相加或外推為 RA2OL FPS；素材預載本身不會消除逐幀的姿態、身分、光照核對、像素合成與呈現成本。

另外確證並修復[舊新 VXL 姿態快取自鎖](2026-11-01-pose-cache-transition-liveness.md)：同一新畫面需 18 Mi pixels，低於 32 Mi 上限，但舊、新合計 36 Mi，舊實作連續 64 次無法發布；新的壓力回收維持同圖逐像素結果、SHP／暫態回退與已發布畫面。這是已重現的產品錯誤，仍不能倒推舊實機日誌一定命中此條件。

完整 world snapshot 先準備素材再分給多個視圖仍會產生未落入單一相機的 CPU 成本。它同時保障相機移動、三個副相機與大型物件的完整性；本輪沒有用任意距離裁掉物件。原生遊戲世界繪製、GPU 回讀與 DWM 提交仍有成本，未聲稱它們已消失。

回退前後排程與 CPU 並行詳見 [歷史與回放對照](2026-11-01-shp-kernel-and-cpu-replay.md)。局中首次啟用另查出 Host needsBridge 漏列部分功能需求；這是外部取樣暫未就緒時可確定的啟動缺口，不等於已證明 RA2OL 全部失效的根因。雙機同步、實際顯卡與遊戲內視覺仍需實機驗證。
