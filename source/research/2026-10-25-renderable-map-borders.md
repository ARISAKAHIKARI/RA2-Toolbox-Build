# pre.6：可繪製邊緣與攝像機限位分開處理

## 問題與先前推論的更正

使用者指出：原版仍繪製活動邊界周圍的地形，坦克不能越過玩法邊界，攝像機也不能無限向上移動；這種場景應讓受限攝像機中的跟蹤單位偏離中心，而不是把邊界以上全部清黑。使用者提供的小窗／原版兩張對照是實機觀察，不假設它們與私人 capture 為同一對局。

調查前檢索 `INDEX.md`、pre.5 地圖邊界筆記（持久 SQLite） 與同一持久 SQLite v40 的 MapRect／VisibleRect／`568300`／`578540` 紀錄。pre.5 的 31,640 組原指令對照證明了兩個 predicate 的公式，**沒有證明較窄的 usable predicate 應用於地形繪圖**。當時進一步把它當作畫面遮罩，導致合法邊緣地形被丟棄；本次撤回這個推論及對應裁切。

支援的原版映像 SHA-256：`7b8a068535d6af06845edf95ae829b113d00c02909330e16f197426cd7db94b6`。原指令取自既有私人 capture 的 `02-on` 有效區段；不公開 EXE、記憶體、遊戲素材或使用者截圖。

## 三個不同用途

| 範圍／限制 | 原版依據 | 本工具用途 |
| --- | --- | --- |
| 實體地圖合法格 | `MapClass::CoordinatesLegal`，`0x568300`，依 MapRect 寬高判斷 | 地形取樣的實體邊界；另保留索引、指標、座標身分及 shroud 檢查 |
| 玩法可用區 | `IsWithinUsableArea(CellClass*,true)`，`0x578540`，依 live VisibleRect、格高及坡面判斷 | 保留 predicate 作獨立用途和原版對照，**不當作 Tactical 繪圖遮罩**；本次不修改遊戲尋路或單位移動 |
| 攝像機中心可移動區 | `0x6D8640`，由 live VisibleRect 和實際 viewport 大小導出 | 先限制相機位置，再按相機取樣與繪製；目標到邊時可以偏離窗口中心 |

VisibleRect 是原版欄位名稱，不表示其每一條邊可以直接套成螢幕像素的透明／黑色遮罩。可繪製的實體地形仍由目前相機的 viewport 裁切。

## 原版繪圖入口證據

`Tactical` 的地形繪圖選擇區段為 `0x6D521C..0x6D526A`：

1. `0x6D5226` 以 Map `0x87F7E8` 作 ECX，呼叫 `0x568300`，參數為 cell 座標指標，AL 回傳布林、callee 清理 4 bytes。
2. 非法座標跳至 `0x6D526A`；合法座標於 `0x6D5239` 呼叫 `0x5657A0` 查 cell。
3. `0x6D524F` 以 cell 作 ECX，呼叫 `0x47EC90`，傳入螢幕點、viewport rect `0x886FA0` 及繪圖旗標。

這個入口沒有呼叫 `0x578540`／`0x578460`。`assist/tests/map_render_bounds_native.py` 執行原始 gate 和 `CoordinatesLegal` bytes，僅把 cell lookup 與最後 draw 呼叫替換為記錄邊界。另獨立執行原始 usable predicate 分類同一輸入，沒有用它替代繪圖條件。

四種全圖／可用區矩形、各不等式鄰邊、隨機座標、不同高度及坡面，共 **7,932 組**：

- **7,110 組**實體非法，未呼叫 draw。
- **458 組**實體合法但 usable 為 false，仍呼叫 draw。
- **364 組**實體合法且 usable 為 true，呼叫 draw。

所有結果與實際編譯的 production predicate 一致，零差異。這項證據證明繪圖准入條件，沒有執行最後的完整地形像素繪製或整個遊戲可見格註冊流程。

## 原版相機限制與奇數視口

`0x6D8640..0x6D86FE` 讀 Map 的 live 矩形及 viewport 寬／高 `0x886FA8/+4`，參數為 stack 上的 Point 指標，原地改寫座標，`ret 4`。保留原有相機範圍常數及高建築上緣空間。

本次另確認原版以整數除以 2 計算半視口：奇數尺寸時左／上使用截斷後的一半，右／下取得剩餘像素。`map_bounds.h::clampCamera()` 已對齊此規則；640×512 小窗的原有值不變。

受控原指令與編譯 production helper 共 **1,458 組**相機座標零差異，其中 **567 組**為奇數 viewport。地圖小於 viewport 時採用中點的 **12 組**測試單獨記錄，這是工具為避免原版交替夾到兩端而採用的策略，**不聲稱與原版該缺陷等價**。原指令區段 SHA、計數及界限存於 `verification/map-render-bounds-native.json`。

## 修復與實際畫布回歸

- `watch_view.h`、`radar_map.h` 移除 usable 篩選，保留 `legal`、cell 身分、容量及 shroud／reveal 規則。物件和動畫仍依賴已接受的 cell。
- `map_bounds.h` 的 `Screen` 改名為 `CameraExtent`，刪除像素 `clip()`，避免再次混用兩種語義。`watch_engine.h` 和 `radar_scene.h` 不再依相機限位清除畫布；Surface 仍裁至實際 viewport。
- 小窗取樣中心仍反算自受限相機位置；focus cell 獨立記錄，保持正常 refresh 與目標生命週期。
- live bounds、原生 radar offset／factor／尺寸仍參與快取身分；採集完成的 fresh bounds 驗證保留。觸發在同一 frame、同一 cell array 下改界仍拒絕舊結果。

`watch_host.cpp` 以 production `sample()` 接到 production `drawView()`／Surface，建立 `{131,146,3,5,124,135}` 地圖與 `(69,73)` 的跟蹤單位。相機 Y 固定於 **2296**；320×256 小窗的 focus ring 中心為 **(160,52.5)**，正常偏向上方。上方 40 行、中央 256 像素的 **10,240 個像素**均有邊緣地形，沒有舊篩選造成的額外背景黑帶。這是受控記憶體與資產邊界替身，實際 Surface 合成有執行，GDI 畫面呈現沒有冒充為實機觀察。

`watch_view.cpp` 另涵蓋同 frame 縮／擴界、實體非法即使 reveal 也不顯示、受限角落完整取樣。`minimap_overlay.cpp` 涵蓋 usable 為 false 的合法 border cell、投影 offset／高度改變後舊 bank 淘汰與新像素位置，以及恢復原邊界。`map_bounds.cpp` 涵蓋奇偶視口、小圖中點與混合矩形拒絕。`radar_scene.cpp` 保留背景失效及已解碼資產重用驗證。

新 watch／minimap 邊緣斷言在 pre.5 舊篩選下失敗，修復後通過。獨立唯讀審查確認原有 shroud、動態邊界失效及相機限位仍存在。

## 排除與未決事項

不是提高相機位置上限、強制目標永遠居中、忽略劇情觸發，亦不需要修改單位可走區域。這次修正的是取樣／繪圖與相機限位的職責；**沒有調整相機角度、朝向、固定 2:1 投影或光源方向**。

完整 Windows 遊戲中的同地點、全部斜坡與 MOD 特殊地圖、FPS 及雙機聯機尚未完成新版本實機驗收。原版 gate replay、相機座標對照和本工具 canvas 測試各自的證據範圍分開保留。


## pre.12 Radar 適用範圍更正
上述 0x6D5226 CoordinatesLegal 是 Tactical 地形入口。RadarClass::SetVisibleRect 0x654490 使用 mode1 usable 決定 raw 矩形；初始 FillTerrainColors 0x654EA0 逐個 physical Cell 作 flat 2x1/raw 矩形交集，增量更新 0x655250 才逐格 usable。MapClass::SetVisibleRect 0x567230 在交集後把 live Rect 強制向內保留左2、上2、右2、南6；外部已讀到規範化後的 Rect，不可二次內縮。詳見 repo 2026-10-25-minimap-bottom-pixels-pre12.md 與受控測試 native_visible_inset.py。
