# SHP 內核與 CPU 回放：歷史對照及受控量測

基線 `1e6e25d4b6d7d6185c3d87e94942deff6d555c1d`。本輪只改工具箱外部畫面生成，不改遊戲程式碼、邏輯幀率、Hook 或命令。量測是本機合成輸入，不是遊戲／RA2OL FPS。

## 「Vera 整合前較快、回退後變慢」的可確認差異

- [整合前效能 checkpoint 7593a400](https://github.com/ARISAKAHIKARI/RA2-Toolbox-Build/commit/7593a400689f8f266295d36122f7f87c8a39ebea) 加入 PoseTransform、一次投影、immutable atlas 和完整輸入比較。這些機制在目前源碼仍存在，不能說成全部被回退。
- [HF18 23a7e5a](https://github.com/ARISAKAHIKARI/RA2-Toolbox-Build/commit/23a7e5acb737caea83970e95d3726545928b95d4) 加入 Vulkan／D3D11 共享紋理與 DirectComposition 直出。
- [HF29 60fc8a3](https://github.com/ARISAKAHIKARI/RA2-Toolbox-Build/commit/60fc8a3411a4d3ec68c75cabc1486a2a0257f9b5) 確實撤去第二 presentationWorker、HF28 的 CPU 輸出行分片、共享紋理直出；保留 GPU 計算，但最終像素經回讀與既有合成器。這可能降低部分場景吞吐，但沒有證據能把當時所有卡頓歸因於單一撤回。
- 7593a400、HF18 與現在的地形 `min(60, informationFPS)`、觀測 1000 Hz、繪製 `informationFPS` 目標仍一致。未發現可以直接恢復的遺失 cadence。第二工作執行緒的撤回是資源交接架構變更，不能僅恢復其呼叫就視為安全修正。
- 本分支沒有實作停止原生世界繪製；[原生繪製階段研究](2026-10-29-native-render-suppression-feasibility.md) 也明確記載未實作。外部顯示吞吐與遊戲模擬速度是不同的量測。

## SHP 內核

Hotfix30 已有 1× indexed 專用內核與完整 A 查表，本輪不重複宣稱。`camera_raster.h` 把縮放繪製的橫向 source index 按原 float 除法／floor 每列共用，並把同一列的原生 Z walk 與非逐像素深度算式提到列外。保留 signed native candidate 比較、float 運算順序、透明 coverage、材質先後與目的像素語義。

參考：[固定 YRpp RLE blitter](https://github.com/CnCNet/YRpp/blob/d9795546950fcc75a94325ffc83b44d16ec34ce2/Blitters/RLEBlitTransXlatAlphaZReadWrite.h)、[固定 Vera native Z](https://github.com/YuriPlanet/vera20k/blob/3d42426b2e9c1b4cf913a7dbbaaf8bf7d3b6bc7f/src/render/native_z.rs)。這些是源碼參考；前後相等不等於已用真實遊戲建立視覺 oracle。

`shp_raster_kernel.cpp/.py` 將保存的原始 Surface 與目前 Surface 同時編譯，12,864 次有序繪製逐次比較完整 RGBA、float-Z 位元、u16-Z 與 A。包括兩種精度、全部 depth gradients、native shape／coverage、NaN、材質／位移、附件、橋面／裁切、非對稱縮放、rasterScale 1/2/4 與 signed native 深度邊界。舊摘要測試曾修改 material 卻未傳入 blit，本測試實際傳入。

動態基準經正式 SHP RLE decoder 產生八個不同姿態；每幀重新提交 160 個單位並改變姿態、位置、光照。計時僅 blit，排除初始化與解碼；不以整幀重用充作內核提升。五輪各 12 幀，前後順序交替，取中位數。

| 1280×720／160 個 SHP | 原內核 | 新內核 | 變化 |
| --- | ---: | ---: | ---: |
| 1× | 1.872 ms | 1.886 ms | 約 +0.8%，原快路徑未改，無提升主張 |
| 0.75× nearest | 3.479 ms | 2.443 ms | -29.8% |
| 1.5× nearest | 13.770 ms | 8.760 ms | -36.4% |
| 1.5× smooth／xBRZ | 27.206 ms | 21.816 ms | -19.8% |

## CPU 回放的安全分區

新 `cpu_replay_rows.h` 與 `GpuSceneRecord::replay` 不復原舊版 band 複製／座標平移。它們使用同一完整 Surface，僅把寫入行範圍分開；各行仍依原有全部 draw 次序處理。元資料、底圖與 A 在開工前完成，RGBA／float-Z／native-Z 不相交寫入；座標 scratch 為各 worker 私有。

`Surface::blit` 唯一跨目的像素讀取路徑是 `displaced()`，兩個入口均由 `material.displace` 控制；此 call 及其附件整體作為串行屏障。普通透明、packed blend、shadow、mixStrength、translucency 只讀自己正在寫的目的像素，可按行並行。任何附件需要 xBRZ reconstruction 或 deferred GPU materialization 時，也保留整個 call 在 owner 串行執行，避免在每個分區重複準備與修改共享 cache／failure flag。

使用既有有界 persistent row pool，無每幀建立執行緒。少於八個 call、估計少於 262,144 候選像素或過低的畫面保持串行；同時另一個 renderer 佔用 pool 時直接串行，不排隊。worker 配置失敗回傳失敗，不發布部分完成圖。

`cpu_replay_rows.cpp/.py` 直接編譯保存的原版 GpuSceneRecord 與新版，先做 48 組完整四平面對照，另有雙 renderer 同時執行；涵蓋跨行正／負 cloak 位移、packed／半透明／陰影／附件、橋面裁切、A effects、逐像素 NaN、GPU deferred 成功與失敗後重試。六個動態量測配置另逐幀完整比對。計時包含底圖初始化、A、完整回放及屏障，排除 build draw list。七輪不同姿態／光照，暖場一輪後取中位數。

| 1280×720 完整回放 | 原串行 | 新回放 | 變化 |
| --- | ---: | ---: | ---: |
| 3 個物件／普通 | 2.067 ms | 1.985 ms | 低負荷串行，無穩定收益主張 |
| 3 個物件／屏障與混色 | 2.698 ms | 2.769 ms | 低負荷串行，無穩定收益主張 |
| 160 個物件／普通 | 10.585 ms | 3.713 ms | -64.9% |
| 160 個物件／屏障與混色 | 13.848 ms | 6.993 ms | -49.5% |
| 400 個物件／普通 | 20.257 ms | 5.663 ms | -72.0% |
| 400 個物件／屏障與混色 | 25.287 ms | 11.458 ms | -54.7% |

兩個基準的百分比不能相加；CPU 回放對照兩邊均使用本輪優化後的 Surface，隔離回放分區收益。仍串行的 xBRZ／GPU deferred／位移密集場景收益會較小。沒有重新啟用 DirectPresentationEnabled，沒有真實顯卡、完整遊戲或聯機驗證。

本機原始 evidence 位於 `work/ra2-hook-audit/diagnostics/shp-kernel/with-row-entry`、`work/ra2-hook-audit/diagnostics/cpu-replay-rows/current`。SHP 保存基線 SHA256 `498a54822d7a7cba08c86172bc27f65cec0131526be8a116c43fb4c573302d72`；回放保存基線 `d079e5e7214de8fd8534c7a2b1d91f3c6a83c176a8d0d50641eea0e72db9bed9`。fixture 需 Zig 0.13.0、正式 xBRZ 編譯單元；runner 接受 `--zig`、`--build-dir`、`--baseline-raster` 或 `--baseline-record`，build/test log 與可執行檔寫到指定目錄。
