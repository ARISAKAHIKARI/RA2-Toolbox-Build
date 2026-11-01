# 全鏈路顯示效能：讀取、排程、場景計畫與視窗提交

基線 `1e1568155385d13f225a7c99877dd7bbd535868b`。使用者要求是整條顯示鏈路的效能，不只是小地圖縮小；副相機適當多繪製可接受。本輪保持既有視野、面板需求、選定刷新率、畫面內容與 Hook／命令行為，處理重複讀取、重複準備、重複像素工作和鎖等待。

本輪是工具箱 Host 的生產源碼與受控 Windows／記憶體／像素測試，不新增遊戲地址、呼叫 ABI 或逆向結論。持久 Library 筆記本無工具存取，研究索引與證據先保留在倉庫。沒有啟動、終止或注入正在使用的遊戲。

## 各段發現與修正

1. **資料讀取。** 小於一頁但跨 4 KiB 邊界的物件塊曾直接繞過已有頁快取。現在讀完整的兩頁快取；任一頁不可用仍按原始請求範圍 fallback，不把快取失敗當有效零資料。批讀先去重再套頁數上限，並重用暫存容量。Cell 掃描仍按原有地圖准入、完整性與 epoch，先批讀本 chunk 所需的 0x148 位元組區間，不降掃描頻率、不跳過資料。
2. **高頻觀測。** 主場景採樣每次先讀完整相機／視窗，再判定同一遊戲幀、未過 50 ms 而不採樣。新增單次 fresh 幀號 early probe；只提前做原本的拒絕，變幀、回退、失讀、50 ms 到期仍走完整世界與相機驗證。它不授權發布，也不改原先 50 ms 策略。
3. **執行緒交接。** SceneBank、Latest、Recurring 和小地圖發布曾持鎖析構舊大快照，阻塞取圖或提交。退休指標移到鎖外；新 epoch 拒絕舊完成、lease、停止語義維持。小地圖有完整實景後，不再額外複製一次完整 actor snapshot 只為中途重新發布舊圖；初次載入仍立即顯示 diagram。
4. **地形計畫。** 靜態 TMP 已不變，ViewportCanvas 仍反覆遍歷、flatten、生成深度 walk 與配置每條 draw。保留不可變 COW 地形命令前綴；輸出、crop、光照、素材、遮罩、精度和 owner 身分變化依正式 key 失效，動態尾段每次重新生成。失敗尾段不污染下一輪。
5. **CPU 最終合成。** 小地圖完整 draw plan 相同仍重新計算所有輸出像素。新增保守的完整 plan 重用；仍逐次讀取 revision、光照與色盤，有未被 plan 完整描述的 CPU 特效／裝飾則走完整路徑。另補 terrain lighting signature 缺少的 palette table／rows／scalar／rgb555，避免亮度相同但 converter 色表變更時錯用舊色。
6. **副相機。** 舊 Result 用 request tick 當圖像身分，使同一完成圖在曝光／移動時重複 GDI+ 縮放。現在按不可變 pixels/GPU owner、尺寸、序號與邊框狀態決定是否重畫；成功才 commit，失敗保留前圖並重試。無手動跟蹤且自動跟蹤未開或已抑制時，直接發布空結果，不先掃描全圖再發現沒有目標。原有 640×512 邏輯視野與主副面板需求保持。
7. **最終呈現。** 主覆蓋層只要有 RenderUpgrade，就以 continuous 強制反覆合成／整窗提交。改為新 revision、實際畫面／尺寸／可見性／恢復需求才提交，同圖每秒保活；失敗和 GPU pending 仍重試。維持最長 8 ms 視窗／前台／lease 檢查，新圖立即喚醒並遵守原輸出率，閒置不推進提交時鐘。

## 量測與驗證方式

[完整建置、回歸與產物雜湊](evidence/pipeline-performance-build.json) 記錄實際執行命令及輸出。以下為受控樣本，不是 RA2OL 或真實顯卡 FPS：

- 正式 Sampler 的 128×128 地圖、32640 個有效 Cell、8 次 full fresh sweep：22,728 → 2,592 次 RPM，首次量測 65.523 → 59.618 ms。資料、逐 Cell 身分、迷霧和變更內容一致；讀取呼叫少 88.6% 不等於 FPS 增加九倍。
- 一萬個靜態 TMP 命令 prepare + 完整 plan 比較：同參數對照 4.024 → 0.238 ms；像素、深度、遮罩、動態尾段及失效案例對照。
- 600×300 的正式 SceneRenderer：相同場景 120 次，同參數對照平均 0.487 → 0.141 ms，新增 120 次 CPU resolve 重用；同幀環境光／原始色盤變化仍更新。原生 converter 表格紅→綠的反例是在遊戲幀前進後驗證；既有 converter 同幀快取未改，不能宣稱同幀修改表格也即時探知。
- 320×256 副相機 240 次同圖曝光：集中重跑的重縮放 82.731 ms，保留前圖只 BitBlt 1.656 ms；新 raster 0。
- 實際 Worker 測試覆蓋 reentrant destructor、invalidate／shutdown／expiry、晚完成舊 epoch、原小地圖 deadline 和 21 次鎖外退休。

集中測試曾發現 fixture 預留 0x700000..0xBFFFFF 與 ASLR 後的測試 EXE 0xBA0000 重疊。只修改測試：查詢可用低位址 arena，將測試 global 地址映射到保持頁內偏移的專用記憶體；保留全部資料／呼叫數斷言，未修改產品 ASLR。原始失敗記錄也保存在 evidence。

完整編譯與最終重跑的量測可能因系統負載不同；以 evidence 各測試紀錄為準。所有功能正確性斷言依資料／像素／實際呼叫數，不依不穩定的耗時門檻。

## 後續實機定位

自動 `logs/Runtime.log` 每五秒加入分段統計：information-sample、host-control、host-paint、world-observation、world-render、minimap-worker、watch render/UI 及 compositor。提供次數、平均、最大值，區分採樣、準備、CPU 合成、DIB copy、GPU present／ULW 系統提交；不逐幀寫磁碟，不把等待下一幀的時間當計算耗時。

共享的是唯讀 ProcessConnection 與不可變場景，頁快取仍由各讀取任務按自己的 epoch 管理，避免混入其他時間點的資料。全圖初始化與遍歷、GPU 驅動／回讀、DWM 和遊戲自己的原生繪製仍可能耗時；本輪不宣稱這些成本已消失，也不宣稱雙機不同步已修復。原版遊戲的世界繪製並未被關閉。
