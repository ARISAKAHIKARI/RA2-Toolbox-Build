# 共用渲染效能：pre.9 → pre.10

基準為 `dde2f56b3c663f3355d1c7ca0c436f5a2ba8ee58` 加攻擊腳下兩個提交，本機固定快照 `be2d15b`。本輪修改外部讀取／渲染器，不新增遊戲寫入、Hook、硬體槽、引擎地址或原生 ABI 推論；未調查新的遊戲 PE。礦車自動點殺蜘蛛仍待做。

## 現場證據與問題

使用者要求小地圖、副相機、隊形／移動規劃預覽與渲染升級共用程式並改善 CPU／GPU。現有程式本已共用 SceneRenderer、CameraAssets、ViewportCanvas、Vulkan backend；副相機三視口也已共享 prepared world 和排序。因此沒有再建立另一套繪製器。

現有 pre.9 日誌的 504×284 小地圖、5040×2835 原生範圍、約 44–75 個物件，多次完整更新為 466–591ms；15 筆指定時段樣本的中位為 499.07ms。terrain 階段包含光照／整圖驗證、mask 與 ground/frozen extras；palette 階段包括物件素材驗證，不是純像素繪製。單筆有 365824 次 render RPM，而 visibility 的 1540 次 RPM 約 4.45ms。故不能把這個問題歸因為 GPU 繪製全部過慢。

## 修改與依據

1. **失敗的 Live INI 索引重建風暴。** 原程式在尾部 section 名失讀、null 或重名時，不保存失敗 bootstrap 狀態。後續不同素材查詢會反覆遍歷整張表。4096 sections／45 查詢的實際 Win32 自進程 fixture，尾部失讀為 368910→8286 RPM，最終並行負載量測為 179.497→4.060ms；正常 missing-section 為 8287→8287 RPM。新模式由 CameraAssets.beginPresentation 明確啟用；每 query 仍 fresh 驗 root/header，身份、header、世代改變立即失效，下一 presentation 立即重試。其他 EffectiveINI 使用者保留既有行為。523 個 active assertions 包含真正 Context 的 art/rules forwarding、empty/live/disk fallback 及恢復。
2. **共用單次呈現的物件光照。** ObjectLightContext 在同次呈現共用實際 converter 的 tint／metadata 與三類 Rules extra light，仍逐物件讀取 converter/cell/橋面及強度。接入隊形和 SceneRenderer；23,552 個 State 與凍結原函式精確相等。1024 actors 的實際 RPM 38400→23853；最終 20 輪交錯約 10.390→6.539ms。不是遊戲 FPS。
3. **CPU 完成前段直接恢復。** 精確 prefix key 命中時，直接恢復 RGBA／Z／nativeZ／A，省掉隨後會被覆蓋的底圖初始化與 mask/alpha 重播。600×300、10000 TMP＋移動物件，三輪約 0.258–0.303→0.172–0.197ms，四平面相等。另修 re-retain prefix 世代未失效的既有缺口；舊負控制漏掉新增繪製，新版等於完整冷回放，普通 warm begin 仍命中。
4. **批次素材失效。** palette/VPL 改變的 key 先聚合，再各掃 sprites/displayed 一次，避免每 key 重掃整個 cache。256 變動素材的完整 Renderer 回歸約 1.06–1.13→0.55–0.59ms；32 次 RGBA／coverage、失讀與恢復均相同。
5. **GPU 分箱及 metadata 容量重用。** 移植先前本機已有量測但不在此 pre.9 的獨立修正：8/16/32 adaptive CSR bins、重用 metadata 容量，保留 pre.9 新預算與 storage-range 檢查；沒有整份覆蓋舊 header。真 RTX 三輪最終小圖約 3.315→2.234ms、密集小圖 10.523→7.246ms、FP64 小圖 2.312→1.599ms。小負載並非全面提升：zoomout 0.730→0.786ms，三視口合計 1.515→1.602ms；完整結果保留。16 幅 odd/even 畫面跨變體／輪次逐 byte 相等。
6. **GPU 索引邊界。** reference 低 16 位是 draw index，因此 GPU 明確限制 65536；65537 在提交前退到完整 CPU，不截斷、不 alias。真 GPU 65536 與 CPU 65537 最後一筆綠像素通過；x86/x64 各 148855 項 CSR painter-order 檢查通過。

## 驗證與尚未確認

CPU replay 48 組全平面、並行／失敗恢復／alpha；SceneRenderer 120/120 重用及 same-frame light/palette 失效；上述新增 fixtures 均通過。GPU/CPU 時間包含各報告明示的範圍，不能相加換算整局提速。對齊頁雜湊碰撞也做過查表反例，但本 ProcessReader 的正常自動 bucket 量測未顯示大成本，沒有套用無收益的雜湊改寫。

尚未用同一遊戲場景證明 Live INI 就是現場半秒耗時的唯一原因，也沒有實際遊戲前後 FPS／聯機驗收。新診斷拆出 setup／terrain scan／mask／extras RPM、各繪製階段 RPM、素材失效次數、palette 命中及 INI index build/failure/reuse，供下一次同場景核對。完整 source-bound 報告隨更新包 diagnostics 提供；沒有發布遊戲 EXE、原始記憶體或私有 dump。
